# Feature Specification: Asynchronous CRUD Operations

**Feature Branch**: `004-async-crud-operations`
**Created**: 2026-03-22
**Status**: Draft
**Input**: User description: "Implement the asynchronous functions"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Get an Entity Asynchronously (Priority: P1)

As a library consumer, I call `GetEntityAsync` with a table name, partition key, row key, and a callback function. The library performs the network request on a separate thread and invokes the callback with the resulting JSON entity once the request completes. This allows my application to continue doing other work while the I/O operation is in flight.

**Why this priority**: The async get is the simplest async operation and, once working, proves that the threading model, callback invocation, and data hand-off between threads are correct. All other async operations follow the same pattern.

**Independent Test**: Against a running Azurite instance with a pre-created table and a known entity, call `GetEntityAsync` with a callback that captures the result into a shared variable. Wait for the callback to fire and verify the returned JSON contains the expected PartitionKey, RowKey, and property values.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table containing an entity with PartitionKey "pk1" and RowKey "rk1", **When** `GetEntityAsync("MyTable", "pk1", "rk1", callback)` is called, **Then** the callback is invoked on a background thread with a JSON object containing the entity's properties.
2. **Given** an authenticated client and a table that does not contain the specified entity, **When** `GetEntityAsync` is called, **Then** the callback is invoked with an empty JSON object.
3. **Given** an authenticated client, **When** `GetEntityAsync` is called, **Then** the calling thread returns immediately without blocking on the network request.

---

### User Story 2 - Upsert an Entity Asynchronously (Priority: P1)

As a library consumer, I call `UpsertEntityAsync` with a table name, a JSON entity, and a callback. The library performs the upsert on a separate thread and invokes the callback with `true` on success or `false` on failure.

**Why this priority**: Async writes are equally fundamental as async reads and together they enable fully non-blocking CRUD workflows. Having both read and write proves the async pattern works for both directions of data flow.

**Independent Test**: Against Azurite, call `UpsertEntityAsync` with a callback that captures the boolean result. Wait for the callback, then call `GetEntity` (sync) to confirm the entity was persisted.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a valid JSON entity with PartitionKey and RowKey, **When** `UpsertEntityAsync` is called, **Then** the callback is invoked with `true` and the entity is persisted in the table.
2. **Given** an authenticated client and a JSON entity missing PartitionKey or RowKey, **When** `UpsertEntityAsync` is called, **Then** the callback is invoked with `false`.
3. **Given** an authenticated client, **When** `UpsertEntityAsync` is called, **Then** the calling thread returns immediately without blocking.

---

### User Story 3 - Delete an Entity Asynchronously (Priority: P2)

As a library consumer, I call `DeleteEntityAsync` with a table name, partition key, row key, and a callback. The library performs the delete on a separate thread and invokes the callback with `true` on success or `false` on failure.

**Why this priority**: Delete completes the basic async CRUD lifecycle. It is lower priority than read/write because applications can function without async delete initially.

**Independent Test**: Against Azurite, create an entity via `UpsertEntity`, call `DeleteEntityAsync` with a callback, wait for the callback, then confirm `GetEntity` returns an empty result.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table containing the specified entity, **When** `DeleteEntityAsync` is called, **Then** the callback is invoked with `true` and the entity is removed.
2. **Given** an authenticated client and a table that does not contain the specified entity, **When** `DeleteEntityAsync` is called, **Then** the callback is invoked with `false`.
3. **Given** an authenticated client, **When** `DeleteEntityAsync` is called, **Then** the calling thread returns immediately without blocking.

---

### User Story 4 - Query Entities Asynchronously (Priority: P2)

As a library consumer, I call `QueryEntitiesAsync` with a table name, a filter string, and a callback. The library executes the query (including following all continuation tokens) on a separate thread and invokes the callback with the full result set.

**Why this priority**: Async querying enables non-blocking bulk reads. It is slightly lower priority than single-entity operations because consumers can build basic async workflows with point reads alone.

**Independent Test**: Against Azurite, insert several entities, call `QueryEntitiesAsync` with a filter, wait for the callback, and verify only matching entities are returned.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table with multiple entities, **When** `QueryEntitiesAsync` is called with a filter that matches some entities, **Then** the callback is invoked with a vector containing only the matching entities.
2. **Given** an authenticated client and a table with entities, **When** `QueryEntitiesAsync` is called with an empty filter, **Then** the callback is invoked with all entities in the table.
3. **Given** an authenticated client and a non-existent table, **When** `QueryEntitiesAsync` is called, **Then** the callback is invoked with an empty vector.

---

### User Story 5 - Batch Upsert Entities Asynchronously (Priority: P3)

As a library consumer, I call `BatchUpsertEntitiesAsync` with a table name, a vector of JSON entities, and a callback. The library sends the batch transaction on a separate thread and invokes the callback with `true` on success or `false` on failure.

**Why this priority**: Batch operations are the most complex async operation. Consumers can achieve the same result by calling `UpsertEntityAsync` repeatedly for smaller workloads. Async batch matters for high-throughput scenarios.

