# Public API Contract: Asynchronous CRUD Operations

**Feature**: 004-async-crud-operations
**Date**: 2026-03-22

## Overview

This feature implements the five async methods already declared in `azure-storage-client.hpp`. No new public symbols are added. The header file is unchanged.

## Async Method Contracts

All async methods share these invariants:
- Returns `void` — the caller cannot obtain a return value or future
- Accepts a `std::function` callback as the last parameter
- Returns to the caller immediately without blocking
- Invokes the callback exactly once on a background thread
- If the callback is empty (default-constructed), the operation executes but no callback fires
- The result delivered via callback is identical to what the sync counterpart would return

---

### GetEntityAsync

```cpp
void GetEntityAsync(const std::string &table_name,
                    const std::string &partition_key,
                    const std::string &row_key,
                    std::function<void(nlohmann::json)> callback);
```

**Callback receives**: `nlohmann::json` — the entity object on success, or empty JSON `{}` on not-found/error.

**Delegates to**: `GetEntity(table_name, partition_key, row_key)`

---

### UpsertEntityAsync

```cpp
void UpsertEntityAsync(const std::string &table_name,
                       const nlohmann::json &entity,
                       std::function<void(bool)> callback);
```

**Callback receives**: `true` if entity was created/replaced (HTTP 204), `false` on validation failure or service error.

**Delegates to**: `UpsertEntity(table_name, entity)`

---

### DeleteEntityAsync

```cpp
void DeleteEntityAsync(const std::string &table_name,
                       const std::string &partition_key,
                       const std::string &row_key,
                       std::function<void(bool)> callback);
```

**Callback receives**: `true` if entity was deleted (HTTP 204), `false` on not-found or service error.

**Delegates to**: `DeleteEntity(table_name, partition_key, row_key)`

---

### QueryEntitiesAsync

```cpp
void QueryEntitiesAsync(const std::string &table_name,
                        const std::string &filter,
                        std::function<void(std::vector<nlohmann::json>)> callback);
```

**Callback receives**: `std::vector<nlohmann::json>` — all matching entities (following continuation tokens), or empty vector on error.

**Delegates to**: `QueryEntities(table_name, filter)`

---

### BatchUpsertEntitiesAsync

```cpp
void BatchUpsertEntitiesAsync(const std::string &table_name,
                              const std::vector<nlohmann::json> &entities,
                              std::function<void(bool)> callback);
```

**Callback receives**: `true` if all entities were upserted (HTTP 202, no error lines in response), `false` on validation failure, batch limit exceeded, or service error.

**Delegates to**: `BatchUpsertEntities(table_name, entities)`

## Threading Contract

- Each async call spawns a new `std::thread` which is immediately detached
- The thread captures a **copy** of all client state (endpoint, credentials) and all parameters by value
- The thread creates its own CURL handle via `perform_request()` — no handle sharing
- No concurrency limit is imposed by the library
- No thread pool is used
- The callback executes on the background thread, not the calling thread
