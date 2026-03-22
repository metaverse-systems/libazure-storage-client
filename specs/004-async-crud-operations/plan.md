# Implementation Plan: Asynchronous CRUD Operations

**Branch**: `004-async-crud-operations` | **Date**: 2026-03-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/004-async-crud-operations/spec.md`

## Summary

Implement the five asynchronous CRUD methods on `AzureTableClient` (`GetEntityAsync`, `UpsertEntityAsync`, `DeleteEntityAsync`, `QueryEntitiesAsync`, `BatchUpsertEntitiesAsync`). Each method captures a snapshot of client state at call time, spawns a detached `std::thread` that delegates to the corresponding synchronous method, and invokes the user-provided callback with the result. No new public API surface — only replacing the existing stub implementations.

## Technical Context

**Language/Version**: C++20
**Primary Dependencies**: libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON), `<thread>` (C++ standard library)
**Storage**: Azure Table Storage (remote via REST API); Azurite for local testing
**Testing**: Manual verification against Azurite (`http://127.0.0.1:10002/devstoreaccount1`)
**Target Platform**: Linux (GCC/Clang), Windows (MinGW cross-compilation)
**Project Type**: Library (shared object `.so`/`.dll`)
**Build System**: Autotools (`configure.ac`, `Makefile.am`, `src/Makefile.am`)
**Performance Goals**: Minimal threading overhead — async methods must return to caller within microseconds; actual I/O time matches sync equivalents
**Constraints**: No retry logic; no exceptions; no thread pools; no concurrency limits; callback invoked exactly once; copy client state at dispatch time
**Scale/Scope**: 5 method implementations in `src/azure-storage-client.cpp`; add `#include <thread>` header

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | Standalone Library — Zero Framework Coupling | PASS | No new dependencies. `std::thread` is C++ standard library (already permitted by C++20). No framework coupling. |
| II | Azure REST API Fidelity | PASS | Async methods delegate directly to the sync implementations which already faithfully implement the REST API. No changes to HTTP request construction. |
| III | Dual Authentication — Shared Key and Bearer Token | PASS | Copy-at-dispatch captures whichever auth mode is active. Both modes work identically in async as in sync. |
| IV | Minimal Surface Area | PASS | No new public methods — only implementing already-declared stubs. No retry, no thread pool, no convenience wrappers. |
| V | Cross-Platform Portability | PASS | `std::thread` is portable across GCC, Clang, and MinGW. No platform-specific code. Build system requires adding `-pthread` to `src/Makefile.am` for `std::thread` to link on Linux; harmless on MinGW. |

**Gate result**: ALL PASS. No violations. Proceeding to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/004-async-crud-operations/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── public-api.md
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
include/libazure-storage-client/
├── azure-storage-client.hpp   # Public header (no changes — async stubs already declared)
└── json.hpp                   # nlohmann-json (vendored, no changes)

src/
├── azure-storage-client.cpp   # Implementation file (all changes here)
├── Makefile.am                # Build rules (add `-pthread` flag)
└── Makefile.in                # Generated (no changes)
```

**Structure Decision**: Single-project flat layout. All changes are in `src/azure-storage-client.cpp` — replacing the five async stub bodies with `std::thread`-based implementations that copy client state and delegate to the sync methods. No new files needed.

## Complexity Tracking

> No constitution violations — this section is intentionally empty.

## Post-Design Constitution Re-check

*Re-evaluated after Phase 1 design completion.*

| # | Principle | Status | Post-Design Notes |
|---|-----------|--------|-------------------|
| I | Standalone Library — Zero Framework Coupling | PASS | `std::thread` is C++ standard library, not a framework. No new external dependencies added. |
| II | Azure REST API Fidelity | PASS | Async methods delegate directly to sync implementations. No changes to HTTP request construction, headers, or API compliance. |
| III | Dual Authentication — Shared Key and Bearer Token | PASS | Copy-at-dispatch captures whichever auth mode is active. Both modes preserved identically in async path. |
| IV | Minimal Surface Area | PASS | No new public methods. No retry logic, thread pools, or convenience wrappers. Build system change (`-pthread`) is minimal and required for `std::thread` to link. |
| V | Cross-Platform Portability | PASS | `std::thread` supported on all target platforms (GCC, Clang, MinGW). `-pthread` flag is harmless on MinGW. No platform-specific code. Build system requires `-pthread` added to `src/Makefile.am` (see research R5). |

**Gate result**: ALL PASS post-design. Ready for `/speckit.tasks`.
