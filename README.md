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

    // Async upsert — runs in a background thread, invokes callback on completion
    nlohmann::json entity2;
    entity2["PartitionKey"] = "Users";
    entity2["RowKey"] = "user2";
    entity2["Name"] = "Bob";
    client.UpsertEntityAsync("MyTable", entity2, [](bool ok) {
        std::cout << "Async upsert: " << (ok ? "success" : "failed") << std::endl;
    });

    // Async get — retrieves entity in a background thread
    client.GetEntityAsync("MyTable", "Users", "user2",
        [](nlohmann::json result) {
            if (!result.empty())
                std::cout << result["Name"].get<std::string>() << std::endl;
        });

    // Async query — filters entities in the background
    client.QueryEntitiesAsync("MyTable", "PartitionKey eq 'Users'",
        [](std::vector<nlohmann::json> results) {
            std::cout << "Found " << results.size() << " entities" << std::endl;
        });

    // Async delete
    client.DeleteEntityAsync("MyTable", "Users", "user2",
        [](bool ok) {
            std::cout << "Async delete: " << (ok ? "success" : "failed") << std::endl;
        });

    return 0;
}
```

Compile with:

```bash
g++ -std=c++20 example.cpp $(pkg-config --cflags --libs azure-storage-client) -o example
```

## Checked queries

`QueryEntities` and `QueryEntitiesAsync` return an empty vector on failure, so a
caller cannot tell a failed query from a table with no matching rows. Use the
checked form instead:

```cpp
auto result = client.QueryEntitiesChecked("MyTable", "PartitionKey eq 'Users'");
if (!result.ok) {
    // result.failure: invalid_request, transport, http_status,
    //                 malformed_response or incomplete
    // result.http_status holds the HTTP status when one was received
    return;
}
for (const auto &row : result.rows) { /* ... */ }
```

- `ok == true` with zero rows means the table or filter matched nothing.
- `http_status` means the service answered with a status other than 200 (for
  example 404 for a missing table).
- `transport` means no response was received.
- `malformed_response` means the body was not a JSON object with a `value` array.
- `incomplete` means a later page failed; `rows` is empty, never a partial set.
- `invalid_request` means the filter could not be URL-escaped; no request is sent.
- The result never contains a URL, header, token or response body.

`QueryEntitiesCheckedAsync(table, filter, callback)` does the same and calls the
callback on a worker thread.

Use `AzureTableClient::EscapeFilterString(value)` when placing a value inside a
quoted OData filter literal; it doubles single quotes:

```cpp
auto filter = "PartitionKey eq '" + AzureTableClient::EscapeFilterString(name) + "'";
```

An empty filter queries the whole table.

## Versioning and compatibility

Version 0.1.0 added the checked API above. The change is additive and
ABI-compatible: no data member or virtual function was added, no signature
changed, and the shared library soname stays `libazure-storage-client.so.0`
(libtool version-info 1:0:1). `QueryEntities` and `QueryEntitiesAsync` keep their
behaviour, including returning an empty result on failure.

Version 0.2.0 adds `GetEntityChecked` and `GetEntityCheckedAsync`, which return an
`EntityResult` (`ok`, `found`, `row`, `failure`, `http_status`). A 404 `ResourceNotFound`
is a definite absence (`ok`, not `found`); every other non-200 outcome is a failure.
`GetEntity` and `GetEntityAsync` are unchanged for callers: they cannot tell failure
from empty, so use the Checked form.

Version 0.3.0 makes whole numbers round-trip exactly. A write sends every integer property outside the
32-bit range as a decimal string with a sibling `<key>@odata.type` of `Edm.Int64`, so the service keeps a
64-bit integer instead of an approximate decimal; reads (`GetEntity`, `GetEntityChecked`, `QueryEntities`,
`QueryEntitiesChecked` and their async forms) turn those annotated values back into JSON integers and strip
the metadata keys. A write holding an unsigned value above `INT64_MAX` is refused (returns false). The
change is additive: no signature changed. `tests/test_azure_table_client` has an opt-in case that talks to a
real account (tag `[real-account]`, hidden from `make check`, skipped unless `AZURE_TABLE_ACCOUNT`,
`AZURE_TABLE_ENDPOINT` and `AZURE_TABLE_KEY` are all set); run it with
`./test_azure_table_client "[real-account]"`. It prints no key, endpoint or row body.

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
