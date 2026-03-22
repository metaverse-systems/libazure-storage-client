# Feature Specification: Tests, README, and LICENSE

**Feature Branch**: `005-tests-readme-license`  
**Created**: 2026-03-22  
**Status**: Draft  
**Input**: User description: "Write tests for this library, use catch2. Also generate a README.md file, and a LICENSE file for MIT license."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Run Automated Tests Against the Library (Priority: P1)

As a library developer or contributor, I want to run a test suite that exercises all public methods of the `AzureTableClient` class so that I can verify the library behaves correctly before committing changes. The tests use the Catch2 framework and run against a local Azure Table Storage emulator (Azurite), requiring no cloud credentials. I run a single build command and then execute the test binary to see pass/fail results.

**Why this priority**: Tests are the core deliverable of this feature. Without them, neither the README nor LICENSE provides development confidence. Tests protect against regressions and validate that the existing CRUD and async operations work as documented.

**Independent Test**: Build the project with tests enabled, start a local Azurite instance, and run the test binary. All tests should pass and produce a clear pass/fail summary.

**Acceptance Scenarios**:

1. **Given** a developer has cloned the repository and has Catch2 and Azurite installed, **When** they build the project, **Then** a test executable is produced alongside the library.
2. **Given** a built test executable and a running Azurite instance, **When** the developer runs the test binary, **Then** all tests execute and report pass/fail results for each test case.
3. **Given** the test suite, **When** a test case for `CreateTableIfNotExists` runs, **Then** it verifies the table is created successfully and that a second call succeeds without error (idempotent).
4. **Given** the test suite, **When** test cases for `UpsertEntity` and `GetEntity` run, **Then** they verify that an entity can be inserted and then retrieved with matching properties.
5. **Given** the test suite, **When** a test case for `DeleteEntity` runs, **Then** it verifies that after deletion, retrieving the entity returns an empty result.
6. **Given** the test suite, **When** a test case for `QueryEntities` runs, **Then** it verifies that entities matching a filter are returned and non-matching entities are excluded.
7. **Given** the test suite, **When** a test case for `BatchUpsertEntities` runs, **Then** it verifies that multiple entities are persisted in a single operation.
8. **Given** the test suite, **When** test cases for the async variants (`GetEntityAsync`, `UpsertEntityAsync`, `DeleteEntityAsync`, `QueryEntitiesAsync`, `BatchUpsertEntitiesAsync`) run, **Then** they verify that callbacks are invoked with the correct results and that the calling thread is not blocked.

---

### User Story 2 - Read the README for Quickstart Guidance (Priority: P2)

As a potential user of the library, I want to read a README.md file in the repository root that explains what the library does, how to build it, how to install it, how to run the tests, and how to use it in my own project, so that I can evaluate and adopt it quickly.

**Why this priority**: A README is the first thing any visitor sees on the repository page. Without it, discoverability and adoption are severely hampered. However, it depends on the tests existing first so the README can document how to run them.

**Independent Test**: Open the repository root and confirm a README.md file exists. Read it and verify it contains sections for project description, build instructions, test instructions, and usage examples.

**Acceptance Scenarios**:

1. **Given** a user visits the repository, **When** they open the root directory, **Then** a README.md file is present.
2. **Given** the README.md, **When** the user reads it, **Then** it contains a project description explaining that this is a C++ client library for Azure Table Storage.
3. **Given** the README.md, **When** the user reads the build section, **Then** it documents the prerequisites and step-by-step build commands.
4. **Given** the README.md, **When** the user reads the testing section, **Then** it explains how to install test dependencies, start the local emulator, and run the test suite.
5. **Given** the README.md, **When** the user reads the usage section, **Then** it shows a minimal code example demonstrating how to create a client, create a table, and upsert/get an entity.
6. **Given** the README.md, **When** the user looks for license information, **Then** it references the MIT license and points to the LICENSE file.

---

### User Story 3 - Confirm the License (Priority: P3)

As a potential user or contributor, I want a LICENSE file in the repository root containing the full MIT license text so that I can confirm the library is permissively licensed and include it in my own projects.

**Why this priority**: A LICENSE file is essential for legal clarity but is a simple, static artifact. It does not affect functionality and is the lowest-effort deliverable.

**Independent Test**: Open the repository root and confirm a LICENSE file exists. Verify it contains the standard MIT license text with the correct year and copyright holder.

**Acceptance Scenarios**:

1. **Given** a user visits the repository, **When** they open the root directory, **Then** a LICENSE file is present.
2. **Given** the LICENSE file, **When** the user reads it, **Then** it contains the full MIT license text.
3. **Given** the LICENSE file, **When** the user checks the copyright line, **Then** it includes the year 2026 and the copyright holder "Metaverse Systems".

