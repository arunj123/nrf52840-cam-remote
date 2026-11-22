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
On Windows:
```powershell
python scripts/build.py --test
python scripts/build.py
python scripts/build.py --flash
```

## 3. Post-Verification Commit & Session Hand-Over
- After every verified build, passing test suite, or hardware flash, update `docs/ROADMAP.md` and create a structured Git commit following `.agent/rules/project-lifecycle.md`.
- Prompt the user to start a fresh session referencing `docs/ROADMAP.md` whenever context size or turn depth makes continuing economically suboptimal.

## 4. Environment Setup
If the environment is not set up, run:
```bash
./scripts/setup_env.sh
```
