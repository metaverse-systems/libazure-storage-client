# Quickstart: Synchronous CRUD Operations

**Feature**: 003-sync-crud-operations
**Prerequisites**: Azurite running on `http://127.0.0.1:10002/devstoreaccount1`, library built and linked.

## Setup

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>
#include <iostream>

int main()
{
    // Azurite well-known credentials
    std::string account = "devstoreaccount1";
    std::string key = "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsu"
                      "Fq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==";
    std::string endpoint = "http://127.0.0.1:10002/devstoreaccount1";

    AzureTableClient client(account, key, endpoint);

    // Create a table
    client.CreateTableIfNotExists("People");
```

## Upsert an Entity

```cpp
    nlohmann::json person;
    person["PartitionKey"] = "engineering";
    person["RowKey"] = "alice";
    person["Name"] = "Alice";
    person["Age"] = 30;

    bool ok = client.UpsertEntity("People", person);
    std::cout << "Upsert: " << (ok ? "success" : "failed") << std::endl;
```

## Get an Entity

```cpp
    nlohmann::json result = client.GetEntity("People", "engineering", "alice");
    if (!result.empty())
        std::cout << "Got: " << result.dump(2) << std::endl;
    else
        std::cout << "Entity not found" << std::endl;
```

## Query Entities

```cpp
    // Insert a second entity
    nlohmann::json person2;
    person2["PartitionKey"] = "engineering";
    person2["RowKey"] = "bob";
    person2["Name"] = "Bob";
    person2["Age"] = 25;
    client.UpsertEntity("People", person2);

    // Query with filter
    auto results = client.QueryEntities("People", "Age gt 27");
    std::cout << "Found " << results.size() << " entities:" << std::endl;
    for (const auto &e : results)
        std::cout << "  " << e.dump() << std::endl;

    // Query all
    auto all = client.QueryEntities("People", "");
    std::cout << "Total entities: " << all.size() << std::endl;
```

## Delete an Entity

```cpp
    bool deleted = client.DeleteEntity("People", "engineering", "bob");
    std::cout << "Delete: " << (deleted ? "success" : "failed") << std::endl;
```

## Batch Upsert

```cpp
    std::vector<nlohmann::json> batch;
    for (int i = 0; i < 5; i++)
    {
        nlohmann::json e;
        e["PartitionKey"] = "batch-test";
        e["RowKey"] = "row-" + std::to_string(i);
        e["Value"] = i * 10;
        batch.push_back(e);
    }

    bool batch_ok = client.BatchUpsertEntities("People", batch);
    std::cout << "Batch: " << (batch_ok ? "success" : "failed") << std::endl;

    return 0;
}
```

## Build & Run

```bash
# Start Azurite (if not already running)
azurite-table --location /tmp/azurite &

# Build the project
make

# Compile and link the example
g++ -std=c++20 example.cpp -o example \
    $(pkg-config --cflags --libs azure-storage-client) \
    -lcurl -lssl -lcrypto

# Run
./example
```

## Expected Output

```
Upsert: success
Got: {
  "Age": 30,
  "Name": "Alice",
  "PartitionKey": "engineering",
  "RowKey": "alice"
}
Found 1 entities:
  {"Age":30,"Name":"Alice","PartitionKey":"engineering","RowKey":"alice"}
Total entities: 2
Delete: success
Batch: success
```