---

### Edge Cases

- What happens when the Azurite emulator is not running? Tests that require a live endpoint should fail with clear error messages rather than hanging indefinitely.
- Tests use a single shared table created once during fixture setup. Each test case uses unique PartitionKey/RowKey values to avoid interference between tests.
- What happens when `BatchUpsertEntities` is called with more than 100 entities? The test should verify the library returns `false` without sending a request.
- What happens when `UpsertEntity` is called with a JSON entity missing `PartitionKey` or `RowKey`? The test should verify the library returns `false`.
- What happens when `GetEntity` requests a non-existent entity? The test should verify an empty JSON object is returned.
- What happens when `QueryEntities` uses a filter that matches no entities? The test should verify an empty vector is returned.

## Clarifications

### Session 2026-03-22

- Q: How should the build system discover and link Catch2? → A: pkg-config (`PKG_CHECK_MODULES([CATCH2], [catch2-with-main])`), consistent with existing libcurl/openssl discovery.
- Q: Should tests share a single table or create/destroy a unique table per test case? → A: Shared table with unique PartitionKey/RowKey per test case for speed; single fixture setup.
- Q: How should test source files be organized? → A: Single test file with Catch2 tags for grouping (sync, async, edge cases).
- Q: How should Bearer Token auth be tested when Azurite only supports Shared Key? → A: Header-level verification. Construct a Bearer Token client and call `SetBearerToken()`, then verify the `Authorization: Bearer {token}` header is emitted. Full CRUD correctness relies on Shared Key tests; Bearer Token tests validate the auth dispatch path only.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST include a test executable built using the Catch2 testing framework, with all tests in a single source file organized by Catch2 tags (`[sync]`, `[async]`, `[edge]`).
- **FR-002**: The test suite MUST cover all public synchronous methods of `AzureTableClient`: `CreateTableIfNotExists`, `GetEntity`, `UpsertEntity`, `BatchUpsertEntities`, `QueryEntities`, and `DeleteEntity`.
- **FR-003**: The test suite MUST cover all public asynchronous methods of `AzureTableClient`: `GetEntityAsync`, `UpsertEntityAsync`, `BatchUpsertEntitiesAsync`, `QueryEntitiesAsync`, and `DeleteEntityAsync`.
- **FR-004**: The test suite MUST cover both authentication modes: Shared Key (full CRUD against Azurite) and Bearer Token (header-level verification only, since Azurite does not support Bearer Token auth). Bearer Token tests MUST verify that (a) the Bearer Token constructor produces a usable client, (b) `SetBearerToken()` updates the token on a live client instance, and (c) requests include the `Authorization: Bearer {token}` header.
- **FR-005**: The test suite MUST be runnable against a local Azure Table Storage emulator (Azurite) with no cloud credentials required.
- **FR-006**: The test suite MUST include edge-case tests for invalid inputs: missing `PartitionKey`, missing `RowKey`, empty entity (`{}`), batch size exceeding 100, non-existent entity retrieval, and filter matching no entities.
- **FR-007**: The test executable MUST be buildable as part of the existing autotools build system.
- **FR-008**: A README.md file MUST exist in the repository root and contain: project description, prerequisites, build instructions, test instructions, usage examples, and license reference.
- **FR-009**: A LICENSE file MUST exist in the repository root containing the full MIT license text with "2026" as the year and "Metaverse Systems" as the copyright holder.
- **FR-010**: The README.md MUST document how to link against the library using pkg-config.

## Assumptions

- The project will continue to use the GNU Autotools build system (autoconf/automake/libtool).
- Azurite is the standard local emulator for testing. The tests assume Azurite is accessible at `http://127.0.0.1:10002/devstoreaccount1` (default Azurite Table Storage port) with the well-known development credentials.
- Catch2 v3 will be discovered via pkg-config (`catch2-with-main`), matching the project's existing pattern for libcurl and openssl. Contributors must have Catch2 v3 installed as a system package.
- The copyright holder for the MIT license is "Metaverse Systems" based on the repository organization name.
- Tests will use the Shared Key authentication mode by default since Azurite supports the well-known development storage account credentials.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All test cases pass when run against a local Azurite instance, with 100% of public API methods (11 CRUD methods plus `SetBearerToken()`) covered by at least one test. Constructor coverage is implicit via client instantiation in every test.
- **SC-002**: A new contributor can build the project and run all tests within 5 minutes by following the README instructions alone.
- **SC-003**: The test suite completes execution in under 30 seconds on a standard development machine.
- **SC-004**: The README.md contains all required sections (description, prerequisites, build, test, usage, license) and is readable without prior project knowledge.
- **SC-005**: The LICENSE file is recognized as MIT by standard license detection tools (e.g., GitHub's license detection).
