# Tasks: Tests, README, and LICENSE

**Input**: Design documents from `/specs/005-tests-readme-license/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/public-api.md, quickstart.md

**Tests**: Explicitly requested — User Story 1 is the test suite itself.

**Organization**: Tasks grouped by user story for independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Exact file paths included in descriptions

---

## Phase 1: Setup (Build System Integration)

**Purpose**: Extend the Autotools build system to support Catch2 tests

- [ ] T001 Add PKG_CHECK_MODULES for catch2-with-main, AM_CONDITIONAL, and tests/Makefile to AC_CONFIG_FILES in configure.ac
- [ ] T002 Add conditional SUBDIRS entry for tests/ in Makefile.am

---

## Phase 2: Foundational (Test Infrastructure)

**Purpose**: Create the test build target and shared fixture that all test cases depend on

**⚠️ CRITICAL**: No test case tasks can begin until this phase is complete

- [ ] T003 Create tests/Makefile.am with check_PROGRAMS, CXXFLAGS, LDADD, and LDFLAGS for test_azure_table_client
- [ ] T004 Create tests/test_azure_table_client.cpp with Catch2 includes, Azurite connection constants, and shared table fixture (CreateTableIfNotExists in a Catch2 event listener)

**Checkpoint**: `autoreconf -fi && ./configure && make check` compiles the test executable and the fixture creates the shared table against Azurite

---

## Phase 3: User Story 1 — Automated Test Suite (Priority: P1) 🎯 MVP

**Goal**: Test all 11 public methods of AzureTableClient plus edge cases, organized by Catch2 tags

**Independent Test**: Start Azurite, run `make check` — all test cases pass with clear output

### Sync CRUD Tests

- [ ] T005 [P] [US1] Add TEST_CASE for CreateTableIfNotExists (idempotent — second call returns true) with tag [sync] in tests/test_azure_table_client.cpp
- [ ] T006 [P] [US1] Add TEST_CASE for UpsertEntity and GetEntity (insert entity, retrieve, verify properties match) with tag [sync] in tests/test_azure_table_client.cpp
- [ ] T007 [P] [US1] Add TEST_CASE for DeleteEntity (upsert entity, delete, verify GetEntity returns empty JSON) with tag [sync] in tests/test_azure_table_client.cpp
- [ ] T008 [P] [US1] Add TEST_CASE for QueryEntities (upsert multiple entities with different PartitionKeys, query with filter, verify matching entities returned and non-matching excluded) with tag [sync] in tests/test_azure_table_client.cpp
- [ ] T009 [P] [US1] Add TEST_CASE for BatchUpsertEntities (batch of 3 entities, verify all retrievable via GetEntity) with tag [sync] in tests/test_azure_table_client.cpp

### Async CRUD Tests

- [ ] T010 [P] [US1] Add TEST_CASE for GetEntityAsync (upsert entity first, call async get with callback capturing result into promise/future, verify properties match) with tag [async] in tests/test_azure_table_client.cpp
- [ ] T011 [P] [US1] Add TEST_CASE for UpsertEntityAsync (call async upsert with callback capturing bool, verify true returned and entity retrievable via sync GetEntity) with tag [async] in tests/test_azure_table_client.cpp
- [ ] T012 [P] [US1] Add TEST_CASE for DeleteEntityAsync (upsert entity first, call async delete with callback, verify true returned and GetEntity returns empty) with tag [async] in tests/test_azure_table_client.cpp
- [ ] T013 [P] [US1] Add TEST_CASE for QueryEntitiesAsync (upsert entities, call async query with callback capturing vector, verify correct entities returned) with tag [async] in tests/test_azure_table_client.cpp
- [ ] T014 [P] [US1] Add TEST_CASE for BatchUpsertEntitiesAsync (call async batch upsert with callback capturing bool, verify true returned and all entities retrievable) with tag [async] in tests/test_azure_table_client.cpp

### Edge Case Tests

- [ ] T015 [P] [US1] Add TEST_CASE for UpsertEntity with missing PartitionKey (verify returns false without network call) with tag [edge] in tests/test_azure_table_client.cpp
- [ ] T016 [P] [US1] Add TEST_CASE for UpsertEntity with missing RowKey (verify returns false without network call) with tag [edge] in tests/test_azure_table_client.cpp
- [ ] T017 [P] [US1] Add TEST_CASE for BatchUpsertEntities with >100 entities (verify returns false without network call) with tag [edge] in tests/test_azure_table_client.cpp
- [ ] T018 [P] [US1] Add TEST_CASE for GetEntity with non-existent entity (verify returns empty JSON object) with tag [edge] in tests/test_azure_table_client.cpp
- [ ] T019 [P] [US1] Add TEST_CASE for QueryEntities with filter matching no entities (verify returns empty vector) with tag [edge] in tests/test_azure_table_client.cpp
- [ ] T020 [P] [US1] Add TEST_CASE for UpsertEntity with empty entity `{}` (verify returns false without network call) with tag [edge] in tests/test_azure_table_client.cpp

### Auth Dispatch Tests

- [ ] T021 [P] [US1] Add TEST_CASE for Bearer Token constructor and SetBearerToken() — construct a BearerToken client, call SetBearerToken() with a new token, then attempt CreateTableIfNotExists against Azurite (expected: request sent with Authorization: Bearer header; Azurite rejects with 401/403 — assert the call returns false but does not crash) with tag [auth] in tests/test_azure_table_client.cpp

### Pagination Test

- [ ] T022 [P] [US1] Add TEST_CASE for QueryEntities pagination — upsert >1000 entities with the same PartitionKey, call QueryEntities with no filter, verify all entities are returned (exercises continuation token handling per Constitution Principle II) with tag [sync] in tests/test_azure_table_client.cpp

**Checkpoint**: `make check` passes all sync, async, auth, and edge-case tests. Run `./tests/test_azure_table_client "[sync]"`, `"[async]"`, `"[auth]"`, and `"[edge]"` individually to confirm tags work.

---

## Phase 4: User Story 2 — README Documentation (Priority: P2)

**Goal**: Provide a complete README.md so new users can build, test, and adopt the library

**Independent Test**: Read README.md and follow instructions to build, install, and run tests from a clean clone

- [ ] T023 [US2] Create README.md in repository root with project description, prerequisites section, build instructions (autoreconf -fi, ./configure, make, make install), testing section (Catch2 install, Azurite start, make check), usage code example (SharedKey client construction, CreateTableIfNotExists, UpsertEntity, GetEntity), pkg-config integration (pkg-config --cflags --libs azure-storage-client), and license reference

**Checkpoint**: README.md exists, contains all 6 required sections, and is readable by someone with no prior knowledge of the project

---

## Phase 5: User Story 3 — MIT License (Priority: P3)

**Goal**: Add a LICENSE file for legal clarity and GitHub license detection

**Independent Test**: Open LICENSE file, confirm MIT text with "2026 Metaverse Systems" copyright

- [ ] T024 [P] [US3] Create LICENSE file in repository root with full MIT license text, copyright line "Copyright (c) 2026 Metaverse Systems"

**Checkpoint**: LICENSE file exists and would be detected as MIT by GitHub's license detection

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final validation across all user stories

- [ ] T025 Run autoreconf -fi, ./configure, make, make check end-to-end against Azurite and verify all tests pass. Measure with `time make check` and confirm wall-clock time < 30 seconds (SC-003)
- [ ] T026 Run quickstart.md verification checklist to confirm all items pass. After push, verify GitHub detects LICENSE as MIT (SC-005)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — BLOCKS all test cases
- **User Story 1 (Phase 3)**: Depends on Phase 2 (test infrastructure must exist)
- **User Story 2 (Phase 4)**: Depends on Phase 3 (README documents test instructions that must exist)
- **User Story 3 (Phase 5)**: No dependencies on other stories — can run in parallel with Phase 3 or 4
- **Polish (Phase 6)**: Depends on all phases complete

### User Story Dependencies

- **US1 (Tests)**: Depends on Foundational (Phase 2) only
- **US2 (README)**: Depends on US1 completion (documents test instructions)
- **US3 (LICENSE)**: Independent — can run in parallel with any phase after Setup

### Within User Story 1

All test cases (T005–T022) are marked [P] because they write to different PartitionKeys in a shared table and modify different TEST_CASE blocks in the same file. They can be authored in any order but all go into the same file.

### Parallel Opportunities

- T001 and T002 are sequential (T002 depends on the HAVE_CATCH2 conditional from T001)
- T003 and T004 are sequential (T004 depends on the build target from T003)
- T005–T022 are all parallelizable within Phase 3 (independent test cases, unique PartitionKeys)
- T024 (LICENSE) can run in parallel with any Phase 3+ task

---

## Parallel Example: User Story 1

```text
# After Phase 2 is complete, launch all sync tests together:
T005: CreateTableIfNotExists test [sync]
T006: UpsertEntity + GetEntity test [sync]
T007: DeleteEntity test [sync]
T008: QueryEntities test [sync]
T009: BatchUpsertEntities test [sync]
T022: QueryEntities pagination [sync]

# And all async tests together:
T010: GetEntityAsync test [async]
T011: UpsertEntityAsync test [async]
T012: DeleteEntityAsync test [async]
T013: QueryEntitiesAsync test [async]
T014: BatchUpsertEntitiesAsync test [async]

# And all edge-case tests together:
T015: Missing PartitionKey [edge]
T016: Missing RowKey [edge]
T017: Batch >100 entities [edge]
T018: Non-existent entity [edge]
T019: Empty query result [edge]
T020: Empty entity {} [edge]

# And auth dispatch test:
T021: Bearer Token + SetBearerToken [auth]
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (configure.ac + Makefile.am changes)
2. Complete Phase 2: Foundational (tests/Makefile.am + test file scaffold)
3. Complete Phase 3: User Story 1 (all 18 test cases)
4. **STOP and VALIDATE**: `make check` — all tests pass against Azurite
5. This is a fully functional test suite — usable without README or LICENSE

### Incremental Delivery

1. Setup + Foundational → build system ready for tests
2. Add User Story 1 → test suite works → **MVP delivered**
3. Add User Story 2 → README documents everything → project is discoverable
4. Add User Story 3 → LICENSE added → project is legally clear
5. Polish → end-to-end validation confirms everything works together
