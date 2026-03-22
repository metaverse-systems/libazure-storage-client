#include <catch2/catch_all.hpp>
#include <libazure-storage-client/azure-storage-client.hpp>

#include <future>
#include <string>
#include <vector>

// Azurite well-known development credentials (not real credentials)
static const std::string ACCOUNT_NAME = "devstoreaccount1";
static const std::string ACCOUNT_KEY =
    "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/"
    "K1SZFPTOtr/KBHBeksoGMGw==";
static const std::string TABLE_ENDPOINT =
    "http://127.0.0.1:10002/devstoreaccount1";
static const std::string TEST_TABLE = "CatchTestTable";

// Shared table fixture — creates the table once before all tests
struct TableFixture : Catch::EventListenerBase {
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const &) override {
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);
        client.CreateTableIfNotExists(TEST_TABLE);
    }
};

CATCH_REGISTER_LISTENER(TableFixture)

// ---------------------------------------------------------------------------
// Sync CRUD Tests [sync]
// ---------------------------------------------------------------------------

TEST_CASE("CreateTableIfNotExists is idempotent", "[sync]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    bool first = client.CreateTableIfNotExists(TEST_TABLE);
    REQUIRE(first == true);

    bool second = client.CreateTableIfNotExists(TEST_TABLE);
    REQUIRE(second == true);
}

TEST_CASE("UpsertEntity and GetEntity round-trip", "[sync]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "sync_upsert";
    entity["RowKey"] = "row1";
    entity["Name"] = "Alice";
    entity["Value"] = 42;

    bool ok = client.UpsertEntity(TEST_TABLE, entity);
    REQUIRE(ok == true);

    auto result = client.GetEntity(TEST_TABLE, "sync_upsert", "row1");
    REQUIRE(result.contains("PartitionKey"));
    REQUIRE(result["Name"].get<std::string>() == "Alice");
    REQUIRE(result["Value"].get<int>() == 42);
}

TEST_CASE("DeleteEntity removes entity", "[sync]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "sync_delete";
    entity["RowKey"] = "row1";
    entity["Name"] = "ToDelete";

    bool ok = client.UpsertEntity(TEST_TABLE, entity);
    REQUIRE(ok == true);

    bool deleted = client.DeleteEntity(TEST_TABLE, "sync_delete", "row1");
    REQUIRE(deleted == true);

    auto result = client.GetEntity(TEST_TABLE, "sync_delete", "row1");
    REQUIRE(result.empty());
}

TEST_CASE("QueryEntities filters correctly", "[sync]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json e1;
    e1["PartitionKey"] = "sync_query";
    e1["RowKey"] = "match1";
    e1["Name"] = "Match";

    nlohmann::json e2;
    e2["PartitionKey"] = "sync_query";
    e2["RowKey"] = "match2";
    e2["Name"] = "Match";

    nlohmann::json e3;
    e3["PartitionKey"] = "sync_query_other";
    e3["RowKey"] = "nomatch1";
    e3["Name"] = "NoMatch";

    REQUIRE(client.UpsertEntity(TEST_TABLE, e1));
    REQUIRE(client.UpsertEntity(TEST_TABLE, e2));
    REQUIRE(client.UpsertEntity(TEST_TABLE, e3));

    auto results = client.QueryEntities(TEST_TABLE,
        "PartitionKey eq 'sync_query'");

    REQUIRE(results.size() >= 2);
    for (const auto &r : results) {
        REQUIRE(r["PartitionKey"].get<std::string>() == "sync_query");
    }
}

TEST_CASE("BatchUpsertEntities persists multiple entities", "[sync]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    std::vector<nlohmann::json> entities;
    for (int i = 0; i < 3; i++) {
        nlohmann::json e;
        e["PartitionKey"] = "sync_batch";
        e["RowKey"] = "row" + std::to_string(i);
        e["Name"] = "Batch" + std::to_string(i);
        entities.push_back(e);
    }

    bool ok = client.BatchUpsertEntities(TEST_TABLE, entities);
    REQUIRE(ok == true);

    for (int i = 0; i < 3; i++) {
        auto result = client.GetEntity(TEST_TABLE, "sync_batch",
                                       "row" + std::to_string(i));
        REQUIRE(result.contains("PartitionKey"));
        REQUIRE(result["Name"].get<std::string>() == "Batch" + std::to_string(i));
    }
}

