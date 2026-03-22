# Research: Tests, README, and LICENSE

**Feature**: 005-tests-readme-license  
**Date**: 2026-03-22

## 1. Catch2 v3 Discovery via pkg-config

**Decision**: Use `PKG_CHECK_MODULES([CATCH2], [catch2-with-main])` in `configure.ac`.

**Rationale**: 
- `catch2-with-main` is the standard Catch2 v3 pkg-config module that provides a built-in `main()` function, eliminating the need for a custom test runner main.
- This follows the project's existing pattern: libcurl uses `PKG_CHECK_MODULES([LIBCURL], [libcurl])` and OpenSSL uses `PKG_CHECK_MODULES([OPENSSL], [openssl])`.
- Catch2 should be made optional with `AM_CONDITIONAL([HAVE_CATCH2], ...)` so `./configure` succeeds even when Catch2 is not installed (library consumers do not need it).

**Alternatives considered**:
- Vendored single-header: rejected because it breaks the pkg-config consistency pattern and bloats the repository.
- `catch2` module (without main): rejected because it requires writing a custom `main()` — unnecessary complexity.

## 2. Autotools Test Integration Pattern

**Decision**: Add `tests/` as a conditional `SUBDIRS` entry, using `check_PROGRAMS` for the test executable.

**Rationale**:
- `check_PROGRAMS` means the test binary is built only with `make check`, not installed.
- Linking against `$(top_builddir)/src/libazure-storage-client.la` uses the libtool archive directly — no system install required to run tests.
- `-pthread` must be added to LDFLAGS since the library's async methods spawn `std::thread`.

**Key files**:
- `configure.ac`: add `PKG_CHECK_MODULES([CATCH2], [catch2-with-main], [have_catch2=yes], [have_catch2=no])`, `AM_CONDITIONAL`, and `tests/Makefile` to `AC_CONFIG_FILES`.
- `Makefile.am`: conditionally add `tests` to `SUBDIRS`.
- `tests/Makefile.am`: define `check_PROGRAMS`, sources, CXXFLAGS, LDADD, LDFLAGS.

**Alternatives considered**:
- Tests in `src/`: rejected — mixes library and test concerns, pollutes `src/Makefile.am`.
- `noinst_PROGRAMS` instead of `check_PROGRAMS`: rejected — `check_PROGRAMS` only builds on `make check`, which is the Autotools convention.

## 3. Catch2 v3 Header and API

**Decision**: Use `#include <catch2/catch_all.hpp>` (Catch2 v3 unified header).

**Rationale**: 
- Catch2 v3 replaced v2's `<catch2/catch.hpp>` with `<catch2/catch_all.hpp>`.
- With `catch2-with-main` linked, no `#define CATCH_CONFIG_MAIN` is needed — main is provided by the library.

**Test organization**:
- `TEST_CASE("...", "[sync]")` for synchronous method tests.
- `TEST_CASE("...", "[async]")` for asynchronous method tests.
- `TEST_CASE("...", "[edge]")` for edge case / validation tests.

## 4. Test Isolation — Shared Table Strategy

**Decision**: One table created before tests, unique PartitionKey/RowKey per test case.

**Rationale**:
- Avoids repeated table creation/deletion overhead against Azurite (each CreateTable is an HTTP round-trip).
- Catch2 event listeners or a top-level `TEST_CASE` with early execution can create the table once.
- Each test uses a unique PartitionKey (e.g., based on test name or UUID) to prevent cross-test data interference.

**Alternatives considered**:
- Per-test table: rejected — significantly slower, table creation is the most expensive Azurite operation.
- Catch2 `SECTION` isolation: rejected — sections share the same `TEST_CASE` scope, which limits test independence.

## 5. Azurite Well-Known Credentials

**Decision**: Hardcode the Azurite development credentials in the test file.

**Rationale**:
- The well-known Azurite development credentials are public, documented by Microsoft, and intended for local testing:
  - Account name: `devstoreaccount1`
  - Account key: `Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==`
  - Table endpoint: `http://127.0.0.1:10002/devstoreaccount1`
- These are not real credentials and pose no security risk.

## 6. README Structure

**Decision**: Standard README.md with 6 sections: description, prerequisites, build, test, usage, license.

**Rationale**:
- Matches FR-008 and FR-010 requirements from the spec.
- Usage section includes a complete code example showing SharedKey client construction, table creation, entity upsert, and entity retrieval.
- Build section documents `autoreconf -fi && ./configure && make && sudo make install`.
- Test section documents `npm install -g azurite && azurite-table &` followed by `make check`.
- pkg-config usage documented: `pkg-config --cflags --libs azure-storage-client`.

## 7. MIT License

**Decision**: Standard MIT license text with "2026 Metaverse Systems" copyright line.

**Rationale**: Spec FR-009 explicitly requires MIT license with year 2026 and copyright holder "Metaverse Systems". The standard SPDX-compliant MIT text will be used for GitHub license detection compatibility (SC-005).
