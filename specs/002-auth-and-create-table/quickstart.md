# Quickstart: Auth & CreateTableIfNotExists

**Feature**: 002-auth-and-create-table
**Date**: 2026-03-22

## Prerequisites

1. **Azurite** running locally (Table service on port 10002):
   ```bash
   npm install -g azurite
   azurite-table --tableHost 127.0.0.1 --tablePort 10002
   ```

2. **Build the library**:
   ```bash
   cd /home/tim/projects/metaverse-systems/libazure-storage-client
   autoreconf -fi
   ./configure
   make
   ```

## Usage: SharedKey Auth + CreateTableIfNotExists

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>
#include <iostream>

int main() {
    // Azurite well-known dev credentials
    std::string account = "devstoreaccount1";
    std::string key = "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==";
    std::string endpoint = "http://127.0.0.1:10002/devstoreaccount1";

    AzureTableClient client(account, key, endpoint);

    // Create table (idempotent — safe to call repeatedly)
    bool ok = client.CreateTableIfNotExists("MyTable");
    std::cout << "CreateTable: " << (ok ? "success" : "failed") << std::endl;

    // Call again — returns true (409 treated as success)
    bool ok2 = client.CreateTableIfNotExists("MyTable");
    std::cout << "CreateTable (2nd): " << (ok2 ? "success" : "failed") << std::endl;

    return 0;
}
```

## Usage: Bearer Token Auth

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>

int main() {
    std::string endpoint = "https://myaccount.table.core.windows.net";
    std::string token = "<your-entra-id-token>";

    AzureTableClient client(endpoint, token);

    // All requests use Authorization: Bearer <token>
    client.CreateTableIfNotExists("MyTable");

    // Update token without recreating client
    client.SetBearerToken("<refreshed-token>");
    client.CreateTableIfNotExists("AnotherTable");

    return 0;
}
```

## Usage: Switch from SharedKey to Bearer

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>

int main() {
    // Start with SharedKey (e.g., local dev against Azurite)
    AzureTableClient client("devstoreaccount1",
                            "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==",
                            "http://127.0.0.1:10002/devstoreaccount1");

    client.CreateTableIfNotExists("DevTable"); // Uses SharedKey

    // Later, switch to bearer token (e.g., production Entra ID)
    client.SetBearerToken("<production-token>");
    // Now all requests use Bearer auth instead of SharedKey

    return 0;
}
```

## Build & Link

```bash
# Compile with pkg-config
g++ -std=c++20 main.cpp $(pkg-config --cflags --libs azure-storage-client) -o myapp
```

## Verification Against Azurite

```bash
# 1. Start Azurite Table service
azurite-table --tableHost 127.0.0.1 --tablePort 10002 &

# 2. Build and run your test program
make && ./myapp

# Expected output:
# CreateTable: success
# CreateTable (2nd): success
```
