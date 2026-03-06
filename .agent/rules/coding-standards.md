# Coding Standards

This document defines the language and framework standards for the nRF52840 project.

## 1. Language & Framework
- **Language**: Modern C++ (Target: **C++23**).
- **Framework**: Zephyr RTOS (v4.x).

## 2. Modern C++ Features
- **Maximal Compile-time Execution**: Use `constexpr`, `consteval`, and `constinit`.
- **Static Polymorphism**: Prefer templates/CRTP over `virtual` functions.
- **Modern Types**: Use `std::array`, `std::span`, `std::string_view`, and fixed-width integers from `<cstdint>`.
- **Error Handling**: Use `std::optional` or `std::expected`.

## 3. Formatting & Style
- Use consistent indentation (4 spaces).
- Follow standard C++ naming conventions (PascalCase for classes, camelCase for methods/variables).
