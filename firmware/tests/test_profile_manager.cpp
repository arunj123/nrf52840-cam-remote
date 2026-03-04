#include <gtest/gtest.h>
#include "profile_manager.hpp"
#include <map>
#include <array>
#include <string>
#include <optional>
#include <string_view>
#include <cstring>

// ─── Mock StorageHal ────────────────────────────────────────────
struct MockStorageState {
    uint8_t stored_value = 0;
    bool has_stored = false;
    int store_count = 0;
    int load_count = 0;

    std::map<std::string, std::array<uint8_t, 7>> addr_storage;

    void reset() {
        stored_value = 0;
        has_stored = false;
        store_count = 0;
        load_count = 0;
        addr_storage.clear();
    }
};

static MockStorageState storage;

struct MockStorageHal {
    static bool store(std::string_view key, uint8_t value) {
        storage.stored_value = value;
        storage.has_stored = true;
        storage.store_count++;
        return true;
    }
    static std::optional<uint8_t> load(std::string_view /*key*/) {
        storage.load_count++;
        if (storage.has_stored) {
            return storage.stored_value;
        }
        return std::nullopt;
    }

    static bool store_addr(std::string_view key, const uint8_t* addr) {
        std::array<uint8_t, 7> a;
        for (int i = 0; i < 7; ++i) a[i] = addr[i];
        storage.addr_storage[std::string(key)] = a;
        return true;
    }

    static std::optional<std::array<uint8_t, 7>> load_addr(std::string_view key) {
        auto it = storage.addr_storage.find(std::string(key));
        if (it != storage.addr_storage.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

using TestProfileManager = remote::ProfileManager<MockStorageHal, 3>;

class ProfileManagerTest : public ::testing::Test {
protected:
    TestProfileManager mgr;
    void SetUp() override {
        storage.reset();
    }
};

// ─── Basic slot cycling ─────────────────────────────────────────

TEST_F(ProfileManagerTest, DefaultsToSlot0) {
    mgr.init();
    EXPECT_EQ(mgr.active_slot(), 0);
}

TEST_F(ProfileManagerTest, CycleThroughAllSlots) {
    mgr.init();
    EXPECT_EQ(mgr.cycle(), 1);
    EXPECT_EQ(mgr.cycle(), 2);
    EXPECT_EQ(mgr.cycle(), 0);  // wraps
}

TEST_F(ProfileManagerTest, CycleWrapsMultipleTimes) {
    mgr.init();
    for (int i = 0; i < 9; ++i) {
        mgr.cycle();
    }
    EXPECT_EQ(mgr.active_slot(), 0);  // 9 % 3 == 0
}

// ─── Persistence ────────────────────────────────────────────────

TEST_F(ProfileManagerTest, CyclePersistsToStorage) {
    mgr.init();
    mgr.cycle();
    EXPECT_TRUE(storage.has_stored);
    EXPECT_EQ(storage.stored_value, 1);
}

TEST_F(ProfileManagerTest, InitRestoresFromStorage) {
    storage.has_stored = true;
    storage.stored_value = 2;
    mgr.init();
    EXPECT_EQ(mgr.active_slot(), 2);
}

TEST_F(ProfileManagerTest, InitClampsInvalidStoredValue) {
    storage.has_stored = true;
    storage.stored_value = 99;  // out of range
    mgr.init();
    EXPECT_EQ(mgr.active_slot(), 0);  // falls back to 0
}

TEST_F(ProfileManagerTest, InitWithEmptyStorageDefaultsTo0) {
    storage.has_stored = false;
    mgr.init();
    EXPECT_EQ(mgr.active_slot(), 0);
    EXPECT_EQ(storage.load_count, 1);
}

TEST_F(ProfileManagerTest, SetSlotValid) {
    mgr.init();
    mgr.set_slot(2);
    EXPECT_EQ(mgr.active_slot(), 2);
    EXPECT_EQ(storage.stored_value, 2);
}

TEST_F(ProfileManagerTest, SetSlotNoStore) {
    mgr.init();
    mgr.set_slot_no_store(2);
    EXPECT_EQ(mgr.active_slot(), 2);
    EXPECT_EQ(storage.store_count, 0); // Should NOT have stored
}

TEST_F(ProfileManagerTest, SetSlotAddr) {
    mgr.init();
    uint8_t addr[7] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    mgr.set_slot_addr(1, addr);
    
    auto retrieved = mgr.get_addr(1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ((*retrieved)[0], 0x01);
}

TEST_F(ProfileManagerTest, SetSlotOutOfRangeClampsTo0) {
    mgr.init();
    mgr.set_slot(5);
    EXPECT_EQ(mgr.active_slot(), 0);
}

// ─── Compile-time properties ────────────────────────────────────

TEST_F(ProfileManagerTest, NumSlotsIs3) {
    EXPECT_EQ(TestProfileManager::kNumSlots, 3);
}

// ─── Address Storage ────────────────────────────────────────────

TEST_F(ProfileManagerTest, StoreAndRetrieveAddr) {
    mgr.init();
    uint8_t addr[7] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    mgr.update_active_addr(addr);

    auto retrieved = mgr.active_addr();
    ASSERT_TRUE(retrieved.has_value());
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ((*retrieved)[i], addr[i]);
    }
}

TEST_F(ProfileManagerTest, RetrieveAddrBySlot) {
    mgr.init();
    uint8_t addr1[7] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    uint8_t addr2[7] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

    mgr.set_slot(0);
    mgr.update_active_addr(addr1);
    mgr.set_slot(1);
    mgr.update_active_addr(addr2);

    auto s0 = mgr.get_addr(0);
    auto s1 = mgr.get_addr(1);

    ASSERT_TRUE(s0.has_value());
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ((*s0)[6], 0x01);
    EXPECT_EQ((*s1)[6], 0x02);
}

TEST_F(ProfileManagerTest, InitRestoresAddresses) {
    uint8_t addr[7] = {0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    storage.addr_storage["profile/slot1/addr"] = {0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    storage.has_stored = true;
    storage.stored_value = 1;

    mgr.init();
    EXPECT_EQ(mgr.active_slot(), 1);
    auto active = mgr.active_addr();
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ((*active)[1], 0xDE);
}

TEST_F(ProfileManagerTest, EmptySlotReturnsNullopt) {
    mgr.init();
    EXPECT_FALSE(mgr.get_addr(0).has_value());
}
