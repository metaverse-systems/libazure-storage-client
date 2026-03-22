# libazure-storage-client

A C++20 client library for Azure Table Storage, implementing the full REST API with Shared Key and Bearer Token authentication.

## Prerequisites

- GCC 10+ or Clang 12+ (C++20 support required)
- GNU Autotools (autoconf, automake, libtool)
- [libcurl](https://curl.se/libcurl/)
- [OpenSSL](https://www.openssl.org/)
- [nlohmann-json](https://github.com/nlohmann/json) (header-only, installed system-wide)

## Building

```bash
autoreconf -fi
./configure
make
sudo make install
```

The library installs to the default prefix (`/usr/local`). Override with `./configure --prefix=/path`.

## Testing

### Test Dependencies

- [Catch2 v3](https://github.com/catchorg/Catch2) — `sudo apt install catch2` (Ubuntu/Debian)
- [Azurite](https://github.com/Azure/Azurite) — `npm install -g azurite`

### Running Tests

```bash
# Start the Azurite Table Storage emulator (in a separate terminal)
azurite-table &

# Build and run all tests
make check
```

Tests run against the local Azurite emulator using well-known development credentials — no Azure subscription or real credentials are needed.

Run tests by tag:

```bash
./tests/test_azure_table_client "[sync]"
./tests/test_azure_table_client "[async]"
./tests/test_azure_table_client "[edge]"
./tests/test_azure_table_client "[auth]"
```

If Catch2 is not installed, `./configure` will still succeed and `make` will build the library normally — tests are simply skipped.

## Usage

### Linking with pkg-config

```bash
pkg-config --cflags --libs azure-storage-client
```

In your build system:

```makefile
CXXFLAGS += $(shell pkg-config --cflags azure-storage-client)
LDFLAGS  += $(shell pkg-config --libs azure-storage-client)
```

### Code Example

```cpp
#include <libazure-storage-client/azure-storage-client.hpp>
#include <iostream>

int main() {
    // Create a client with Shared Key authentication
    AzureTableClient client(
        "myaccount",
        "myaccountkey",
        "https://myaccount.table.core.windows.net"
    );

    // Create a table
    client.CreateTableIfNotExists("MyTable");

    // Upsert an entity
    nlohmann::json entity;
    entity["PartitionKey"] = "Users";
    entity["RowKey"] = "user1";
    entity["Name"] = "Alice";
    entity["Age"] = 30;
    client.UpsertEntity("MyTable", entity);

    // Retrieve an entity
    auto result = client.GetEntity("MyTable", "Users", "user1");
    std::cout << result["Name"].get<std::string>() << std::endl;

    return 0;
}
```

Compile with:

```bash
g++ -std=c++20 example.cpp $(pkg-config --cflags --libs azure-storage-client) -o example
```

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
