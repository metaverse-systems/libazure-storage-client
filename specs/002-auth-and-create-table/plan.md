# Implementation Plan: Auth & CreateTableIfNotExists

**Branch**: `002-auth-and-create-table` | **Date**: 2026-03-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-auth-and-create-table/spec.md`

## Summary

Implement the two authentication modes (Shared Key Lite and Bearer Token) and the `CreateTableIfNotExists` operation for the `AzureTableClient` C++ library. The Shared Key path constructs the StringToSign per Azure Table Storage Shared Key Lite format, HMAC-SHA256 signs it with OpenSSL, and emits the `Authorization: SharedKey` header. The Bearer Token path emits `Authorization: Bearer {token}`. `CreateTableIfNotExists` POSTs to `{endpoint}/Tables` and treats HTTP 201 and 409 as success. All Shared Key and CreateTable scenarios are verified against Azurite.

## Technical Context

**Language/Version**: C++20
**Primary Dependencies**: libcurl (HTTP transport), OpenSSL (HMAC-SHA256 + base64), nlohmann-json (JSON serialization)
**Storage**: N/A (library is a REST client; Azure Table Storage is the remote store)
**Testing**: Manual verification against Azurite (`http://127.0.0.1:10002/devstoreaccount1`); no test framework in project currently
**Target Platform**: Linux (GCC/Clang), Windows (MinGW cross-compile)
**Project Type**: Shared library (.so / .dll)
**Performance Goals**: N/A (thin REST client; performance bounded by network)
**Constraints**: No exceptions, no retry logic, no additional dependencies (constitution Principle IV)
**Scale/Scope**: Single source file (`src/azure-storage-client.cpp`), single public header

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Requirement | Compliance | Notes |
|-----------|-------------|------------|-------|
| I. Standalone Library | No framework coupling; deps limited to libcurl, OpenSSL, nlohmann-json | ✅ PASS | Only uses approved dependencies |
| II. Azure REST API Fidelity | Canonical headers (`x-ms-version`, `x-ms-date`, `Accept`, `Content-Type`); 409 = success for CreateTable | ✅ PASS | FR-005, FR-009–FR-014 explicitly mandate these |
| III. Dual Authentication | Shared Key Lite + Bearer Token; `SetBearerToken()` on live instance | ✅ PASS | FR-001–FR-008 cover both modes; bearer priority clarified |
| IV. Minimal Surface Area | No convenience wrappers, retry, caching; returns bool/empty; no exceptions | ✅ PASS | Library returns `true`/`false`; no retry; no new public methods |
| V. Cross-Platform Portability | Autotools build; C++20; platform-neutral business logic | ✅ PASS | No platform-specific code needed; OpenSSL and libcurl are cross-platform |
| Technical: Naming | `this->` for member access; no trailing underscore | ✅ PASS | Existing code already follows this convention |
| Technical: Async model | Each async call gets own curl handle; no sharing across threads | ✅ N/A | This feature implements sync methods only; async stubs unchanged |

**Gate result: PASS** — No violations. Proceeding to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/002-auth-and-create-table/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
include/libazure-storage-client/
├── azure-storage-client.hpp   # Public header (AzureTableClient class)
└── json.hpp                   # nlohmann-json (vendored)

src/
└── azure-storage-client.cpp   # All implementation (auth, HTTP, operations)

configure.ac                   # Autotools: deps (libcurl, openssl)
Makefile.am                    # Top-level: subdirs, pkg-config, headers
src/Makefile.am                # Library build: sources, flags, linker
```

**Structure Decision**: Single-source-file library. All implementation goes into `src/azure-storage-client.cpp`. No new files are created; the existing `azure-storage-client.hpp` public header already declares all needed methods. Internal helpers (HMAC signing, base64, header construction, URL building) are file-scoped in the `.cpp` (anonymous namespace or static functions) per constitution Principle IV (no internal details in public header).

## Complexity Tracking

> No constitution violations — table not applicable.
