# Feature Specification: Synchronous CRUD Operations

**Feature Branch**: `003-sync-crud-operations`
**Created**: 2026-03-22
**Status**: Draft
**Input**: User description: "Implement the synchronous functions"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Get a Single Entity (Priority: P1)

As a library consumer, I call `GetEntity` with a table name, partition key, and row key, and the library returns the matching entity as a JSON object. This is the foundational read operation and the simplest CRUD path to validate that authenticated requests, URL construction, and response parsing all work end-to-end.

**Why this priority**: Point reads are the most common Table Storage operation and the simplest to implement correctly. Once this works, it proves the full request-response pipeline (auth, URL building, response body parsing) which all other operations depend on.

**Independent Test**: Against a running Azurite instance with a pre-created table and a known entity, call `GetEntity` and verify the returned JSON contains the expected PartitionKey, RowKey, and property values.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table containing an entity with PartitionKey "pk1" and RowKey "rk1", **When** `GetEntity("MyTable", "pk1", "rk1")` is called, **Then** the library returns a JSON object containing the entity's properties including PartitionKey and RowKey.
2. **Given** an authenticated client and a table that does not contain an entity with the specified keys, **When** `GetEntity` is called with those keys, **Then** the library returns an empty JSON object.
3. **Given** an authenticated client and a non-existent table name, **When** `GetEntity` is called, **Then** the library returns an empty JSON object.

---

### User Story 2 - Upsert a Single Entity (Priority: P1)

As a library consumer, I call `UpsertEntity` with a table name and a JSON object containing at least PartitionKey and RowKey. If the entity does not exist, it is created. If it already exists, it is replaced. The library returns `true` on success.

**Why this priority**: Write capability is equally fundamental as read. Upsert is preferred over separate insert/update because it simplifies consumer code and is the most common write pattern. It also provides the data needed to test `GetEntity`.

**Independent Test**: Against Azurite, upsert an entity, then retrieve it with `GetEntity` to confirm it was persisted. Upsert the same entity with modified properties and retrieve again to confirm the update took effect.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table with no matching entity, **When** `UpsertEntity` is called with a JSON object containing PartitionKey, RowKey, and additional properties, **Then** the entity is created and the library returns `true`.
2. **Given** an authenticated client and a table with an existing entity matching the provided PartitionKey and RowKey, **When** `UpsertEntity` is called with modified properties, **Then** the entity is replaced and the library returns `true`.
3. **Given** an authenticated client and a service error (e.g., non-existent table), **When** `UpsertEntity` is called, **Then** the library returns `false`.

---

### User Story 3 - Delete an Entity (Priority: P2)

As a library consumer, I call `DeleteEntity` with a table name, partition key, and row key to remove an entity from storage. The library returns `true` on success.

**Why this priority**: Delete completes the basic CRUD lifecycle (create, read, delete). It is essential for data management but ranks below read and write because consumers can function without delete in early usage.

**Independent Test**: Against Azurite, create an entity via `UpsertEntity`, delete it via `DeleteEntity`, then confirm `GetEntity` returns an empty result.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table containing the specified entity, **When** `DeleteEntity` is called, **Then** the entity is removed and the library returns `true`.
2. **Given** an authenticated client and a table that does not contain the specified entity, **When** `DeleteEntity` is called, **Then** the library returns `false`.
3. **Given** an authenticated client and a service error, **When** `DeleteEntity` is called, **Then** the library returns `false`.

---

### User Story 4 - Query Entities with Filter (Priority: P2)

As a library consumer, I call `QueryEntities` with a table name and an OData filter string to retrieve a set of matching entities. The library returns a vector of JSON objects. If the filter is empty, all entities in the table are returned.

**Why this priority**: Querying is a core read operation that enables consumers to retrieve sets of data rather than individual entities. It is slightly lower priority than single-entity operations because consumers can build basic functionality with point reads alone.

**Independent Test**: Against Azurite, insert several entities with varying properties, then call `QueryEntities` with a filter that matches a subset and verify only the matching entities are returned.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table with multiple entities, **When** `QueryEntities` is called with a filter that matches some entities, **Then** the library returns only the matching entities as a vector of JSON objects.
2. **Given** an authenticated client and a table with entities, **When** `QueryEntities` is called with an empty filter, **Then** the library returns all entities in the table.
3. **Given** an authenticated client and a filter that matches no entities, **When** `QueryEntities` is called, **Then** the library returns an empty vector.
4. **Given** an authenticated client and a non-existent table, **When** `QueryEntities` is called, **Then** the library returns an empty vector.

