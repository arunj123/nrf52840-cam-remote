---
trigger: always_on
description: Enforces mandatory docs/ROADMAP.md maintenance, post-verification structured Git commits, and proactive economic session hand-overs.
globs: "**/*"
---

# Project Lifecycle & Verification Commit Rules

This rule enforces [`../../.agents/rules/agent_project_lifecycle.md`](../../.agents/rules/agent_project_lifecycle.md) within the `nrf_control` repository (which has its own `.git` boundary):

1. **Mandatory `docs/ROADMAP.md`**: Keep [docs/ROADMAP.md](../../docs/ROADMAP.md) up to date with active status, test counts, flash/RAM footprint, hardware telemetry, and unambiguous next steps.
2. **Commit After Every Verified Build / Flash**: After every successful build, passing unit test run, or verified hardware flash, create a structured Git commit containing:
   - Descriptive Conventional Commits title (`feat(...)`, `fix(...)`, `test(...)`, `chore(...)`).
   - Detailed summary of changes made.
   - Verification & validation results (test pass count, compiler/linker Flash & RAM footprint, serial/hardware status).
   - Debug data and findings (anomalies, serial logs, root-cause fixes).
   - Exact next planned step.
3. **Economic Session Hand-Over**: When session depth/context grows large or continuing in the same session becomes economically suboptimal:
   - Update `docs/ROADMAP.md` and docs,
   - Commit all verified changes,
   - Explicitly prompt the user to start a new clean session referencing `docs/ROADMAP.md`.
