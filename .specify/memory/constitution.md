<!--
  Sync Impact Report
  ==================
  Version change: N/A → 1.0.0 (initial adoption)
  Modified principles: N/A (initial)
  Added sections:
    - Core Principles (5 principles)
    - Technical Constraints
    - Development Workflow
    - Governance
  Removed sections: N/A
  Templates requiring updates:
    - .specify/templates/plan-template.md        ✅ compatible (no changes needed)
    - .specify/templates/spec-template.md         ✅ compatible (no changes needed)
    - .specify/templates/tasks-template.md        ✅ compatible (no changes needed)
  Follow-up TODOs: none
-->

# libazure-storage-client Constitution

## Core Principles

### I. Standalone Library — Zero Framework Coupling

This library MUST remain a self-contained, reusable C++ shared library
with no dependency on any application framework, game engine, or
orchestration layer. Consumers link against the `.so`/`.dll` and call
the public `AzureTableClient` API — nothing else is required.

- Every public symbol MUST be usable without framework-specific
  initialization or lifecycle hooks.
- The only external runtime dependencies are libcurl, OpenSSL, and
  nlohmann-json. No additional dependencies may be introduced without
  a constitution amendment.
- The library MUST NOT pull in or assume the presence of any
  application-level logging, configuration, or dependency-injection
  framework.

### II. Azure REST API Fidelity

The library MUST faithfully implement the Azure Table Storage REST API
as documented by Microsoft, not an approximation or subset that
"works in practice."

- HTTP requests MUST include the canonical headers: `x-ms-version`,
  `x-ms-date`, `Accept`, and `Content-Type` with the exact values
  specified in the public API contract.
- Batch operations MUST use the Entity Group Transaction multipart
  format. All entities in a batch MUST share the same PartitionKey.
  Batches exceeding 100 operations MUST be rejected by the library
  (return `false`), not silently split.
- Query pagination MUST be handled transparently: continuation tokens
  (`x-ms-continuation-NextPartitionKey`,
  `x-ms-continuation-NextRowKey`) MUST be followed until exhausted.
- `CreateTableIfNotExists` MUST treat HTTP 409 (Conflict) as success.
- `DeleteEntity` MUST use `If-Match: *` for unconditional delete.

### III. Dual Authentication — Shared Key and Bearer Token

The library MUST support exactly two authentication modes. Both MUST
be fully functional and independently usable.

- **Shared Key**: construct the `StringToSign` per the Azure Table
  Storage spec, HMAC-SHA256-sign it with the base64-decoded account
  key (via OpenSSL), and emit
  `Authorization: SharedKey {account}:{base64-signature}`.
- **Bearer Token (OAuth2 / Entra ID)**: emit
  `Authorization: Bearer {token}`. The caller owns token acquisition
  and refresh; the library MUST only store and send the token.
- `SetBearerToken()` MUST allow updating the token on a live client
  instance without reconstruction.
- Adding a third authentication mode requires a constitution
  amendment.

### IV. Minimal Surface Area

The public API MUST expose only what is specified in the API contract.
No convenience wrappers, retry logic, connection pooling, caching, or
speculative features.

- YAGNI: features MUST NOT be added "just in case." Every public
  method MUST trace back to a documented API operation.
- Error handling is the caller's responsibility. The library returns
  success/failure (`bool`) or empty results; it MUST NOT throw
  exceptions, retry failed requests, or implement backoff.
- Internal implementation details (curl handles, HMAC helpers, header
  construction) MUST NOT leak into the public header.

### V. Cross-Platform Portability

The library MUST build and produce a usable shared library on both
Linux (GCC/Clang) and Windows (MinGW cross-compilation).

- The build system is Autotools (`configure.ac` + `Makefile.am`).
  No alternative build systems (CMake, Meson, etc.) unless adopted
  via constitution amendment.
- The C++ standard is C++20. Platform-specific `#ifdef` blocks are
  acceptable only for symbol export macros or OS-level API
  differences; business logic MUST remain platform-neutral.
- Installed artifacts: shared library, public headers under
  `${prefix}/include/azure-storage-client/`, and a `.pc` file for
  pkg-config discovery.

## Technical Constraints

- **Language**: C++20
- **Build system**: Autotools (`configure.ac`, `Makefile.am`,
  `src/Makefile.am`)
- **Output**: `libazure-storage-client.so` (Linux),
  `libazure-storage-client.dll` (Windows/MinGW)
- **Dependencies** (exhaustive):
  - libcurl — HTTP/HTTPS transport
  - OpenSSL — HMAC-SHA256 signing, base64 encode/decode
  - nlohmann-json — JSON serialization/deserialization
- **Async model**: each `…Async` variant spawns a `std::thread`,
  invokes the callback on that thread. Each async call gets its own
  `curl_easy` handle; handles MUST NOT be shared across threads.
- **pkg-config**: a `.pc` file MUST be installed so downstream
  projects can discover the library via `pkg-config`.

## Development Workflow

- **Local emulator**: Azurite on
  `http://127.0.0.1:10002/devstoreaccount1` with the well-known
  dev credentials (`devstoreaccount1` / the standard base64 key).
  Azurite supports Shared Key auth only.
- **Testing against Azurite**: all entity CRUD, batch, and query
  operations MUST be verifiable against Azurite before merging.
- **Header installation**: public headers install to
  `${prefix}/include/azure-storage-client/`. Verify with
  `make install DESTDIR=...` during development.
- **Cross-compilation check**: a MinGW cross-build MUST succeed
  before tagging a release.

## Governance

This constitution is the authoritative source of project-level
constraints for libazure-storage-client. All implementation plans,
feature specs, and task lists MUST be validated against these
principles before work begins.

- **Amendment procedure**: any change to a Core Principle or the
  addition/removal of a dependency requires a pull request titled
  `constitution: <summary>`, explicit rationale in the PR
  description, and maintainer approval.
- **Versioning policy**: the constitution follows semantic versioning.
  MAJOR for principle removals or incompatible redefinitions, MINOR
  for new principles or materially expanded guidance, PATCH for
  clarifications and wording fixes.
- **Compliance review**: every implementation plan (`plan.md`) MUST
  include a "Constitution Check" section that maps each proposed
  change against these principles and flags any violations before
  Phase 0 research begins.

**Version**: 1.0.0 | **Ratified**: 2026-03-22 | **Last Amended**: 2026-03-22
