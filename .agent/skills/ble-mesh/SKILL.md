# Zephyr Bluetooth Mesh & MMDL Implementation

## 1. Description
Implements, tests, and verifies Bluetooth Mesh Network and Models (MMDL) on Zephyr RTOS for the nRF52840. Ensures compliance with Bluetooth SIG MESH/MMDL TCRL.

## 2. Triggers
- "Implement Bluetooth Mesh on nRF52840"
- "Create Zephyr Mesh model"
- "Add NLC profile to Mesh"
- "Verify MESH.MMDL test cases"

## 3. Expertise: Zephyr Bluetooth Mesh Profile Expert (nRF52840)
You are an expert in Bluetooth Mesh networking, specifically utilizing the Zephyr RTOS Mesh stack on the Nordic nRF52840 SoC.

### Domain Context: MESH & MMDL Compliance
- **MESH Protocol** (`MESH.MMDL.TCRL.* - MESH.csv`): Core Mesh networking protocol tests (Provisioning, Network, Transport, Access layers).
- **Mesh Models** (`MMDL*.csv`): Tests for standard foundation models, generic models, sensors, time/scenes, and lighting (LCTL, LXYL).
- **Network Lighting Control (NLC)**: Tests for NLC profiles including ALSNLCP, BLCNLCP, BSSNLCP, and DICNLCP.
- **Errata**: Required workarounds for MESH Protocol v1.1 and MMDL v1.1.

## 4. Zephyr RTOS Implementation Rules
- **Core Initialization**: Initialize using `bt_mesh_init()` and handle provisioning via `bt_mesh_prov`.
- **Model Definitions**: Use Zephyr macros (`BT_MESH_MODEL_CB`, `BT_MESH_MODEL_CFG_SRV`, etc.) to define elements and models.
- **NLC Profiles**: Ensure the exact combination of mandatory Server/Client models is bound to the element per the NLC spec.
- **Kconfig Dependencies**: Enforce `CONFIG_BT_MESH=y`, `CONFIG_BT_MESH_PB_ADV=y`, etc.

## 5. Agent Workflow
1. Analyze requested Mesh node type (e.g., Lightness Server, Sensor Client).
2. Look up required models in MESH.MMDL CSV files.
3. Draft Composition Data (`bt_mesh_comp`) ensuring elements/models are structured compliantly.
4. Generate publication/subscription logic and state binding callbacks.
5. Cross-reference Errata files to ensure no deprecated behaviors are used.

## 6. Reference Links
- [Zephyr Bluetooth Mesh Documentation](https://docs.zephyrproject.org/latest/connectivity/bluetooth/bluetooth-mesh/index.html)
- [Bluetooth SIG Mesh Specifications](https://www.bluetooth.com/specifications/specs/)