// ---------------------------------------------------------------------------
// Async CRUD Tests [async]
// ---------------------------------------------------------------------------

TEST_CASE("GetEntityAsync retrieves entity via callback", "[async]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "async_get";
    entity["RowKey"] = "row1";
    entity["Name"] = "AsyncGet";
    REQUIRE(client.UpsertEntity(TEST_TABLE, entity));

    std::promise<nlohmann::json> promise;
    auto future = promise.get_future();

    client.GetEntityAsync(TEST_TABLE, "async_get", "row1",
        [&promise](nlohmann::json result) {
            promise.set_value(std::move(result));
        });

    auto result = future.get();
    REQUIRE(result.contains("PartitionKey"));
    REQUIRE(result["Name"].get<std::string>() == "AsyncGet");
}

TEST_CASE("UpsertEntityAsync inserts entity via callback", "[async]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "async_upsert";
    entity["RowKey"] = "row1";
    entity["Name"] = "AsyncUpsert";

    std::promise<bool> promise;
    auto future = promise.get_future();

    client.UpsertEntityAsync(TEST_TABLE, entity,
        [&promise](bool ok) {
            promise.set_value(ok);
        });

    REQUIRE(future.get() == true);

    auto result = client.GetEntity(TEST_TABLE, "async_upsert", "row1");
    REQUIRE(result["Name"].get<std::string>() == "AsyncUpsert");
}

TEST_CASE("DeleteEntityAsync removes entity via callback", "[async]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "async_delete";
    entity["RowKey"] = "row1";
    entity["Name"] = "AsyncDelete";
    REQUIRE(client.UpsertEntity(TEST_TABLE, entity));

    std::promise<bool> promise;
    auto future = promise.get_future();

    client.DeleteEntityAsync(TEST_TABLE, "async_delete", "row1",
        [&promise](bool ok) {
            promise.set_value(ok);
        });

    REQUIRE(future.get() == true);

    auto result = client.GetEntity(TEST_TABLE, "async_delete", "row1");
    REQUIRE(result.empty());
}

TEST_CASE("QueryEntitiesAsync returns matching entities via callback", "[async]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json e1;
    e1["PartitionKey"] = "async_query";
    e1["RowKey"] = "row1";
    e1["Name"] = "AQ1";

    nlohmann::json e2;
    e2["PartitionKey"] = "async_query";
    e2["RowKey"] = "row2";
    e2["Name"] = "AQ2";

    REQUIRE(client.UpsertEntity(TEST_TABLE, e1));
    REQUIRE(client.UpsertEntity(TEST_TABLE, e2));

    std::promise<std::vector<nlohmann::json>> promise;
    auto future = promise.get_future();

    client.QueryEntitiesAsync(TEST_TABLE, "PartitionKey eq 'async_query'",
        [&promise](std::vector<nlohmann::json> results) {
            promise.set_value(std::move(results));
        });

    auto results = future.get();
    REQUIRE(results.size() >= 2);
    for (const auto &r : results) {
        REQUIRE(r["PartitionKey"].get<std::string>() == "async_query");
    }
}

TEST_CASE("BatchUpsertEntitiesAsync persists entities via callback", "[async]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    std::vector<nlohmann::json> entities;
    for (int i = 0; i < 3; i++) {
        nlohmann::json e;
        e["PartitionKey"] = "async_batch";
        e["RowKey"] = "row" + std::to_string(i);
        e["Name"] = "AB" + std::to_string(i);
        entities.push_back(e);
    }

    std::promise<bool> promise;
    auto future = promise.get_future();

    client.BatchUpsertEntitiesAsync(TEST_TABLE, entities,
        [&promise](bool ok) {
            promise.set_value(ok);
        });

    REQUIRE(future.get() == true);

    for (int i = 0; i < 3; i++) {
        auto result = client.GetEntity(TEST_TABLE, "async_batch",
                                       "row" + std::to_string(i));
        REQUIRE(result.contains("PartitionKey"));
    }
}

