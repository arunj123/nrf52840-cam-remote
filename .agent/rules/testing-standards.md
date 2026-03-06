# Testing Standards

This document outlines the requirements for Test-Driven Development (TDD) and hardware abstraction.

## 1. Design for Testability
- **Decoupling**: Business logic must be decoupled from hardware-specific code (Zephyr drivers, BLE stack).
- **Hardware Abstraction**: Use templates or interface-based designs to swap real drivers with mocks on the host.

## 2. Unit Testing
- Core logic (e.g., `GestureEngine`) must have 100% host-based test coverage using **GoogleTest**.
- Tests must be runnable on Linux/WSL.

## 3. Verification
- All new features must include corresponding unit tests.
- CI/GitHub Actions should be used to verify builds and tests.