---

### User Story 5 - Batch Upsert Entities (Priority: P3)

As a library consumer, I call `BatchUpsertEntities` with a table name and a vector of JSON entities. The library sends all entities in a single batch transaction, which either fully succeeds or fully fails. The library returns `true` on success.

**Why this priority**: Batch operations improve throughput for bulk writes but are more complex. Consumers can achieve the same result by calling `UpsertEntity` in a loop for smaller workloads. Batch becomes important for performance-sensitive use cases.

**Independent Test**: Against Azurite, batch upsert several entities sharing the same PartitionKey, then query the table to confirm all entities were persisted.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a vector of entities sharing the same PartitionKey, **When** `BatchUpsertEntities` is called, **Then** all entities are persisted and the library returns `true`.
2. **Given** an authenticated client and a vector containing an entity that would cause a service error, **When** `BatchUpsertEntities` is called, **Then** no entities are persisted (atomic rollback) and the library returns `false`.
3. **Given** an authenticated client and an empty vector of entities, **When** `BatchUpsertEntities` is called, **Then** the library returns `true` (no-op success).

---

### Edge Cases

- What happens when `GetEntity` is called with an empty partition key or row key? The library sends the request as-is and lets the service reject it; `GetEntity` returns an empty JSON object.
- What happens when `UpsertEntity` is called with a JSON object missing PartitionKey or RowKey? The library sends the request as-is; the service rejects it and `UpsertEntity` returns `false`.
- What happens when `QueryEntities` returns more results than a single page (Azure returns a continuation token)? The library follows all continuation tokens and returns all matching results in a single combined vector. No library-level cap is imposed; the caller controls result size via OData `$filter` and `$top` parameters.
- What happens when `BatchUpsertEntities` is called with entities spanning multiple PartitionKeys? Azure Table Storage requires all entities in a batch to share the same PartitionKey. The library sends the batch as-is; the service rejects it and the library returns `false`.
- What happens when `BatchUpsertEntities` is called with more than 100 entities? Azure Table Storage limits batch transactions to 100 operations. The library sends the batch as-is; the service rejects it and the library returns `false`.
- What happens when `DeleteEntity` targets an entity with a specific ETag? The library uses a wildcard ETag (`*`) to unconditionally delete regardless of concurrent modifications.
- What happens when special characters appear in PartitionKey or RowKey values? The library URL-encodes key values in request URLs to handle characters like `/`, `'`, and `%`.
- What happens when Azure Table Storage returns HTTP 429 (throttling) or 503 (service unavailable)? The library does NOT retry. It returns the failure value immediately (`false` or empty result). The caller is responsible for implementing retry logic.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `GetEntity` MUST send an HTTP GET to `{endpoint}/{tableName}(PartitionKey='{partitionKey}',RowKey='{rowKey}')` and return the response body parsed as a JSON object.
- **FR-002**: `GetEntity` MUST return an empty JSON object when the service responds with HTTP 404 or any error status.
- **FR-003**: `GetEntity` MUST URL-encode the PartitionKey and RowKey values in the request URL to handle special characters (single quotes escaped as `''` per OData conventions).
- **FR-004**: `UpsertEntity` MUST send an HTTP PUT (merge or replace) to `{endpoint}/{tableName}(PartitionKey='{partitionKey}',RowKey='{rowKey}')` with the entity JSON as the request body.
- **FR-005**: `UpsertEntity` MUST extract PartitionKey and RowKey from the provided JSON entity to construct the request URL.
- **FR-006**: `UpsertEntity` MUST return `true` when the service responds with HTTP 204 (No Content) and `false` for any other status or error.
- **FR-007**: `DeleteEntity` MUST send an HTTP DELETE to `{endpoint}/{tableName}(PartitionKey='{partitionKey}',RowKey='{rowKey}')` with an `If-Match: *` header to unconditionally delete regardless of ETag.
- **FR-008**: `DeleteEntity` MUST return `true` when the service responds with HTTP 204 (No Content) and `false` for any other status or error.
- **FR-009**: `QueryEntities` MUST send an HTTP GET to `{endpoint}/{tableName}()` with the filter string passed as a `$filter` query parameter. If the filter is empty, no `$filter` parameter is included.
- **FR-010**: `QueryEntities` MUST parse the response body as JSON and extract the array of entities from the `value` field.
- **FR-011**: `QueryEntities` MUST follow continuation tokens (`x-ms-continuation-NextPartitionKey` and `x-ms-continuation-NextRowKey` response headers) by issuing additional requests until no more continuation tokens are returned, combining all results into a single vector. No library-level page cap is imposed; the caller controls result size via OData `$filter` and `$top` query parameters.
- **FR-012**: `QueryEntities` MUST return an empty vector when the service responds with an error or the table does not exist.
- **FR-013**: `BatchUpsertEntities` MUST send a batch transaction request conforming to the Azure Table Storage batch format to upsert all provided entities atomically.
- **FR-014**: `BatchUpsertEntities` MUST return `true` when the batch transaction succeeds (HTTP 202) and `false` for any error or partial failure.
- **FR-015**: `BatchUpsertEntities` MUST return `true` (no-op) when called with an empty entity vector.
- **FR-016**: All synchronous operations MUST include the same authentication headers (Shared Key or Bearer Token) and standard headers (`x-ms-date`, `x-ms-version`, `Accept`) as established by the existing `CreateTableIfNotExists` implementation.
- **FR-017**: All synchronous operations MUST use `application/json;odata=nometadata` as the Accept header value.
- **FR-018**: `UpsertEntity` and `BatchUpsertEntities` MUST set the `Content-Type` header to `application/json;odata=nometadata` for request bodies.

