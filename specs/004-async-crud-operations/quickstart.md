# Quickstart: Asynchronous CRUD Operations

**Feature**: 004-async-crud-operations
**Date**: 2026-03-22

## Prerequisites

- Azurite running on `http://127.0.0.1:10002/devstoreaccount1`
- Library built with `-pthread` support
- A table created via `CreateTableIfNotExists`

## Usage Examples

### Async Get

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>
#include <iostream>
#include <mutex>
#include <condition_variable>

int main() {
    AzureTableClient client(
        "devstoreaccount1",
        "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==",
        "http://127.0.0.1:10002/devstoreaccount1");

    client.CreateTableIfNotExists("MyTable");

    // Upsert a test entity first
    nlohmann::json entity;
    entity["PartitionKey"] = "users";
    entity["RowKey"] = "user1";
    entity["Name"] = "Alice";
    client.UpsertEntity("MyTable", entity);

    // Async get with synchronization
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

    client.GetEntityAsync("MyTable", "users", "user1",
        [&](nlohmann::json result) {
            std::cout << "Got entity: " << result.dump(2) << std::endl;
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
            cv.notify_one();
        });

    // Wait for callback
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return done; });

    return 0;
}
```

### Async Upsert

```cpp
client.UpsertEntityAsync("MyTable", entity,
    [](bool success) {
        std::cout << "Upsert " << (success ? "succeeded" : "failed") << std::endl;
    });
```

### Async Delete

```cpp
client.DeleteEntityAsync("MyTable", "users", "user1",
    [](bool success) {
        std::cout << "Delete " << (success ? "succeeded" : "failed") << std::endl;
    });
```

### Async Query

```cpp
client.QueryEntitiesAsync("MyTable", "PartitionKey eq 'users'",
    [](std::vector<nlohmann::json> results) {
        std::cout << "Found " << results.size() << " entities" << std::endl;
    });
```

### Async Batch Upsert

```cpp
std::vector<nlohmann::json> entities;
for (int i = 0; i < 10; i++) {
    nlohmann::json e;
    e["PartitionKey"] = "batch";
    e["RowKey"] = "row" + std::to_string(i);
    e["Value"] = i;
    entities.push_back(e);
}

client.BatchUpsertEntitiesAsync("MyTable", entities,
    [](bool success) {
        std::cout << "Batch " << (success ? "succeeded" : "failed") << std::endl;
    });
```

## Key Notes

- The calling thread returns immediately from all async methods
- Callbacks fire on background threads — use mutexes/condition variables if you need to wait or share data
- The client can be destroyed after dispatching async calls (state is copied)
- No concurrency limit — caller is responsible for throttling
