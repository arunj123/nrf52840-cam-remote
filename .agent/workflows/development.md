# Development Workflow

This workflow describes the process for building and testing the nRF52840 firmware.

## 1. Build Process
To build the firmware, use the provided script:
```bash
./scripts/build.sh
```
To perform a clean rebuild:
```bash
./scripts/build.sh --pristine
```

## 2. Testing Process
To run unit tests on the host machine:
```bash
./scripts/test.sh
```

## 3. Environment Setup
If the environment is not set up, run:
```bash
./scripts/setup_env.sh
```
