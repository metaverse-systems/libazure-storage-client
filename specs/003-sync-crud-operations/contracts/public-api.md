# Public API Contract: Synchronous CRUD Operations

**Feature**: 003-sync-crud-operations
**Date**: 2026-03-22
**Header**: `include/libazure-storage-client/azure-storage-client.hpp`

## Scope

This contract covers the five synchronous CRUD methods implemented in this feature. Constructors, `SetBearerToken`, and `CreateTableIfNotExists` are documented in feature 002. Async variants remain as stubs.

## Entity Key URL Pattern

All single-entity operations (get, upsert, delete) use this URL pattern:

```
{endpoint}/{tableName}(PartitionKey='{escapedPK}',RowKey='{escapedRK}')
```

**Key escaping rules**:
- Single quotes in key values are doubled: `O'Brien` → `O''Brien`
- The escaped key string is then URL-encoded for path safety

## Methods

### GetEntity

```
nlohmann::json GetEntity(table_name, partition_key, row_key)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Target table name |
| partition_key | `const std::string&` | Entity partition key |
| row_key | `const std::string&` | Entity row key |

**Returns**: JSON object containing the entity's properties (including PartitionKey and RowKey) on success (HTTP 200). Empty JSON object (`{}`) on any error (HTTP 404, network failure, etc.).

**HTTP Request**:
- Method: `GET`
- URL: `{endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}')`
- Headers: Common headers (see feature 002 contract) + `Accept: application/json;odata=nometadata`
- Body: None

---

### UpsertEntity

```
bool UpsertEntity(table_name, entity)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Target table name |
| entity | `const nlohmann::json&` | JSON object with PartitionKey, RowKey, and properties |

**Precondition**: `entity` must contain `"PartitionKey"` and `"RowKey"` string fields.

**Returns**: `true` on success (HTTP 204). `false` for any other status or error.

**Behavior**: Insert-or-replace semantics. Creates the entity if it doesn't exist; fully replaces it if it does. Properties not present in the new entity are removed.

**HTTP Request**:
- Method: `PUT`
- URL: `{endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}')` (keys extracted from entity JSON)
- Headers: Common headers + `Content-Type: application/json;odata=nometadata`
- Body: Entity JSON

---

### DeleteEntity

```
bool DeleteEntity(table_name, partition_key, row_key)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Target table name |
| partition_key | `const std::string&` | Entity partition key |
| row_key | `const std::string&` | Entity row key |

**Returns**: `true` on success (HTTP 204). `false` for any other status or error (including 404 — entity not found).

**HTTP Request**:
- Method: `DELETE`
- URL: `{endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}')`
- Headers: Common headers + `If-Match: *`
- Body: None

---

### QueryEntities

```
std::vector<nlohmann::json> QueryEntities(table_name, filter)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Target table name |
| filter | `const std::string&` | OData $filter expression (empty = return all entities) |

**Returns**: Vector of JSON entity objects on success. Empty vector on any error.

**Pagination**: The library transparently follows continuation tokens (`x-ms-continuation-NextPartitionKey`, `x-ms-continuation-NextRowKey`) until all pages are consumed. No library-level cap.

**HTTP Request** (per page):
- Method: `GET`
- URL: `{endpoint}/{tableName}()?$filter={urlEncodedFilter}` (omit `$filter` if empty)
- Subsequent pages append: `&NextPartitionKey={value}&NextRowKey={value}`
- Headers: Common headers + `Accept: application/json;odata=nometadata`
- Body: None

**Response parsing**: Entities are extracted from the `"value"` array in the JSON response body.

---

### BatchUpsertEntities

```
bool BatchUpsertEntities(table_name, entities)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Target table name |
| entities | `const std::vector<nlohmann::json>&` | Vector of entity JSON objects (max 100, same PartitionKey) |

**Returns**: `true` on success (HTTP 202 with all-204 inner responses). `false` on any error. `true` (no-op) when `entities` is empty.

**Atomicity**: All-or-nothing. If any operation fails, the entire batch is rolled back.

**HTTP Request**:
- Method: `POST`
- URL: `{endpoint}/$batch`
- Headers: Common headers + `Content-Type: multipart/mixed; boundary=batch_{id}`
- Body: Multipart/mixed with changeset containing individual PUT operations

**Body structure**:
```
--batch_{batchId}
Content-Type: multipart/mixed; boundary=changeset_{changesetId}

--changeset_{changesetId}
Content-Type: application/http
Content-Transfer-Encoding: binary

PUT {endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}') HTTP/1.1
Content-Type: application/json
Accept: application/json;odata=nometadata

{entityJson}

--changeset_{changesetId}--
--batch_{batchId}--
```

## Common Behaviors

- **Auth dispatch**: All methods use the same bearer-token-first / shared-key-fallback pattern as `CreateTableIfNotExists`.
- **No retry**: All HTTP errors are returned immediately to the caller.
- **No exceptions**: Methods return `false` or empty results on failure; never throw.
- **Thread safety**: Each synchronous call creates its own libcurl handle and is self-contained.
