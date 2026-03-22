# Tasks: Synchronous CRUD Operations

**Input**: Design documents from `/specs/003-sync-crud-operations/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/public-api.md, quickstart.md

**Tests**: Not requested — no test tasks included.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story. All implementation changes are in `src/azure-storage-client.cpp` (single implementation file).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- All file paths relative to repository root

---

## Phase 1: Setup

**Purpose**: Project initialization and structure

No setup tasks needed — project structure, build system, header declarations, and stub implementations are already in place from features 001 and 002.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Refactor internal HTTP helper and add shared utilities that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T001 Add `HttpResponse` struct (`long status`, `std::string body`, `std::map<std::string, std::string> headers`) and `header_callback` static function (parses `Name: value\r\n` lines into map with lowercase keys) to anonymous namespace in src/azure-storage-client.cpp
- [ ] T002 Refactor `perform_request` to return `HttpResponse` instead of `long` — set `CURLOPT_HEADERFUNCTION` to `header_callback`, set `CURLOPT_HEADERDATA` to `&response.headers`, populate `response.body` via existing `write_callback`, populate `response.status` via `CURLINFO_RESPONSE_CODE` in src/azure-storage-client.cpp
- [ ] T003 Update `CreateTableIfNotExists` to use `response.status` from returned `HttpResponse` (replace `long status = perform_request(...)` with `auto response = perform_request(...)` and use `response.status`) in src/azure-storage-client.cpp
- [ ] T004 [P] Add `escape_odata_key` helper to anonymous namespace — double single quotes (`'` → `''`) then URL-encode via `curl_easy_escape(nullptr, ...)` (nullptr valid since libcurl 7.82.0) in src/azure-storage-client.cpp
- [ ] T005 Add `build_entity_url` helper to anonymous namespace — constructs `{endpoint}/{tableName}(PartitionKey='{escapedPK}',RowKey='{escapedRK}')` using `escape_odata_key` in src/azure-storage-client.cpp

**Checkpoint**: Foundation ready — `perform_request` returns full response data, entity URL helpers available. User story implementation can now begin.

---

## Phase 3: User Story 1 — Get a Single Entity (Priority: P1) 🎯 MVP

**Goal**: Retrieve a single entity by PartitionKey and RowKey, returned as a JSON object.

**Independent Test**: Against Azurite with a pre-created table and known entity, call `GetEntity` and verify the returned JSON contains the expected PartitionKey, RowKey, and property values.

### Implementation for User Story 1

- [ ] T006 [US1] Implement `GetEntity` — build entity URL via `build_entity_url`, add headers (`Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`, auth via bearer or SharedKey using same dispatch pattern as `CreateTableIfNotExists`), call `perform_request` with GET method, parse `response.body` as `nlohmann::json` on HTTP 200, return empty JSON object on any other status in src/azure-storage-client.cpp

**Checkpoint**: `GetEntity` is functional. Can retrieve any entity by key from Azure Table Storage.

---

## Phase 4: User Story 2 — Upsert a Single Entity (Priority: P1) 🎯 MVP

**Goal**: Create or replace an entity using insert-or-replace semantics via HTTP PUT, returning success/failure.

**Independent Test**: Against Azurite, upsert an entity then retrieve it with `GetEntity` to confirm persistence. Upsert the same entity with modified properties and retrieve again to confirm replacement.

### Implementation for User Story 2

- [ ] T007 [US2] Implement `UpsertEntity` — check that entity contains `"PartitionKey"` and `"RowKey"` as strings (return `false` if missing); extract values, build entity URL via `build_entity_url`, add headers (`Content-Type: application/json;odata=nometadata`, `Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`, auth dispatch), call `perform_request` with PUT method and `entity.dump()` as body, return `true` on HTTP 204, `false` otherwise in src/azure-storage-client.cpp

**Checkpoint**: `UpsertEntity` is functional. Can create and replace entities. Combined with US1, basic read/write cycle works.

---

## Phase 5: User Story 3 — Delete an Entity (Priority: P2)

**Goal**: Remove an entity by key with unconditional delete (`If-Match: *`), returning success/failure.

**Independent Test**: Against Azurite, create an entity via `UpsertEntity`, delete it via `DeleteEntity`, then confirm `GetEntity` returns an empty result.

### Implementation for User Story 3

- [ ] T008 [US3] Implement `DeleteEntity` — build entity URL via `build_entity_url`, add headers (`If-Match: *`, `Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`, auth dispatch), call `perform_request` with DELETE method and empty body, return `true` on HTTP 204, `false` otherwise in src/azure-storage-client.cpp

**Checkpoint**: `DeleteEntity` is functional. Full CRUD lifecycle (create, read, delete) is complete.

---

## Phase 6: User Story 4 — Query Entities with Filter (Priority: P2)

**Goal**: Retrieve a set of entities matching an OData filter, with transparent pagination via continuation tokens.

**Independent Test**: Against Azurite, insert several entities with varying properties, call `QueryEntities` with a filter matching a subset, and verify only matching entities are returned.

### Implementation for User Story 4

- [ ] T009 [US4] Implement `QueryEntities` — build base query URL as `{endpoint}/{tableName}()`, append `?$filter={curl_easy_escape(filter)}` if filter is non-empty, add headers (`Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`, auth dispatch), call `perform_request` with GET in a loop (rebuilding `x-ms-date` and `Authorization` headers for each iteration since SharedKey signatures are time-dependent): parse `response.body` JSON and extract `"value"` array into results vector, check `response.headers` for `x-ms-continuation-nextpartitionkey` and `x-ms-continuation-nextrowkey` (lowercase), if present append `NextPartitionKey` and `NextRowKey` query parameters to next request URL and repeat, otherwise break; return empty vector on any error in src/azure-storage-client.cpp

