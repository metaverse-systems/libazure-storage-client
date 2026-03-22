# Data Model: Asynchronous CRUD Operations

**Feature**: 004-async-crud-operations
**Date**: 2026-03-22

## Entities

This feature introduces no new entities or data structures. The async methods operate on the same entities as the sync methods:

### AzureTableClient (existing — no changes to public interface)

| Field | Type | Description |
|-------|------|-------------|
| account_name | `std::string` | Azure storage account name (Shared Key auth) |
| account_key | `std::string` | Base64-encoded account key (Shared Key auth) |
| table_endpoint | `std::string` | Table service endpoint URL |
| bearer_token | `std::string` | OAuth2/Entra ID bearer token |

**State snapshot**: At async dispatch time, all four fields are copied into the thread closure. The background thread operates on the copy, not the live object.

### Async Method Signatures (existing — already declared, stubs to be replaced)

| Method | Parameters Captured by Value | Callback Signature |
|--------|-----------------------------|--------------------|
| `GetEntityAsync` | table_name, partition_key, row_key | `void(nlohmann::json)` |
| `UpsertEntityAsync` | table_name, entity | `void(bool)` |
| `DeleteEntityAsync` | table_name, partition_key, row_key | `void(bool)` |
| `QueryEntitiesAsync` | table_name, filter | `void(std::vector<nlohmann::json>)` |
| `BatchUpsertEntitiesAsync` | table_name, entities | `void(bool)` |

## State Transitions

No state transitions. Each async call is a fire-and-forget operation. The client object's state is not modified by async methods.

## Validation Rules

All validation is inherited from the sync counterparts:
- `UpsertEntity` / `BatchUpsertEntities`: entities must contain `PartitionKey` and `RowKey` string fields
- `BatchUpsertEntities`: vector must not exceed 100 entities
- Empty callback: operation executes but callback is skipped
