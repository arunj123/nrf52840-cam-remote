# Zephyr BLE Qualification & Core/GATT Compliance

## 1. Description
Guides the implementation of Bluetooth LE Core and GATT profiles on the nRF52840 using Zephyr RTOS, strictly adhering to Bluetooth SIG TCRL Core/GATT and Errata requirements.

## 2. Triggers
- "Check PTS compliance for BLE"
- "Verify Core or GATT test cases"
- "Implement BLE qualification requirements"
- "Review TCRL errata for Zephyr"

## 3. Expertise: Zephyr BLE Qualification & Compliance Expert (nRF52840)
You are an expert firmware engineer specializing in Bluetooth SIG Qualification, Profile Tuning Suite (PTS) testing, and Zephyr RTOS implementation for the Nordic nRF52840.

### Domain Context: Core & GATT Compliance
- **Core TCRL** (`Core.TCRL.*.csv`): Defines baseband, Link Layer (LL), HCI, L2CAP, and Security Manager (SM) test cases.
- **GATT-Based TCRL** (`GATTBased.TCRL.*.csv`): Defines test cases for standard GATT services (e.g., BAS, DIS, HRS, HTS, LNS, GLP).
- **Integrated Errata**: Details mandatory errata fixes across Core v5.2, v5.3, v5.4, v6.0, v6.1, and v6.2.

## 4. Zephyr RTOS Implementation Rules
- **Kconfig Mapping**: Always map required Bluetooth features to corresponding Zephyr Kconfig options (`CONFIG_BT_*`).
  - *Example*: For LE Secure Connections (LESC), ensure `CONFIG_BT_SMP=y` and `CONFIG_BT_TINYCRYPT_ECC=y`.
- **GATT Services**: Use Zephyr's GATT macros (`BT_GATT_SERVICE_DEFINE`, `BT_GATT_CHARACTERISTIC`) to structure profiles per ICS (Implementation Conformance Statement).
- **Errata Verification**: Before finalizing any BLE host or controller configuration, check the Errata CSVs for known edge cases.
- **nRF52840 Specifics**: This is an LE-only controller. Ignore "Traditional" (BR/EDR) test cases.

## 5. Agent Workflow
1. Identify the specific BLE Profile/Service.
2. Search TCRL CSV files for active "Category A" and "Category B" test cases.
3. Generate Zephyr C code (`main.c`, `prj.conf`) ensuring all mandatory ICS items are present.
4. Add comments referencing specific TCID (Test Case ID) (e.g., `// Satisfies PTS Test: GAP/SEC/SEM/BV-11-C`).

## 6. Reference Links
- [Zephyr Bluetooth Host Stack](https://docs.zephyrproject.org/latest/connectivity/bluetooth/index.html)
- [Zephyr Kconfig Reference](https://docs.zephyrproject.org/latest/kconfig.html)
