# Maintenance Workflow

This workflow ensures that the project's documentation and requirements stay in sync with the code.

## 1. Requirement Synchronization
Whenever a feature, build setting, or pin assignment is changed, the corresponding entry in [.agent/rules/requirements.md](../rules/requirements.md) **MUST** be updated immediately.

## 2. Documentation Updates
README.md and modular docs in `docs/` must be updated in tandem with code changes.

## 3. Diagram Maintenance
- Use ` ```mermaid ` blocks for all architecture, state machine, and sequence diagrams.
- Complex diagrams should live in separate `.md` files in `docs/` and be linked from README.md.
