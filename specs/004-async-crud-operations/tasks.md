# Tasks: Asynchronous CRUD Operations

**Input**: Design documents from `/specs/004-async-crud-operations/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/public-api.md, quickstart.md

**Tests**: Not requested — no test tasks generated.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Story Mapping

Tasks group the five spec user stories by priority into three implementation phases:

| Tasks Label | Spec User Stories | Priority |
|-------------|-------------------|----------|
| [US1] (Phase 3) | US1 — Get Entity Async + US2 — Upsert Entity Async | P1 |
| [US2] (Phase 4) | US3 — Delete Entity Async + US4 — Query Entities Async | P2 |
| [US3] (Phase 5) | US5 — Batch Upsert Entities Async | P3 |

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Header**: `include/libazure-storage-client/azure-storage-client.hpp` (no changes)
- **Source**: `src/azure-storage-client.cpp` (all async implementations)
- **Build**: `src/Makefile.am` (pthread flag)

---

## Phase 1: Setup

**Purpose**: Build system changes required for `std::thread` support

- [X] T001 Add `-pthread` to compiler and linker flags in src/Makefile.am
- [X] T002 Add `#include <thread>` to src/azure-storage-client.cpp

**Checkpoint**: Library compiles with threading support. No behavioral changes yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: No foundational tasks — the sync methods, CURL infrastructure, and class structure already exist. Build system changes in Phase 1 unblock all user stories.

**⚠️ CRITICAL**: Phase 1 must complete before any user story implementation.

**Checkpoint**: Foundation ready — async method implementation can begin.

---

## Phase 3: User Story 1 — Get & Upsert Entity Async (Priority: P1) 🎯 MVP

**Goal**: Implement `GetEntityAsync` and `UpsertEntityAsync` to prove the copy-at-dispatch threading pattern works for both reads and writes.

**Independent Test**: Against Azurite, call `GetEntityAsync` with a callback that captures the result. Wait for the callback and verify the returned JSON matches the entity. Call `UpsertEntityAsync` with a callback, wait, then verify with sync `GetEntity`.

### Implementation for User Story 1

- [X] T003 [P] [US1] Implement GetEntityAsync with copy-at-dispatch pattern in src/azure-storage-client.cpp
- [X] T004 [P] [US1] Implement UpsertEntityAsync with copy-at-dispatch pattern in src/azure-storage-client.cpp

Each implementation must:
1. Copy `*this` into a local `AzureTableClient` variable
2. Capture the copy and all parameters by value into a lambda
3. Spawn a `std::thread` with the lambda and immediately `.detach()` it
4. Inside the lambda: call the corresponding sync method on the copy
5. Check if callback is non-empty before invoking it with the result

**Checkpoint**: Async get and upsert work against Azurite. The threading pattern (copy-at-dispatch, detached thread, callback invocation) is proven. This is the MVP.

---

## Phase 4: User Story 2 — Delete & Query Entity Async (Priority: P2)

**Goal**: Implement `DeleteEntityAsync` and `QueryEntitiesAsync` to complete the full single-entity async lifecycle and enable non-blocking bulk reads.

**Independent Test**: Against Azurite, create an entity, call `DeleteEntityAsync`, wait for callback, verify entity is gone. Insert several entities, call `QueryEntitiesAsync` with a filter, wait for callback, verify matching entities returned.

### Implementation for User Story 2

- [X] T005 [P] [US2] Implement DeleteEntityAsync with copy-at-dispatch pattern in src/azure-storage-client.cpp
- [X] T006 [P] [US2] Implement QueryEntitiesAsync with copy-at-dispatch pattern in src/azure-storage-client.cpp

Same implementation pattern as Phase 3 tasks. `QueryEntitiesAsync` delegates to `QueryEntities` which already handles continuation tokens.

**Checkpoint**: All single-entity async operations plus async query work. Full async CRUD lifecycle is functional.

---

## Phase 5: User Story 3 — Batch Upsert Entities Async (Priority: P3)

**Goal**: Implement `BatchUpsertEntitiesAsync` for high-throughput non-blocking batch writes.

**Independent Test**: Against Azurite, call `BatchUpsertEntitiesAsync` with several entities sharing the same PartitionKey. Wait for callback, then query the table to confirm all entities persisted.

### Implementation for User Story 3

- [X] T007 [US3] Implement BatchUpsertEntitiesAsync with copy-at-dispatch pattern in src/azure-storage-client.cpp

Same implementation pattern. Delegates to `BatchUpsertEntities` which already handles the batch transaction protocol and the 100-entity limit validation.

**Checkpoint**: All five async methods are implemented. Full feature is complete.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Build verification and validation across all async methods

- [X] T008 Run `make clean && make` to verify library compiles with all async implementations
- [X] T009 Run quickstart.md validation against Azurite to verify all five async methods end-to-end

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: N/A — no tasks
- **User Story 1 (Phase 3)**: Depends on Phase 1 (T001, T002)
- **User Story 2 (Phase 4)**: Depends on Phase 1 (T001, T002) — independent of Phase 3
- **User Story 3 (Phase 5)**: Depends on Phase 1 (T001, T002) — independent of Phases 3–4
- **Polish (Phase 6)**: Depends on all user stories being complete (Phases 3–5)

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Phase 1. No dependencies on other stories.
- **User Story 2 (P2)**: Can start after Phase 1. No dependencies on other stories.
- **User Story 3 (P3)**: Can start after Phase 1. No dependencies on other stories.
- All user stories are **fully independent** — they modify different function bodies in the same file but follow an identical pattern.

### Within Each User Story

- All tasks within a story follow the same copy-at-dispatch pattern
- Tasks marked [P] within a story can run in parallel (different function bodies, no shared code)

### Parallel Opportunities

- T001 and T002 are sequential (T002 depends on T001 for compilation)
- T003 and T004 can run in parallel (different function bodies)
- T005 and T006 can run in parallel (different function bodies)
- T003–T007 (all five async methods) could theoretically all run in parallel after Phase 1, but are sequenced by story priority for incremental delivery

---

## Parallel Example: User Story 1

```bash
# Both tasks modify different function bodies in src/azure-storage-client.cpp:
Task T003: "Implement GetEntityAsync..."    → modifies GetEntityAsync function body
Task T004: "Implement UpsertEntityAsync..." → modifies UpsertEntityAsync function body
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001, T002)
2. Complete Phase 3: User Story 1 (T003, T004)
3. **STOP and VALIDATE**: Test GetEntityAsync and UpsertEntityAsync against Azurite
4. The threading pattern is proven — remaining stories are mechanical repetition

### Incremental Delivery

1. Phase 1 → Build system ready
2. User Story 1 (T003–T004) → Async get + upsert → Validate (MVP!)
3. User Story 2 (T005–T006) → Async delete + query → Validate
4. User Story 3 (T007) → Async batch upsert → Validate
5. Phase 6 (T008–T009) → Full build + quickstart validation

### Single Developer Strategy

Since all changes are in one file (`src/azure-storage-client.cpp`), execute sequentially in priority order:
1. T001 → T002 → T003 → T004 → validate MVP
2. T005 → T006 → validate
3. T007 → T008 → T009 → done

---

## Notes

- All five async methods follow the identical copy-at-dispatch pattern (R1, R2 from research.md)
- No new public API — only replacing existing stub bodies
- Header file `azure-storage-client.hpp` requires no changes
- `perform_request()` creates a fresh CURL handle per call — concurrent execution is safe (R3)
- Empty callback check required before invocation (R4)
- All parameters captured by value in thread closure (R6)
