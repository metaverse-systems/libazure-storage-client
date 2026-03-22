# Data Model: Synchronous CRUD Operations

**Feature**: 003-sync-crud-operations
**Date**: 2026-03-22

## Internal Types

### HttpResponse (anonymous namespace — not public)

Returned by the refactored `perform_request` helper. Contains all HTTP response data needed by any operation.

| Field | Type | Description |
|-------|------|-------------|
| `status` | `long` | HTTP status code (0 on network error) |
| `body` | `std::string` | Response body (empty if no body) |
| `headers` | `std::map<std::string, std::string>` | Response headers (lowercase keys → trimmed values) |

**Lifecycle**: Created per-request inside `perform_request`, returned by value. No heap allocation beyond the strings/map.

## Domain Entities

### Entity (JSON object — no dedicated C++ type)

Entities are represented as `nlohmann::json` objects throughout the public API. There is no dedicated C++ struct — this matches the Minimal Surface Area principle.

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `PartitionKey` | string | Yes | First component of the composite key |
| `RowKey` | string | Yes | Second component of the composite key |
| *(arbitrary)* | string, int, double, bool | No | User-defined properties |

**Uniqueness**: The pair (PartitionKey, RowKey) uniquely identifies an entity within a table.

**OData URL Representation**: `{tableName}(PartitionKey='{escapedPK}',RowKey='{escapedRK}')`

### Batch Transaction (no dedicated C++ type)

Batch transactions are built as multipart/mixed HTTP request bodies inside `BatchUpsertEntities`. No persistent state or dedicated type.

| Attribute | Constraint |
|-----------|-----------|
| Max operations | 100 per batch |
| PartitionKey | All entities must share the same PartitionKey |
| Max payload | 4 MiB |
| Atomicity | All-or-nothing; failure rolls back entire changeset |

## State Transitions

### Entity Lifecycle

```
[Not Exists] --UpsertEntity--> [Exists]
[Exists]     --UpsertEntity--> [Exists] (replaced)
[Exists]     --DeleteEntity--> [Not Exists]
[Exists]     --GetEntity-----> [Exists] (no change, returns data)
[Not Exists] --GetEntity-----> [Not Exists] (returns empty JSON)
[Not Exists] --DeleteEntity--> [Not Exists] (returns false)
```

### perform_request Refactoring

```
Before: perform_request(method, url, headers, body) → long (status code only)
After:  perform_request(method, url, headers, body) → HttpResponse { status, body, headers }
```

**Impact on existing code**: `CreateTableIfNotExists` currently uses the `long` return value. After refactoring, it will use `response.status` from the returned `HttpResponse`. This is a mechanical change — same logic, different access pattern.

## Relationships

```
AzureTableClient (1) ----uses----> perform_request (1) ----returns----> HttpResponse
AzureTableClient (1) ----manages--> Entity (0..*)  via Table Storage REST API
Batch Transaction    ----contains--> Entity (1..100, same PartitionKey)
```
