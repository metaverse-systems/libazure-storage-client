# Implementation Plan: Synchronous CRUD Operations

**Branch**: `003-sync-crud-operations` | **Date**: 2026-03-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/003-sync-crud-operations/spec.md`

## Summary

Implement the five synchronous CRUD stub methods on `AzureTableClient`: `GetEntity`, `UpsertEntity`, `DeleteEntity`, `QueryEntities`, and `BatchUpsertEntities`. This requires extending the internal `perform_request` helper to return a response struct (status code, body, headers) and implementing each method against the Azure Table Storage REST API. All operations reuse the existing Shared Key / Bearer Token authentication dispatch.

## Technical Context

**Language/Version**: C++20
**Primary Dependencies**: libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON)
**Storage**: Azure Table Storage (remote via REST API); Azurite for local testing
**Testing**: Manual verification against Azurite (`http://127.0.0.1:10002/devstoreaccount1`)
**Target Platform**: Linux (GCC/Clang), Windows (MinGW cross-compilation)
**Project Type**: Library (shared object `.so`/`.dll`)
**Build System**: Autotools (`configure.ac`, `Makefile.am`, `src/Makefile.am`)
**Performance Goals**: N/A — synchronous blocking calls; no throughput targets
**Constraints**: No retry logic; no exceptions; caller-facing error handling via return values only
**Scale/Scope**: 5 method implementations + 1 internal helper refactor in a single `.cpp` file

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | Standalone Library — Zero Framework Coupling | PASS | No new dependencies. All work uses existing libcurl, OpenSSL, nlohmann-json. |
| II | Azure REST API Fidelity | PASS | Each method implements the exact HTTP verb, URL pattern, and header requirements per Azure Table Storage REST API docs. Batch uses Entity Group Transaction multipart format. Query follows continuation tokens. Delete uses `If-Match: *`. |
| III | Dual Authentication — Shared Key and Bearer Token | PASS | All new methods reuse the existing auth dispatch pattern from `CreateTableIfNotExists`. No changes to auth logic. |
| IV | Minimal Surface Area | PASS | No new public methods added — only implementing already-declared stubs. No retry logic, no convenience wrappers. Internal `HttpResponse` struct stays in the anonymous namespace. |
| V | Cross-Platform Portability | PASS | No platform-specific code. Only standard C++20, libcurl, and OpenSSL APIs. No changes to build system. |

**Gate result**: ALL PASS. No violations. Proceeding to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/003-sync-crud-operations/
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
├── azure-storage-client.hpp   # Public header (no changes — stubs already declared)
└── json.hpp                   # nlohmann-json (vendored, no changes)

src/
├── azure-storage-client.cpp   # Implementation file (all changes here)
├── Makefile.am                # Build rules (no changes expected)
└── Makefile.in                # Generated (no changes)
```

**Structure Decision**: Single-project flat layout. All implementation changes are in `src/azure-storage-client.cpp` within the existing anonymous namespace (for the `HttpResponse` struct and refactored `perform_request`) and the `AzureTableClient` method definitions. No new files needed for source code.

## Complexity Tracking

> No constitution violations — this section is intentionally empty.

## Post-Design Constitution Re-check

*Re-evaluated after Phase 1 design completion.*

| # | Principle | Status | Post-Design Notes |
|---|-----------|--------|-------------------|
| I | Standalone Library — Zero Framework Coupling | PASS | No new dependencies. `HttpResponse` struct and `header_callback` use only standard C++ and libcurl. |
| II | Azure REST API Fidelity | PASS | All URL patterns, HTTP methods, headers, and multipart batch format verified against Azure Table Storage REST API docs. |
| III | Dual Authentication — Shared Key and Bearer Token | PASS | All operations reuse existing auth dispatch unchanged. |
| IV | Minimal Surface Area | PASS | No new public methods. `HttpResponse` is internal. No retry/caching/wrappers. |
| V | Cross-Platform Portability | PASS | No platform-specific code. Standard libcurl API only. No build system changes. |

**Gate result**: ALL PASS post-design. Ready for `/speckit.tasks`.