### Key Entities

- **AzureTableClient**: The primary public type. Already holds authentication credentials and table endpoint. The synchronous CRUD methods are declared but currently return stub values.
- **Entity**: A JSON object representing a row in Azure Table Storage. Every entity contains a PartitionKey (string) and RowKey (string) which together form the entity's unique identifier. Additional properties are arbitrary key-value pairs.
- **Batch Transaction**: A group of up to 100 entity operations submitted atomically. All entities in a single batch must share the same PartitionKey.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `GetEntity` successfully retrieves a previously stored entity from Azurite and returns it as a valid JSON object containing the expected properties.
- **SC-002**: `UpsertEntity` successfully creates a new entity and updates an existing entity, verified by subsequent `GetEntity` calls.
- **SC-003**: `DeleteEntity` successfully removes an entity, verified by a subsequent `GetEntity` returning an empty result.
- **SC-004**: `QueryEntities` with a filter returns only matching entities from a table containing multiple entities.
- **SC-005**: `QueryEntities` with an empty filter returns all entities in the table.
- **SC-006**: `BatchUpsertEntities` persists all provided entities atomically, verified by subsequent queries.
- **SC-007**: All operations return the appropriate failure value (`false` or empty result) when the service returns an error.
- **SC-008**: All five synchronous CRUD operations pass against a running Azurite instance with no manual intervention.

## Clarifications

### Session 2026-03-22

- Q: When Azure Table Storage throttles requests (HTTP 429 or 503), should the library automatically retry with backoff, or return failure immediately? → A: No retry. The library returns the failure value immediately; the caller is responsible for retry logic.
- Q: How should the internal HTTP helper be extended to support operations that need response body and headers? → A: Return a struct containing status code, response body, and a response headers map from a single call.
- Q: Should QueryEntities impose a maximum page limit to prevent runaway requests, or always exhaust all continuation tokens? → A: No cap. The library always follows all continuation tokens until exhausted. The caller controls result size via OData $filter and $top parameters.

## Assumptions

- Authentication (Shared Key and Bearer Token) and shared HTTP request infrastructure are already implemented and working from feature 002.
- Azurite is available locally at `http://127.0.0.1:10002/devstoreaccount1` with the well-known dev-store credentials for testing.
- The library targets Azure Storage API version `2021-12-02` as established in feature 002.
- The `perform_request` helper already handles HTTP requests via libcurl but currently only returns the status code. It will be extended to return a struct containing the HTTP status code, response body (string), and response headers (string→string map) in a single call. This is required for `GetEntity`/`QueryEntities` (response body parsing) and `QueryEntities` pagination (continuation token headers).
- The Azure Table Storage batch transaction format uses multipart/mixed content type with changeset boundaries.
- The library uses the `InsertOrReplace` merge mode for upsert (HTTP PUT), which fully replaces the entity rather than merging properties.
- OData `nometadata` format is used for all requests and responses to minimize payload size.
- The library does not implement retry logic for throttled or transient-error responses. All HTTP errors are surfaced immediately to the caller.
