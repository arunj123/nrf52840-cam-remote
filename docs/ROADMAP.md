# nRF52840 BLE Camera Remote — Roadmap & Engineering Status

## 1. Project Overview & Architecture
- **Target Hardware:** Pro Micro nRF52840 / Nice!Nano V2 compatible board (`nRF52840-QIAA`, 1 MB Flash, 256 KB RAM)
- **Firmware Stack:** Modern C++ (C++23 on Zephyr v4.3 CI / C++2B on NCS v2.8.0 Windows), Zephyr RTOS
- **Profile Architecture (4 Slots, `CONFIG_BT_MAX_CONN=4`):**
  - **Slots 0–2 (HID Mode, BT IDs `1–3`):** Composite 3-Report HID over GATT (`Cam Remote Pro 1/2/3`, PnP ID `0x05AC:0x0220`: Report 1 Consumer Control + AC Back/Forward, Report 2 Mouse Clicks/Vertical Wheel/Horizontal Pan, Report 3 Standard 8-byte Keyboard + Modifiers), Battery Service (`BAS`), Device Information Service (`DIS`). Each HID slot maps to a resettable Zephyr BT identity (`slot + 1`), maintains independent per-slot connections (`slot_conns[4]`) across slot switches, and dispatches its own 7-byte section of `AllSlotMappings`.
  - **Slot 3 (Config Mode, BT ID `0`):** Suppresses HID notifications/QDEC and advertises `Cam Config` (`9e4571a5-b7d9-4daa-a3e5-a8f086007a2b`) with unencrypted Slot-3-gated GATT read/write (`BT_GATT_PERM_READ | BT_GATT_PERM_WRITE`) for direct Web Bluetooth configuration via [config.html](file:///c:/Users/arunj/projects/nrf_control/config.html), persisted in NVS (`remote/config/mappings`, `21` bytes). Slot 3 disconnects automatically when cycling back to Slot 0.
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

- [x] **Per-Slot BLE Connections & Reconnection (`prj.conf`, `hog.cpp`, `main.cpp`)**: Raised `CONFIG_BT_MAX_CONN=4`, replaced single `active_conn` with per-slot `slot_conns[4]` routed via `bt_conn_get_info().id` (`Slots 0–2 -> BT IDs 1–3`, `Slot 3 -> BT ID 0`), kept HID Slots `0–2` connected across slot switches (disconnecting Slot 3 only on exit), removed `BT_ATT_ERR_NOT_SUPPORTED` from `hog_svc` in Slot 3, replaced `BT_LE_ADV_OPT_DIR_ADDR_RPA` directed advertising with standard fast undirected connectable advertising (`ADV_IND`), and waited for disconnect before `bt_id_reset()` in `clear_bonds()`.
- [x] **Web Bluetooth Slot 3 Auth & Per-Slot Mappings (`hog.hpp`, `hog.cpp`, `main.cpp`, `config.html`)**: Switched `remote_svc` characteristic permissions to `BT_GATT_PERM_READ | BT_GATT_PERM_WRITE` (fixing Windows `NetworkError: Authentication failed`), replaced single shared `ConfigMappings` with `AllSlotMappings` (`15` bytes = `3 x 5` bytes for HID Slots `0–2`, keeping 5-byte NVS/BLE backward compatibility), and added 3-slot tabs + "Copy to All Slots" + early `gattserverdisconnected` handling in [config.html](file:///c:/Users/arunj/projects/nrf_control/config.html).
- [x] **Long-Press Responsiveness & Sleep/Wake-Up Fixes (`gesture_engine.hpp`, `hog.cpp`, `main.cpp`)**: Reduced `kLongPressMs` from `800` ms to `500` ms, added immediate `500` ms Red LED + long-beep cue while holding the main button before reaching the 5s bond clear, counted initial `60` ms debounce in `on_encoder_button_press()`, added release debounce across button handlers, moved `HidService::init()` before `BluetoothManager::start()`, cancelled `adv_watchdog_work` before multi-beep sleep in `on_profile_switch_request()` / `clear_bonds()`, added synchronized `N+1` Red LED blinks on profile switch, gated QDEC trigger registration to the active connected HID slot, and increased `CONFIG_USB_CDC_ACM_RINGBUF_SIZE=4096`.

### Unit Tests, Build Config, Skills & Docs
- [x] **Skills Centralization**: Moved all 9 skills from `.agent/skills/` to shared `C:\Users\arunj\projects\.agents\skills\` (parent repo commit `0d38364`), updated `projects/README.md`, and fixed `nrf52840_skills.py` GPIO flags & SAADC AIN-to-GPIO pin map.
- [x] **Host Unit Tests (`firmware/tests/`)**: Updated `test_battery_conversion.cpp` to 12-bit calibrated formula and expanded `test_gesture_engine.cpp` (500 ms threshold + independent Slots `0–2`) & `test_profile_manager.cpp` to **48/48 passing tests**.
- [x] **Dual-SDK Build Support & Configs**: Removed `provisioning_service.hpp`, split board-specific C++/USB flags out of `prj.conf`, added `adafruit_feather_nrf52840_nrf52840_uf2.{conf,overlay}` and `scripts/build.py`, updated `.gitignore`, restored `+x` modes on `scripts/*.sh`, and added CI unit tests + UF2 artifact upload to `.github/workflows/build.yml`.
- [x] **Web Configurator & Docs**: Updated `config.html` (Slot 0–2 tabs, Copy to All Slots, 21-byte `writeValueWithResponse`, inline status, Disconnect button) and synchronized `README.md`, `docs/gesture_logic.md`, and `.agent/rules/requirements.md`.
- [x] **Independent Second Button (`Trigger 2` / `D4` / `P0.17`) (`hog.hpp`, `hog.cpp`, `gesture_engine.hpp`, `main.cpp`, `config.html`)**: Separated `btn_p4` from `btn_p5` onto its own `sec_btn_sem` / `sec_btn` thread with per-slot configurable short click (`< 500 ms`, default `Play/Pause`) and long press (`>= 500 ms`, default `Next Track`), extending `ConfigMappings` to 7 bytes (`AllSlotMappings` = 21 bytes across Slots `0–2`) while preserving 15/7/5-byte legacy NVS/BLE backward compatibility.
- [x] **Composite 3-Report HID & Expanded Configurator (`hog.hpp`, `hog.cpp`, `config.html`, `test_gesture_engine.cpp`)**: Upgraded `hog_svc` from 1 Consumer report to 3 Input Reports (Report ID 1: Consumer Control + AC Back/Forward; Report ID 2: Mouse Clicks + Vertical Wheel + Horizontal AC Pan; Report ID 3: Standard Keyboard + Shortcuts), added 54 `HidKey` actions + `is_valid_hid_key()` validation, and upgraded `config.html` with `<optgroup>` categories and 4 one-click slot presets.
- [x] **Per-Slot Dynamic BLE Device Names (`prj.conf`, `main.cpp`, `config.html`)**: Enabled `CONFIG_BT_DEVICE_NAME_DYNAMIC=y` (`CONFIG_BT_DEVICE_NAME_MAX=24`) and set dynamic GAP (`bt_set_name()`) + advertising names `"Cam Remote Pro 1"`, `"Cam Remote Pro 2"`, `"Cam Remote Pro 3"`, and `"Cam Config"` per active slot; updated slot tab labels in `config.html`.
- [x] **Agent Lifecycle Rule Discovery Fix (`.agent/rules/project-lifecycle.md`, `C:\Users\arunj\.gemini\GEMINI.md`, `../../.agents/rules/agent_project_lifecycle.md`)**: Fixed nested `.git` rule boundary and missing `trigger: always_on` / empty global `GEMINI.md` so post-verification commits and economic session hand-overs trigger reliably.

---

## 3. Connected Hardware Observation
- **Bootloader Upgraded & Verified (`D:\INFO_UF2.TXT`):**
  - Previous: `UF2 Bootloader 0.6.0` (`SoftDevice: S140 version 6.1.1`, Date: `Jun 19 2021`).
  - Flashed `C:\Users\arunj\projects\.agents\skills\nicenano-nrf52840\resources\bootloader\update-nice_nano_bootloader-0.11.0_nosd.uf2` (`72,192` bytes, targeting MBR/bootloader/UICR `0x0..0xb00`, `0xf4000..0xfc000`, `0xfd800..0xfd900`, `0x10001000..0x10001100`) to `D:\UPDATE.UF2`.
  - Verified on re-entry to bootloader mode:
    - `UF2 Bootloader 0.11.0`
    - `Model: nice!nano` / `Board-ID: nRF52840-nicenano`
    - `Date: May 21 2026`, `SoftDevice: S140 6.1.1`
    - Exposed on `D:` (`NICENANO` FAT volume) and `COM9` (`USB\VID_239A&PID_00B3`).

---

## 4. Verification & Handover Status
- **Verified Build Commit:** `32cc98e` (`feat(firmware,config): add second button actions, composite 3-report HID, and per-slot BLE names`)
- **Host Unit Tests (`python scripts/build.py --test`):** **55 / 55 passed** (`100% tests passed, 0 tests failed out of 55`, `1.23 sec`) across `test_gesture_engine` (25), `test_battery_conversion` (11), and `test_profile_manager` (19).
- **Latest NCS v2.8.0 Firmware Build (`python scripts/build.py`):**
  - **Result:** Exit code `0`, **0 warnings** in `src/*.cpp`, `CONFIG_NORDIC_QSPI_NOR` disabled, `CONFIG_USB_CDC_ACM_RINGBUF_SIZE=4096`, `CONFIG_BT_MAX_CONN=4`, `CONFIG_BT_DEVICE_NAME_DYNAMIC=y`.
  - **Linker Footprint:** `FLASH: 222,644 B / 796 KB (27.31%)`, `RAM: 63,916 B / 256 KB (24.38%)`.
  - **`arm-zephyr-eabi-size`:** `ROM / Flash Used: 222,642 Bytes / 1,048,576 Bytes (21.23%)`, `RAM Used: 64,316 Bytes / 262,144 Bytes (24.53%)`.
  - **UF2 Output (Built, Ready to Flash):** `firmware/build_adafruit_feather_nrf52840_nrf52840_uf2/zephyr/zephyr.uf2` (`445,440 bytes`, start address `0x26000`).
- **Previous On-Hardware Telemetry (`COM9`, prior `444,416`-byte build before dynamic per-slot BLE names):**
  - Verified clean boot (`nRF Connect SDK v2.8.0` / `Zephyr OS v3.7.99`), NVS load (`Profile manager ready (slot 1)`), BT Identities `0–3` (`Identity[2]: CE:1F:FE:E5:66:02`), undirected advertising (`ADV: start slot 1 (BT ID 2)`), 5s advertising watchdog, `Secondary button thread started (interrupt-driven)` / `Zzz... Sleeping (SecBtn)`, main/encoder/QDEC threads, battery monitor (`BAT: raw=379, avg=379, mV=5089, %=100`), and automatic BLE reconnection on Slot 1 (`Connected (slot 1, BT ID 2): 48:74:12:46:69:DE (public)`, `HID: slot 1 conn set`, `QDEC: trigger registered`, `Security changed: ... level 2`).
- **Exact Next Step (New Session Hand-Over, Flash & Interactive Verification):**
  1. Start a new clean session referencing `docs/ROADMAP.md`.
  2. Double-tap `RST` to mount `D:\` (`UF2 Bootloader 0.11.0`) and run `python scripts/build.py --flash` to flash the `445,440`-byte `zephyr.uf2`.
  3. Re-pair (or clear bond via 5s `D5` hold) on existing BLE hosts so Windows/macOS/Android refreshes the cached 147-byte composite 3-report HID Report Map and per-slot names (`Cam Remote Pro 1/2/3`), then test vertical/horizontal scroll, mouse clicks, navigation keys, and keyboard shortcuts via [config.html](file:///c:/Users/arunj/projects/nrf_control/config.html).
  4. Push to `origin/master` when ready so GitHub Pages serves the updated [config.html](file:///c:/Users/arunj/projects/nrf_control/config.html).
