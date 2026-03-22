# Data Model: Autotools Scaffold

**Feature**: 001-autotools-scaffold  
**Date**: 2026-03-22

## Overview

This feature introduces no domain entities or persistent data. It
establishes the build infrastructure only.

The single "model" relevant to this scaffold is the public API class
declared in the header. It is documented here for traceability from
spec → data-model → tasks.

## AzureTableClient (public header declaration)

The public header `src/azure-storage-client.h` declares the full
class interface. All methods are stubs in the `.cpp` file — they
compile and link but perform no operations.

### Fields (private, implied by constructors)

| Field | Type | Source | Purpose |
|-------|------|--------|---------|
| `account_name` | `std::string` | Shared Key ctor | Storage account name |
| `account_key` | `std::string` | Shared Key ctor | Base64-encoded account key |
| `table_endpoint` | `std::string` | Both ctors | Table service endpoint URL |
| `bearer_token` | `std::string` | Bearer ctor / `SetBearerToken` | OAuth2 bearer token |

These fields are listed for header design only. They will be
implemented in a later feature. The scaffold header may declare them
or omit private members entirely — the public contract is what
matters.

**Convention**: No underscore suffix on member names. Always use
`this->` to disambiguate members from parameters/locals.

### Methods (public)

| Method | Returns | Stub behavior |
|--------|---------|---------------|
| `AzureTableClient(account_name, account_key, table_endpoint)` | — | No-op |
| `AzureTableClient(table_endpoint, bearer_token)` | — | No-op |
| `SetBearerToken(bearer_token)` | `void` | No-op |
| `CreateTableIfNotExists(table_name)` | `bool` | `return false;` |
| `GetEntity(table_name, partition_key, row_key)` | `nlohmann::json` | `return {};` |
| `UpsertEntity(table_name, entity)` | `bool` | `return false;` |
| `BatchUpsertEntities(table_name, entities)` | `bool` | `return false;` |
| `QueryEntities(table_name, filter)` | `std::vector<nlohmann::json>` | `return {};` |
| `DeleteEntity(table_name, partition_key, row_key)` | `bool` | `return false;` |
| `GetEntityAsync(...)` | `void` | Invoke callback with `{}` |
| `UpsertEntityAsync(...)` | `void` | Invoke callback with `false` |
| `BatchUpsertEntitiesAsync(...)` | `void` | Invoke callback with `false` |
| `QueryEntitiesAsync(...)` | `void` | Invoke callback with `{}` |
| `DeleteEntityAsync(...)` | `void` | Invoke callback with `false` |

### Relationships

None. Single class, no inheritance, no composition with other project
entities. The class depends on `nlohmann::json`, `std::string`,
`std::vector`, `std::function` — all external/standard types.