**Checkpoint**: `QueryEntities` is functional with full pagination support. All single-entity and multi-entity read operations work.

---

## Phase 7: User Story 5 — Batch Upsert Entities (Priority: P3)

**Goal**: Atomically upsert up to 100 entities sharing the same PartitionKey in a single batch transaction.

**Independent Test**: Against Azurite, batch upsert several entities sharing the same PartitionKey, then query the table to confirm all entities were persisted.

### Implementation for User Story 5

- [ ] T010 [US5] Implement `BatchUpsertEntities` — return `true` immediately if entities vector is empty (no-op); return `false` immediately if `entities.size() > 100` (client-side guard per constitution); build multipart/mixed batch body with outer `batch_asc` boundary and inner `changeset_asc` boundary, each changeset entry is a PUT request line (`PUT {entity_url} HTTP/1.1`) with `Content-Type: application/json` and `Accept: application/json;odata=nometadata` headers followed by entity JSON body; POST to `{endpoint}/$batch` with `Content-Type: multipart/mixed; boundary=batch_asc` header plus standard headers (`x-ms-date`, `x-ms-version: 2021-12-02`, auth dispatch); return `true` if HTTP status is 202 and response body does not contain `HTTP/1.1 4` or `HTTP/1.1 5` error lines, `false` otherwise in src/azure-storage-client.cpp

**Checkpoint**: All five synchronous CRUD operations are implemented and functional.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Validate the complete implementation builds and works end-to-end

- [ ] T011 Build project with `make` and verify zero compilation errors or warnings
- [ ] T012 Run quickstart.md end-to-end validation against Azurite — execute each operation (CreateTableIfNotExists, UpsertEntity, GetEntity, QueryEntities, DeleteEntity, BatchUpsertEntities) and confirm expected output. Also verify at least one error case per method: `GetEntity` on non-existent table returns `{}`, `DeleteEntity` on missing entity returns `false`, `UpsertEntity` with missing PartitionKey returns `false`, `BatchUpsertEntities` with >100 entities returns `false`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: N/A — already complete from prior features
- **Foundational (Phase 2)**: No external dependencies — BLOCKS all user stories
- **User Stories (Phase 3–7)**: All depend on Foundational phase completion
  - US1 (GetEntity) and US2 (UpsertEntity) can proceed in any order after Phase 2
  - US3 (DeleteEntity) and US4 (QueryEntities) can proceed in any order after Phase 2
  - US5 (BatchUpsertEntities) can proceed after Phase 2
  - All user stories are implementation-independent (each modifies a different method body)
- **Polish (Phase 8)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 — GetEntity (P1)**: Can start after Phase 2. No dependencies on other stories.
- **US2 — UpsertEntity (P1)**: Can start after Phase 2. No implementation dependency on US1 (test verification uses GetEntity but implementation is independent).
- **US3 — DeleteEntity (P2)**: Can start after Phase 2. No implementation dependencies.
- **US4 — QueryEntities (P2)**: Can start after Phase 2. No implementation dependencies.
- **US5 — BatchUpsertEntities (P3)**: Can start after Phase 2. Uses `build_entity_url` and `escape_odata_key` from Phase 2 for per-entity URLs.

### Within Each User Story

All user stories in this feature have a single implementation task per method. No internal ordering needed.

### Parallel Opportunities

- T004 (`escape_odata_key`) can run in parallel with T001–T003 (independent helper, different part of anonymous namespace)
- After Phase 2, all five user story phases (3–7) are implementation-independent — each modifies a different method body in the same file

---

## Parallel Example: Foundational Phase

```bash
# Sequential chain (T001 → T002 → T003):
Task: "Add HttpResponse struct and header_callback — T001"
Task: "Refactor perform_request to return HttpResponse — T002"
Task: "Update CreateTableIfNotExists to use response.status — T003"

# Can run in parallel with above:
Task: "Add escape_odata_key helper — T004"

# After T004 completes:
Task: "Add build_entity_url helper — T005"
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2)

1. Complete Phase 2: Foundational (`HttpResponse` refactor + URL helpers)
2. Complete Phase 3: US1 — GetEntity
3. Complete Phase 4: US2 — UpsertEntity
4. **STOP and VALIDATE**: Upsert an entity then GetEntity to confirm read/write cycle works
5. Library is usable for basic entity storage

### Incremental Delivery

1. Complete Foundational → Internal helpers ready
2. Add US1 (GetEntity) + US2 (UpsertEntity) → Basic read/write works (MVP!)
3. Add US3 (DeleteEntity) → Full CRUD lifecycle
4. Add US4 (QueryEntities) → Multi-entity reads with pagination
5. Add US5 (BatchUpsertEntities) → Bulk write performance
6. Each story adds capability without breaking previous stories

---

## Notes

- All implementation is in a single file (`src/azure-storage-client.cpp`) — no cross-file conflicts
- No test tasks included (not requested in specification)
- Auth dispatch pattern (bearer priority, SharedKey fallback) is copied from `CreateTableIfNotExists` into each new method
- `header_callback` stores lowercase header names for case-insensitive lookup of continuation tokens
- Batch boundary IDs use simple static strings (`batch_asc`, `changeset_asc`) — Azure is not strict about format
- Commit after each phase completion for clean git history
