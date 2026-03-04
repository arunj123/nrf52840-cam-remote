/** @file
 *  @brief Profile Manager — switch HID target between bonded devices (C++23)
 *
 *  Template-based, hardware-decoupled profile slot manager.
 *  Persists the active slot index via a StorageHal abstraction.
 *
 *  StorageHal concept:
 *    struct StorageHal {
 *        static bool store(std::string_view key, uint8_t value);
 *        static std::optional<uint8_t> load(std::string_view key);
 *        static bool store_addr(std::string_view key, const uint8_t* addr);
 *        static std::optional<std::array<uint8_t, 7>> load_addr(std::string_view key);
 *    };
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <array>
#include <cstdio>

namespace remote {

/// Compile-time profile slot manager.
/// @tparam StorageHal  NVS back-end (swappable for testing)
/// @tparam N           Number of profile slots (default 3)
template <typename StorageHal, uint8_t N = 3>
class ProfileManager {
public:
    static_assert(N >= 2, "Need at least 2 profile slots");

    static constexpr uint8_t kNumSlots = N;
    static constexpr std::string_view kStorageKey = "profile/slot";

    /// Initialise from NVS.  Falls back to slot 0 on first boot.
    void init() {
        auto saved = StorageHal::load(kStorageKey);
        active_ = (saved.has_value() && *saved < N) ? *saved : 0;

        for (uint8_t i = 0; i < N; ++i) {
            char key[32];
            std::snprintf(key, sizeof(key), "profile/slot%u/addr", i);
            auto addr = StorageHal::load_addr(key);
            if (addr) {
                slot_addrs_[i] = *addr;
                has_bond_[i] = true;
            } else {
                has_bond_[i] = false;
            }
        }
    }

    /// Advance to the next slot (wraps around).
    uint8_t cycle() {
        active_ = (active_ + 1) % N;
        StorageHal::store(kStorageKey, active_);
        return active_;
    }

    /// @return current active slot (0-based).
    [[nodiscard]] constexpr uint8_t active_slot() const { return active_; }

    /// Directly select a slot (clamped to valid range).
    void set_slot(uint8_t slot) {
        active_ = (slot < N) ? slot : 0;
        StorageHal::store(kStorageKey, active_);
    }

    /// Set slot without storing (used by settings load)
    void set_slot_no_store(uint8_t slot) {
        active_ = (slot < N) ? slot : 0;
    }

    /// Directly set address for a slot (used by settings load)
    void set_slot_addr(uint8_t slot, const uint8_t* addr_bytes) {
        if (slot >= N || !addr_bytes) return;
        for (int i = 0; i < 7; ++i) slot_addrs_[slot][i] = addr_bytes[i];
        has_bond_[slot] = true;
    }

    /// Update the address for the currently active slot.
    void update_active_addr(const uint8_t* addr_bytes) {
        if (!addr_bytes) return;

        std::array<uint8_t, 7> new_addr;
        for (int i = 0; i < 7; ++i) new_addr[i] = addr_bytes[i];

        slot_addrs_[active_] = new_addr;
        has_bond_[active_] = true;

        char key[32];
        std::snprintf(key, sizeof(key), "profile/slot%u/addr", active_);
        StorageHal::store_addr(key, addr_bytes);
    }

    /// @return the stored address for a slot, if it has a bond.
    [[nodiscard]] std::optional<std::array<uint8_t, 7>> get_addr(uint8_t slot) const {
        if (slot < N && has_bond_[slot]) {
            return slot_addrs_[slot];
        }
        return std::nullopt;
    }

    /// @return the stored address for the current active slot.
    [[nodiscard]] std::optional<std::array<uint8_t, 7>> active_addr() const {
        return get_addr(active_);
    }

private:
    uint8_t active_{0};
    std::array<std::array<uint8_t, 7>, N> slot_addrs_{};
    std::array<bool, N> has_bond_{};
};

} // namespace remote
