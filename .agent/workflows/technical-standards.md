---
description: Technical standards, code rules, and development workflows for the nRF52840 BLE Camera Remote.
---
# Technical Standards & Developer Rules

This document outlines the mandatory technical standards and workflows for the nRF52840 project. All contributions must adhere to these rules.

## 1. Compliance & Maintenance
- **Requirement Sync**: Whenever a feature, build setting, or pin assignment is changed, the corresponding entry in [functional-requirements.md](functional-requirements.md) **MUST** be updated immediately.
- **Documentation Sync**: README.md and modular docs in `docs/` must be updated in tandem with code changes.

## 2. Language & Framework Standards
- **Language**: Modern C++ (Target: **C++23**).
- **Framework**: Zephyr RTOS (v4.x).
- **Modern Features**:
    - **Maximal Compile-time Execution**: Use `constexpr`, `consteval`, and `constinit`.
    - **Static Polymorphism**: Prefer templates/CRTP over `virtual` functions.
    - **Modern Types**: Use `std::array`, `std::span`, `std::string_view`, and fixed-width integers from `<cstdint>`.
    - **Error Handling**: Use `std::optional` or `std::expected`.

## 3. Design for Testability (TDD)
- **Decoupling**: Business logic must be decoupled from hardware-specific code (Zephyr drivers, BLE stack).
- **Hardware Abstraction**: Use templates or interface-based designs to swap real drivers with mocks on the host.
- **Unit Testing**:
    - Core logic (e.g., `GestureEngine`) must have 100% host-based test coverage using **GoogleTest**.
    - Tests must run on Linux/WSL via `./scripts/test.sh`.

## 4. Documentation & Diagrams
- **Mermaid**: Use ` ```mermaid ` blocks for all architecture, state machine, and sequence diagrams.
- **Modularity**: Complex diagrams should live in separate `.md` files in `docs/` and be linked from README.md.

## 5. Workflow Scripts
- **Build**: `./scripts/build.sh`
- **Test**: `./scripts/test.sh`
- **Setup**: `./scripts/setup_env.sh`