**Independent Test**: Against Azurite, call `BatchUpsertEntitiesAsync` with several entities sharing the same PartitionKey and a callback. Wait for the callback, then query the table to confirm all entities were persisted.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a vector of entities sharing the same PartitionKey, **When** `BatchUpsertEntitiesAsync` is called, **Then** the callback is invoked with `true` and all entities are persisted.
2. **Given** an authenticated client and a vector containing more than 100 entities, **When** `BatchUpsertEntitiesAsync` is called, **Then** the callback is invoked with `false` without sending a network request.
3. **Given** an authenticated client and an empty vector, **When** `BatchUpsertEntitiesAsync` is called, **Then** the callback is invoked with `true`.

---

### Edge Cases

- What happens when a callback throws an exception? The library does not catch exceptions thrown by user-provided callbacks. The exception propagates on the background thread. It is the caller's responsibility to handle exceptions within their callback.
- What happens when the client object is destroyed while an async operation is in flight? Safe. Because async methods copy all needed client state at dispatch time, the background thread does not reference the client object after dispatch. The client can be safely destroyed while operations are in flight.
- What happens when multiple async operations are issued concurrently? Each operation runs independently on its own thread. The library does not serialize concurrent operations; the callback for each operation fires independently when its request completes.
- What happens when the async version receives the same error conditions as the sync version (e.g., missing keys, service errors)? The async version delegates to the corresponding sync implementation and returns the same result through the callback. The error behavior is identical to the sync counterpart.
- What happens when the callback is `nullptr` or an empty `std::function`? The library checks for a valid callback before invoking it. If the callback is empty, the operation still executes but no result is delivered.

## Clarifications

### Session 2026-03-22

- Q: How should the library handle concurrent `SetBearerToken()` calls while async operations are in-flight? → A: Copy-at-dispatch — async methods copy all needed client state (endpoint, credentials) at call time before spawning the thread.
- Q: Should the library impose any concurrency limit on simultaneous async operations? → A: No limit — caller is responsible for controlling concurrency.
- Q: Should this feature add a `CreateTableIfNotExistsAsync` method? → A: Out of scope — only implement the five async methods already declared in the header (`GetEntityAsync`, `UpsertEntityAsync`, `DeleteEntityAsync`, `QueryEntitiesAsync`, `BatchUpsertEntitiesAsync`).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Each async method MUST perform its work on a background thread so the calling thread returns immediately.
- **FR-002**: Each async method MUST invoke the user-provided callback exactly once with the operation's result when the operation completes.
- **FR-003**: `GetEntityAsync` MUST produce the same result as `GetEntity` for identical inputs, delivered via callback.
- **FR-004**: `UpsertEntityAsync` MUST produce the same result as `UpsertEntity` for identical inputs, delivered via callback.
- **FR-005**: `DeleteEntityAsync` MUST produce the same result as `DeleteEntity` for identical inputs, delivered via callback.
- **FR-006**: `QueryEntitiesAsync` MUST produce the same result as `QueryEntities` for identical inputs (including following all continuation tokens), delivered via callback.
- **FR-007**: `BatchUpsertEntitiesAsync` MUST produce the same result as `BatchUpsertEntities` for identical inputs, delivered via callback.
- **FR-008**: Each async method MUST validate inputs (e.g., missing PartitionKey/RowKey) using the same rules as its synchronous counterpart before or during execution.
- **FR-009**: The background thread for each async operation MUST be detached or otherwise managed so that it does not block the calling thread or require manual joining by the caller.
- **FR-011**: Each async method MUST capture a snapshot of all required client state (table endpoint, account name, account key, bearer token) at call time before spawning the background thread. The background thread MUST use this snapshot, not the live client members.
- **FR-010**: If the user-provided callback is empty (default-constructed `std::function`), the async method MUST still execute the operation but skip the callback invocation.

## Assumptions

- The existing synchronous CRUD methods are fully implemented and correct. Async methods delegate to them.
- The library does not manage a thread pool. Each async call spawns a new detached thread. No concurrency limit is imposed; the caller is responsible for throttling the number of simultaneous async operations to avoid resource exhaustion.
- The caller is responsible for thread-safe access to any shared state referenced by their callback.
- `CreateTableIfNotExistsAsync` is explicitly out of scope. Table creation is a setup operation where synchronous blocking is acceptable.
- Because async methods copy client state at dispatch time, mutating credentials via `SetBearerToken()` after an async call has been dispatched does not affect operations already in flight.
- The `AzureTableClient` instance only needs to remain alive through the async method call itself (not for the full duration of the background operation), since all needed state is copied before the thread is spawned.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All five async methods invoke their callback with a result that matches the output of the corresponding synchronous method for the same inputs.
- **SC-002**: The calling thread returns from any async method call without waiting for the network round-trip to complete.
- **SC-003**: Multiple async operations issued in rapid succession all complete successfully and invoke their callbacks independently.
- **SC-004**: Async operations against Azurite complete and invoke their callbacks within a reasonable time, comparable to the synchronous equivalents plus minimal threading overhead.
