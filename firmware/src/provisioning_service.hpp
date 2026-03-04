/** @file
 *  @brief Provisioning Service — GATT skeleton for button-function programming
 *
 *  Registers a custom BLE GATT service with a vendor-specific 128-bit UUID.
 *  This is a stub: it accepts writes to a configuration characteristic but
 *  does not act on them yet.  A future companion app will use this service
 *  to program what each button/gesture does per profile.
 */

#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/printk.h>

#include <array>
#include <cstdint>

namespace remote {

namespace provisioning {

// ─── Vendor-specific 128-bit UUIDs ──────────────────────────────
// Service:        12345678-1234-5678-1234-56789abcde00
// Characteristic: 12345678-1234-5678-1234-56789abcde01

// clang-format off
#define PROV_SVC_UUID BT_UUID_DECLARE_128( \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcde00))

#define PROV_CHAR_BUTTON_MAP_UUID BT_UUID_DECLARE_128( \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcde01))
// clang-format on

// ─── Placeholder button-map buffer ──────────────────────────────
// Each byte could map a button index → action.  Layout TBD.
inline constexpr uint8_t kMaxButtonMapLen = 8;
inline std::array<uint8_t, kMaxButtonMapLen> button_map{};

// ─── GATT Callbacks (stubs) ─────────────────────────────────────

inline ssize_t read_button_map(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             button_map.data(), button_map.size());
}

inline ssize_t write_button_map(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len,
                                uint16_t offset, uint8_t flags)
{
    if (offset + len > kMaxButtonMapLen) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy(button_map.data() + offset, buf, len);
    printk("PROV: button-map written (offset=%u len=%u) — stub, no action\n",
           offset, len);
    return len;
}

} // namespace provisioning

// ─── GATT Service Registration ──────────────────────────────────
// This macro registers the service at boot; no explicit init() needed.

BT_GATT_SERVICE_DEFINE(provisioning_svc,
    BT_GATT_PRIMARY_SERVICE(PROV_SVC_UUID),
    BT_GATT_CHARACTERISTIC(PROV_CHAR_BUTTON_MAP_UUID,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                           provisioning::read_button_map,
                           provisioning::write_button_map,
                           nullptr),
);

} // namespace remote
