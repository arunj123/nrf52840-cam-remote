# nRF52840 BLE Camera Remote — Roadmap & Engineering Status

## 1. Project Overview & Architecture
- **Target Hardware:** Pro Micro nRF52840 / Nice!Nano V2 compatible board (`nRF52840-QIAA`, 1 MB Flash, 256 KB RAM)
- **Firmware Stack:** Modern C++ (C++23 on Zephyr v4.3 CI / C++2B on NCS v2.8.0 Windows), Zephyr RTOS
- **Profile Architecture (4 Slots):**
  - **Slots 0–2 (HID Mode):** Consumer Control HID over GATT (`Cam Remote Pro`, PnP ID `0x05AC:0x0220`), Battery Service (`BAS`), Device Information Service (`DIS`).
  - **Slot 3 (Config Mode):** Disables HIDS and advertises `Cam Config` (`9e4571a5-b7d9-4daa-a3e5-a8f086007a2b`) for custom button/encoder mapping via [config.html](file:///c:/Users/arunj/projects/nrf_control/config.html) (Web Bluetooth), persisted in NVS (`remote/config/mappings`).
- **Dual-Target Build Support:**
  - **Windows Local (NCS v2.8.0 / Zephyr v3.7.99):** `adafruit_feather_nrf52840/nrf52840/uf2` via `python scripts/build.py` (with hardware-exact Pro Micro / Nice!Nano overlay).
  - **Linux / WSL / GitHub Actions CI (Zephyr v4.3.0):** `promicro_nrf52840/nrf52840/uf2` via `./scripts/build.sh`.

---

## 2. Review Findings & Fix Checklist

### Critical Firmware & Logic Fixes
- [x] **QDEC ISR Crash (`hog.cpp`)**: Deferred `qdec_trigger_handler` from `nrfx_qdec_0_irq_handler` ISR via `enc_rot_sem` to dedicated thread `encoder_rot_thread`.
- [x] **NVS Boot Persistence Wipe (`main.cpp`)**: Called `profile_mgr.init()` *before* `settings_load()` in `bt_ready_callback()` so restored slot and peer addresses persist across reboots.
- [x] **Custom Mappings Unreachable in HID Mode (`gesture_engine.hpp`)**: Applied `ConfigMappings` across HID slots (0–2) with default `{VolumeUp, VolumeUp, VolumeUp, VolumeDown, Mute}` preserved.
- [x] **Encoder Fast-Spin Detent Drops & Static State (`gesture_engine.hpp`)**: Replaced single `if` + `%= 60` with bounded `while` loop (`kMaxStepsPerReport = 10`) and resettable class static `accumulated_degrees_`.
- [x] **Double Beep on Trigger (`main.cpp`)**: Removed redundant `BuzzerController::beep()` from `LedController::set_trigger_active(true)`.
- [x] **Zeroed Bond Address Handling (`profile_manager.hpp`, `main.cpp`)**: Treated all-zero addresses as unbonded in `init()`/`set_slot_addr()` and deleted cleared keys via `settings_delete()`.
- [x] **Permanent Identity Deletion on `clear_bonds()` (`main.cpp`)**: Switched `bt_id_delete()` + `bt_id_create()` to in-place `bt_id_reset(id, nullptr, nullptr)` for `id > 0`.
- [x] **`write_config` ATT Error Return, Validation & Mutex (`hog.cpp`)**: Returned `BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET)`, rejected invalid key bitmasks (`~0x3FU`) with `BT_ATT_ERR_VALUE_NOT_ALLOWED`, and guarded `g_mappings` with `config_mutex`.
- [x] **Button Interrupt & Connection Guards (`hog.cpp`)**: Handled P7 and P4/P5 ISR pins independently with port checks, switched to `GPIO_INT_EDGE_TO_ACTIVE`, verified `device_is_ready()` on GPIO ports, returned 1-byte `0x00` in `read_input_report`, guarded `send_hid_report` in Slot 3, and unref'd prior `active_conn` in `hid_set_conn`.
- [x] **Advertising Watchdog, ID Logging & Boot Workqueue (`main.cpp`)**: Logged the active slot's identity address in `try_advertising()`, gated directed/undirected toggling on bonded HID slots, validated `bt_conn_get_info()` + `active_slot() < 3` in `pairing_complete()`, moved callback registration into `BluetoothManager::start()`, and removed blocking `k_msleep(500)` loop from `bt_ready_callback`.
- [x] **Steady Advertising Watchdog (`main.cpp`)**: Prevented `adv_watchdog_handler()` from stopping/restarting BLE advertising every 5s when already advertising in the matching slot and mode (`if (!is_advertising)`).
- [x] **QSPI Child Node Pin Contention (`adafruit_feather_nrf52840_nrf52840_uf2.overlay`)**: Disabled `&gd25q16` in addition to `&qspi` so `qspi_nor` does not grab `P0.17`, `P0.20`, or `P0.22` (`CONFIG_NORDIC_QSPI_NOR` stripped completely).
- [x] **Battery SAADC Oversampling & Clamp (`battery.cpp`, `battery_conversion.hpp`)**: Propagated DT `zephyr_oversampling` (`4` = 16x) into `adc_sequence.oversampling` and clamped `raw_to_millivolts` upper input to `4095`.

### Unit Tests, Build Config, Skills & Docs
- [x] **Skills Centralization**: Moved all 9 skills from `.agent/skills/` to shared `C:\Users\arunj\projects\.agents\skills\` (parent repo commit `0d38364`), updated `projects/README.md`, and fixed `nrf52840_skills.py` GPIO flags & SAADC AIN-to-GPIO pin map.
- [x] **Host Unit Tests (`firmware/tests/`)**: Updated `test_battery_conversion.cpp` to 12-bit calibrated formula and expanded `test_gesture_engine.cpp` & `test_profile_manager.cpp` to **45/45 passing tests**.
- [x] **Dual-SDK Build Support & Configs**: Removed `provisioning_service.hpp`, split board-specific C++/USB flags out of `prj.conf`, added `adafruit_feather_nrf52840_nrf52840_uf2.{conf,overlay}` and `scripts/build.py`, updated `.gitignore`, restored `+x` modes on `scripts/*.sh`, and added CI unit tests + UF2 artifact upload to `.github/workflows/build.yml`.
- [x] **Web Configurator & Docs**: Updated `config.html` (`writeValueWithResponse`, inline status, Disconnect button) and synchronized `README.md`, `docs/gesture_logic.md`, and `.agent/rules/requirements.md`.

---

## 3. Connected Hardware Observation
- **Detected Bootloader (`D:\INFO_UF2.TXT` before first flash):**
  - `Model: nice!nano` / `Board-ID: nRF52840-nicenano`
  - `UF2 Bootloader 0.6.0` (`SoftDevice: S140 version 6.1.1`, Date: `Jun 19 2021`)
  - Exposed on `D:` (`NICENANO` FAT volume) and `COM9` (`USB\VID_239A&PID_00B3`).
- **Recommendation (Manual Action Only):**
  - Bootloader v0.6.0 is legacy (lacks flash queue overflow protection, ACL write-protection, USB-disconnect auto-exit, and `REGOUT0` fixes).
  - Recommended upgrade package in centralized skills: `C:\Users\arunj\projects\.agents\skills\nicenano-nrf52840\resources\bootloader\update-nice_nano_bootloader-0.11.0_nosd.uf2`.
  - Per safety policy, only application firmware (`zephyr.uf2`) was flashed; bootloader upgrade is left to the user.

---

## 4. Verification & Handover Status
- **Host Unit Tests (`python scripts/build.py --test`):** **45 / 45 passed** (`100% tests passed, 0 tests failed out of 45`, `1.29 sec`) across `test_gesture_engine` (15), `test_battery_conversion` (11), and `test_profile_manager` (19).
- **Latest NCS v2.8.0 Clean Firmware Build (`python scripts/build.py --clean`):**
  - **Result:** Exit code `0`, **0 warnings** in `src/*.cpp`, `CONFIG_NORDIC_QSPI_NOR` disabled (`# CONFIG_NRFX_QSPI is not set`).
  - **Linker Footprint:** `FLASH: 219,716 B / 796 KB (26.96%)`, `RAM: 52,156 B / 256 KB (19.90%)`.
  - **`arm-zephyr-eabi-size`:** `ROM / Flash Used: 219,707 Bytes / 1,048,576 Bytes (20.95%)`, `RAM Used: 52,550 Bytes / 262,144 Bytes (20.05%)`.
  - **UF2 Output:** `firmware/build_adafruit_feather_nrf52840_nrf52840_uf2/zephyr/zephyr.uf2` (`439,808 bytes`, start address `0x26000`).
- **Hardware Flash & Serial Boot Telemetry (`COM9`, captured from initial UF2 flash before the `&gd25q16` & watchdog patch):**
  - Flashed `449,024` bytes to `D:\FLASH.UF2`; UF2 bootloader unmounted cleanly and booted into `COM9` (`VID:PID 239A:00B3`).
  - Boot log confirmed: `Booting nRF Connect SDK v2.8.0`, `Using Zephyr OS v3.7.99`, `Bluetooth initialized`, `Battery monitor initialized`, `Button thread started`, `Encoder button thread started`, `Encoder rotation thread started`, `BAT: raw=389, avg=389, mV=5223, %=100`, `HID Ready`, `ADV: start slot 0 using ID addr F5:AA:CF:43:EF:5B (random)`.
  - Expected NVS notice: `bt_keys: Invalid key length 56 != 84` (`settings: set-value failure ... error(-22)`) from pre-existing Zephyr 4.3 bond entries in `storage_partition`; safely skipped without crash.
  - Because the board already exited bootloader mode into the running application (`D:\INFO_UF2.TXT` absent), the final `&gd25q16` + advertising-watchdog rebuild (`439,808` bytes `zephyr.uf2`) has **not** yet been flashed onto the board.
- **Exact Next Step:**
  1. Double-tap Reset on the Nice!Nano to re-enter UF2 bootloader (`D:` / `NICENANO`), then run `python scripts/build.py --flash` to flash the final `439,808`-byte `zephyr.uf2`.
  2. Verify clean `COM9` boot output (confirming `qspi_nor` error and 5-second `ADV:` restart logs are gone), then test BLE HID shutter/encoder and Slot 3 `config.html` pairing on target hardware.
  3. Optionally upgrade bootloader from v0.6.0 to v0.11.0 using `update-nice_nano_bootloader-0.11.0_nosd.uf2`.
