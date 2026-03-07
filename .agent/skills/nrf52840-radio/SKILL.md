# nRF52840 RADIO Skill

This skill focuses on the configuration and optimization of the 2.4 GHz RADIO peripheral for BLE, IEEE 802.15.4, and proprietary protocols.

## 1. Transmit Power (TXPOWER)
- Range: -40 dBm to +8 dBm.
- Default in many stacks is 0 dBm.
- **Optimization**: Use +8 dBm for range, or -12 dBm to -20 dBm for indoor proximity remotes to save power.

## 2. RSSI (Received Signal Strength Indicator)
- The RADIO peripheral provides hardware RSSI measurement.
- Useful for proximity detection or connection quality monitoring.
- RSSI sample time is ~0.25 µs.

## 3. Multi-protocol Support
- nRF52840 supports concurrent BLE and IEEE 802.15.4 (Zigbee/Thread).
- Uses a hardware-assisted arbitrator to manage radio time between protocols.

## 4. EasyDMA
- The Radio peripheral uses EasyDMA to transfer packets directly to/from RAM.
- Packet buffer pointers must be in **Data RAM** (not Flash or auxiliary memory).

## 5. Packet Buffer (PAYLOAD)
- Hardware supports up to 255-byte payloads.
- Essential for high-throughput BLE (DLE - Data Length Extension).