// ---------------------------------------------------------------------------
// Edge Case Tests [edge]
// ---------------------------------------------------------------------------

TEST_CASE("UpsertEntity with missing PartitionKey returns false", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["RowKey"] = "row1";
    entity["Name"] = "NoPartitionKey";

    REQUIRE(client.UpsertEntity(TEST_TABLE, entity) == false);
}

TEST_CASE("UpsertEntity with missing RowKey returns false", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity;
    entity["PartitionKey"] = "edge_norowkey";
    entity["Name"] = "NoRowKey";

    REQUIRE(client.UpsertEntity(TEST_TABLE, entity) == false);
}

TEST_CASE("BatchUpsertEntities with >100 entities returns false", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    std::vector<nlohmann::json> entities;
    for (int i = 0; i < 101; i++) {
        nlohmann::json e;
        e["PartitionKey"] = "edge_batch_overflow";
        e["RowKey"] = "row" + std::to_string(i);
        entities.push_back(e);
    }

    REQUIRE(client.BatchUpsertEntities(TEST_TABLE, entities) == false);
}

TEST_CASE("GetEntity with non-existent entity returns empty JSON", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    auto result = client.GetEntity(TEST_TABLE, "edge_noent", "does_not_exist");
    REQUIRE(result.empty());
}

TEST_CASE("QueryEntities with filter matching nothing returns empty vector", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    auto results = client.QueryEntities(TEST_TABLE,
        "PartitionKey eq 'edge_empty_no_such_pk'");
    REQUIRE(results.empty());
}

TEST_CASE("UpsertEntity with empty entity returns false", "[edge]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity = nlohmann::json::object();

    REQUIRE(client.UpsertEntity(TEST_TABLE, entity) == false);
}

// ---------------------------------------------------------------------------
// Auth Dispatch Tests [auth]
// ---------------------------------------------------------------------------

TEST_CASE("Bearer Token constructor and SetBearerToken do not crash", "[auth]") {
    // Construct a client using the Bearer Token constructor
    AzureTableClient client(TABLE_ENDPOINT, "initial_fake_token");

    // Update the token via SetBearerToken
    client.SetBearerToken("updated_fake_token");

    // Attempt a request — Azurite does not support Bearer auth so this
    // should return false (401/403), but must not crash or hang
    bool result = client.CreateTableIfNotExists(TEST_TABLE);
    REQUIRE(result == false);
}

// ---------------------------------------------------------------------------
// Pagination Test [sync]
// ---------------------------------------------------------------------------

TEST_CASE("QueryEntities handles pagination with >1000 entities", "[sync][pagination]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    // Insert 1001 entities in batches of 100
    const int TOTAL = 1001;
    const int BATCH_SIZE = 100;
    for (int start = 0; start < TOTAL; start += BATCH_SIZE) {
        int end = std::min(start + BATCH_SIZE, TOTAL);
        std::vector<nlohmann::json> batch;
        for (int i = start; i < end; i++) {
            nlohmann::json e;
            e["PartitionKey"] = "sync_pagination";
            e["RowKey"] = "r" + std::to_string(i);
            batch.push_back(e);
        }
        REQUIRE(client.BatchUpsertEntities(TEST_TABLE, batch) == true);
    }

    // The last entity (index 1000) goes as a single upsert since 1001 % 100 = 1
    // Actually the loop above handles it: batch 10 has 1 entity (index 1000)

    auto results = client.QueryEntities(TEST_TABLE,
        "PartitionKey eq 'sync_pagination'");
    REQUIRE(results.size() == TOTAL);
}
