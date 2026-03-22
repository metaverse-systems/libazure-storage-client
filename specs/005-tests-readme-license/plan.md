# Implementation Plan: Tests, README, and LICENSE

**Branch**: `005-tests-readme-license` | **Date**: 2026-03-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/005-tests-readme-license/spec.md`

## Summary

Add a Catch2 v3 test suite covering all 11 public methods of `AzureTableClient` (6 sync, 5 async) plus edge cases, integrate the test executable into the existing Autotools build, and add README.md and LICENSE (MIT) files to the repository root.

## Technical Context

**Language/Version**: C++20  
**Primary Dependencies**: libcurl, OpenSSL, nlohmann-json (existing); Catch2 v3 (new, test-only)  
**Storage**: Azure Table Storage via REST API (tested against local Azurite emulator)  
**Testing**: Catch2 v3 via pkg-config (`catch2-with-main`), single test source file with tags  
**Target Platform**: Linux (GCC/Clang), Windows (MinGW cross-compilation)  
**Project Type**: Library (shared `.so`/`.dll`)  
**Performance Goals**: Test suite completes in <30 seconds  
**Constraints**: No new runtime dependencies; Catch2 is test-time only  
**Scale/Scope**: 1 class, 11 public methods, ~15-20 test cases

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Relevant? | Status | Notes |
|-----------|-----------|--------|-------|
| I. Standalone Library — Zero Framework Coupling | Yes | **PASS** | Catch2 is test-only, not linked into the library itself. No new runtime dependency. |
| II. Azure REST API Fidelity | Yes | **PASS** | Tests verify API fidelity (409 on CreateTable, `If-Match: *` on Delete, batch ≤100, pagination). |
| III. Dual Authentication | Yes | **PASS** | FR-004 requires tests for both Shared Key and Bearer Token modes. |
| IV. Minimal Surface Area | Yes | **PASS** | No new public API methods added. README documents existing API only. |
| V. Cross-Platform Portability | Yes | **PASS** | Autotools build system extended (not replaced). `tests/Makefile.am` follows existing pattern. |
| Technical Constraints — naming (`this->`, no trailing `_`) | Yes | **PASS** | Test code is external to library; no new library source changes. |
| Technical Constraints — dependencies exhaustive list | Yes | **PASS** | Catch2 is test-time only, not a runtime dependency of the library. |
| Development Workflow — Azurite | Yes | **PASS** | Tests run against Azurite at `http://127.0.0.1:10002/devstoreaccount1`. |

**Gate result: ALL PASS** — no violations, no justifications needed.

## Project Structure

### Documentation (this feature)

```text
specs/005-tests-readme-license/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (public-api.md)
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
├── configure.ac              # Modified: add PKG_CHECK_MODULES for catch2, AC_CONFIG_FILES for tests/Makefile
├── Makefile.am               # Modified: add tests to SUBDIRS
├── tests/
│   ├── Makefile.am           # New: build test executable
│   └── test_azure_table_client.cpp  # New: all Catch2 test cases
├── README.md                 # New: project documentation
└── LICENSE                   # New: MIT license text
```

**Structure Decision**: Tests live in a new `tests/` subdirectory at the repository root, following the Autotools convention of a separate `SUBDIRS` entry. The test executable links against the library built in `src/`. README.md and LICENSE are standard root-level files.

## Complexity Tracking

No constitution violations — this section is intentionally empty.
