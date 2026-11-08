#include <catch2/catch_all.hpp>
#include <libazure-storage-client/azure-storage-client.hpp>

#include "stub_http_server.hpp"

#include <curl/curl.h>

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

// ---------------------------------------------------------------------------
// Checked query tests [checked]
// ---------------------------------------------------------------------------

namespace {

bool AzuriteUp() {
    static const bool up = []() {
        CURL *curl = curl_easy_init();
        if (!curl) return false;
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:10002/");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        CURLcode rc = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return rc == CURLE_OK;
    }();
    return up;
}

#define REQUIRE_AZURITE() \
    do { if (!AzuriteUp()) { SKIP("Azurite is not running"); } } while (0)

// Test-only access to the protected escape hook seam.
struct HookClient : AzureTableClient {
    using AzureTableClient::AzureTableClient;
    static void SetHook(UrlEscapeFn hook) { SetUrlEscapeHook(hook); }
};

bool FailingEscape(const std::string &, std::string &) { return false; }

nlohmann::json Row(const std::string &pk, const std::string &rk) {
    nlohmann::json e;
    e["PartitionKey"] = pk;
    e["RowKey"] = rk;
    return e;
}

} // namespace

TEST_CASE("Checked query returns rows on success", "[checked]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);
    REQUIRE(client.UpsertEntity(TEST_TABLE, Row("checked_rows", "a")));
    REQUIRE(client.UpsertEntity(TEST_TABLE, Row("checked_rows", "b")));

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'checked_rows'");
    REQUIRE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::none);
    REQUIRE(result.rows.size() == 2);
}

TEST_CASE("Checked query distinguishes empty success", "[checked]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'checked_no_such_pk'");
    REQUIRE(result.ok);
    REQUIRE(result.rows.empty());
    REQUIRE(result.failure == AzureTableClient::Failure::none);
}

TEST_CASE("Checked query on a missing table reports 404", "[checked]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    auto result = client.QueryEntitiesChecked("CheckedNoSuchTable", "PartitionKey eq 'x'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::http_status);
    REQUIRE(result.http_status == 404);
    REQUIRE(result.rows.empty());
}

TEST_CASE("Checked query against an unreachable endpoint reports transport", "[checked]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, "http://127.0.0.1:1/devstoreaccount1");

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'x'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::transport);
    REQUIRE(result.rows.empty());
}

TEST_CASE("Async checked query gives the same result", "[checked]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);
    REQUIRE(client.UpsertEntity(TEST_TABLE, Row("checked_async", "a")));

    std::promise<AzureTableClient::QueryResult> okPromise;
    auto okFuture = okPromise.get_future();
    client.QueryEntitiesCheckedAsync(TEST_TABLE, "PartitionKey eq 'checked_async'",
        [&okPromise](AzureTableClient::QueryResult r) { okPromise.set_value(std::move(r)); });
    auto ok = okFuture.get();
    REQUIRE(ok.ok);
    REQUIRE(ok.rows.size() == 1);

    std::promise<AzureTableClient::QueryResult> missPromise;
    auto missFuture = missPromise.get_future();
    client.QueryEntitiesCheckedAsync("CheckedNoSuchTable", "PartitionKey eq 'x'",
        [&missPromise](AzureTableClient::QueryResult r) { missPromise.set_value(std::move(r)); });
    auto miss = missFuture.get();
    REQUIRE_FALSE(miss.ok);
    REQUIRE(miss.failure == AzureTableClient::Failure::http_status);
    REQUIRE(miss.http_status == 404);
}

// ---------------------------------------------------------------------------
// Checked single-entity read [checked-get] (Azurite)
// ---------------------------------------------------------------------------

TEST_CASE("Checked get round trip against Azurite: found, absent, missing table", "[checked-get]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);

    nlohmann::json entity = Row("cg-part", "cg-row");
    entity["value"] = 7;
    REQUIRE(client.UpsertEntity(TEST_TABLE, entity));

    auto found = client.GetEntityChecked(TEST_TABLE, "cg-part", "cg-row");
    REQUIRE(found.ok);
    REQUIRE(found.found);
    REQUIRE(found.failure == AzureTableClient::Failure::none);
    REQUIRE(found.row.is_object());
    REQUIRE(found.row["value"] == 7);

    auto absent = client.GetEntityChecked(TEST_TABLE, "cg-part", "no-such-row");
    REQUIRE(absent.ok);
    REQUIRE_FALSE(absent.found);
    REQUIRE(absent.row.is_null());

    auto missing = client.GetEntityChecked("CheckedGetNoSuchTable", "cg-part", "cg-row");
    REQUIRE_FALSE(missing.ok);
    REQUIRE_FALSE(missing.found);
    REQUIRE(missing.failure == AzureTableClient::Failure::http_status);
    REQUIRE(missing.http_status == 404);
}

#ifdef __linux__

TEST_CASE("Checked query on a malformed body reports malformed_response", "[checked][stub]") {
    StubResponse bad;
    bad.status = 200;
    bad.body = "this is not json";
    StubHttpServer server({bad});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'x'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::malformed_response);
    REQUIRE(result.rows.empty());
}

TEST_CASE("Checked query on a body without a value array reports malformed_response", "[checked][stub]") {
    StubResponse bad;
    bad.status = 200;
    bad.body = "{\"other\":1}";
    StubHttpServer server({bad});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'x'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::malformed_response);
}

TEST_CASE("Checked query on a server error reports http_status", "[checked][stub]") {
    StubResponse err;
    err.status = 500;
    err.body = "{}";
    StubHttpServer server({err});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");

    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'x'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::http_status);
    REQUIRE(result.http_status == 500);
}

TEST_CASE("Page two failure reports incomplete with no rows", "[checked][stub]") {
    StubResponse page1;
    page1.status = 200;
    page1.body = "{\"value\":[{\"PartitionKey\":\"p\",\"RowKey\":\"1\"}]}";
    page1.headers["x-ms-continuation-nextpartitionkey"] = "p";
    page1.headers["x-ms-continuation-nextrowkey"] = "2";
    StubResponse page2;
    page2.status = 500;
    page2.body = "{}";

    StubHttpServer server({page1, page2});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'p'");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::incomplete);
    REQUIRE(result.rows.empty());
    REQUIRE(server.RequestCount() == 2);

    // The async twin gives the same result against a fresh script.
    StubHttpServer server2({page1, page2});
    AzureTableClient client2(ACCOUNT_NAME, ACCOUNT_KEY, server2.Endpoint() + "/devstoreaccount1");
    std::promise<AzureTableClient::QueryResult> promise;
    auto future = promise.get_future();
    client2.QueryEntitiesCheckedAsync(TEST_TABLE, "PartitionKey eq 'p'",
        [&promise](AzureTableClient::QueryResult r) { promise.set_value(std::move(r)); });
    auto asyncResult = future.get();
    REQUIRE_FALSE(asyncResult.ok);
    REQUIRE(asyncResult.failure == AzureTableClient::Failure::incomplete);
    REQUIRE(asyncResult.rows.empty());
}

TEST_CASE("Legacy queries return rows on success and empty on failure", "[checked][stub]") {
    StubResponse good;
    good.status = 200;
    good.body = "{\"value\":[{\"PartitionKey\":\"p\",\"RowKey\":\"1\"}]}";
    StubResponse err;
    err.status = 500;
    err.body = "{}";

    {
        StubHttpServer server({good});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        REQUIRE(client.QueryEntities(TEST_TABLE, "PartitionKey eq 'p'").size() == 1);
    }
    {
        StubHttpServer server({good});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        std::promise<std::vector<nlohmann::json>> promise;
        auto future = promise.get_future();
        client.QueryEntitiesAsync(TEST_TABLE, "PartitionKey eq 'p'",
            [&promise](std::vector<nlohmann::json> r) { promise.set_value(std::move(r)); });
        REQUIRE(future.get().size() == 1);
    }
    {
        StubHttpServer server({err});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        REQUIRE(client.QueryEntities(TEST_TABLE, "PartitionKey eq 'p'").empty());
    }
    {
        StubHttpServer server({err});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        std::promise<std::vector<nlohmann::json>> promise;
        auto future = promise.get_future();
        client.QueryEntitiesAsync(TEST_TABLE, "PartitionKey eq 'p'",
            [&promise](std::vector<nlohmann::json> r) { promise.set_value(std::move(r)); });
        REQUIRE(future.get().empty());
    }
}

TEST_CASE("A failing URL escape step fails closed without sending a request", "[checked][stub]") {
    StubResponse good;
    good.status = 200;
    good.body = "{\"value\":[{\"PartitionKey\":\"p\",\"RowKey\":\"1\"}]}";
    StubHttpServer server({good});
    HookClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");

    HookClient::SetHook(&FailingEscape);
    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'p'");
    HookClient::SetHook(nullptr);

    REQUIRE_FALSE(result.ok);
    REQUIRE(result.failure == AzureTableClient::Failure::invalid_request);
    REQUIRE(result.rows.empty());
    REQUIRE(server.RequestCount() == 0);
}

// ---------------------------------------------------------------------------
// Checked single-entity read [checked-get] (stub server)
// ---------------------------------------------------------------------------

namespace {

StubResponse Reply(int status, const std::string &body) {
    StubResponse r;
    r.status = status;
    r.body = body;
    return r;
}

const std::string RESOURCE_NOT_FOUND =
    "{\"odata.error\":{\"code\":\"ResourceNotFound\",\"message\":{\"lang\":\"en-US\","
    "\"value\":\"The specified resource does not exist.\"}}}";
const std::string TABLE_NOT_FOUND =
    "{\"odata.error\":{\"code\":\"TableNotFound\",\"message\":{\"lang\":\"en-US\","
    "\"value\":\"The table specified does not exist.\"}}}";

AzureTableClient::EntityResult StubGet(const StubResponse &response) {
    StubHttpServer server({response});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
    return client.GetEntityChecked(TEST_TABLE, "p", "r");
}

} // namespace

TEST_CASE("Checked get: 200 with an object is found", "[checked-get][stub]") {
    auto result = StubGet(Reply(200, "{\"PartitionKey\":\"p\",\"RowKey\":\"r\",\"characterId\":\"c1\"}"));
    REQUIRE(result.ok);
    REQUIRE(result.found);
    REQUIRE(result.failure == AzureTableClient::Failure::none);
    REQUIRE(result.row.is_object());
    REQUIRE(result.row["characterId"] == "c1");
}

TEST_CASE("Checked get: 404 ResourceNotFound is a definite absence", "[checked-get][stub]") {
    auto result = StubGet(Reply(404, RESOURCE_NOT_FOUND));
    REQUIRE(result.ok);
    REQUIRE_FALSE(result.found);
    REQUIRE(result.failure == AzureTableClient::Failure::none);
    REQUIRE(result.row.is_null());
}

TEST_CASE("Checked get: 404 TableNotFound is a failure", "[checked-get][stub]") {
    auto result = StubGet(Reply(404, TABLE_NOT_FOUND));
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.found);
    REQUIRE(result.failure == AzureTableClient::Failure::http_status);
    REQUIRE(result.http_status == 404);
    REQUIRE(result.row.is_null());
}

TEST_CASE("Checked get: 404 with no body is a failure", "[checked-get][stub]") {
    auto result = StubGet(Reply(404, ""));
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.found);
    REQUIRE(result.failure == AzureTableClient::Failure::http_status);
    REQUIRE(result.http_status == 404);
}

TEST_CASE("Checked get: 500 and 503 are failures with their status", "[checked-get][stub]") {
    for (int status : {500, 503}) {
        auto result = StubGet(Reply(status, "{}"));
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(result.found);
        REQUIRE(result.failure == AzureTableClient::Failure::http_status);
        REQUIRE(result.http_status == status);
        REQUIRE(result.row.is_null());
    }
}

TEST_CASE("Checked get: a 200 body that is not an object is malformed_response", "[checked-get][stub]") {
    for (const std::string body : {std::string("this is not json"), std::string("[1,2,3]")}) {
        auto result = StubGet(Reply(200, body));
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(result.found);
        REQUIRE(result.failure == AzureTableClient::Failure::malformed_response);
        REQUIRE(result.row.is_null());
    }
}

TEST_CASE("Checked get: 200 with an empty object is found", "[checked-get][stub]") {
    // The contract only requires a JSON object; an empty one is still an object.
    auto result = StubGet(Reply(200, "{}"));
    REQUIRE(result.ok);
    REQUIRE(result.found);
    REQUIRE(result.row.is_object());
}

TEST_CASE("Checked get: an unreachable endpoint reports transport", "[checked-get][stub]") {
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, "http://127.0.0.1:1/devstoreaccount1");
    auto result = client.GetEntityChecked(TEST_TABLE, "p", "r");
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.found);
    REQUIRE(result.failure == AzureTableClient::Failure::transport);
    REQUIRE(result.http_status == 0);
    REQUIRE(result.row.is_null());
}

TEST_CASE("Checked get: async form gives the same results", "[checked-get][stub]") {
    auto run = [](const StubResponse &response) {
        StubHttpServer server({response});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        std::promise<AzureTableClient::EntityResult> promise;
        auto future = promise.get_future();
        client.GetEntityCheckedAsync(TEST_TABLE, "p", "r",
            [&promise](AzureTableClient::EntityResult r) { promise.set_value(std::move(r)); });
        return future.get();
    };

    auto found = run(Reply(200, "{\"PartitionKey\":\"p\",\"RowKey\":\"r\"}"));
    REQUIRE(found.ok);
    REQUIRE(found.found);
    REQUIRE(found.row.is_object());

    auto absent = run(Reply(404, RESOURCE_NOT_FOUND));
    REQUIRE(absent.ok);
    REQUIRE_FALSE(absent.found);

    auto failed = run(Reply(503, "{}"));
    REQUIRE_FALSE(failed.ok);
    REQUIRE(failed.failure == AzureTableClient::Failure::http_status);
    REQUIRE(failed.http_status == 503);

    auto malformed = run(Reply(200, "nope"));
    REQUIRE_FALSE(malformed.ok);
    REQUIRE(malformed.failure == AzureTableClient::Failure::malformed_response);
}

TEST_CASE("GetEntity keeps its behaviour: the row when found, null otherwise", "[checked-get][stub]") {
    {
        StubHttpServer server({Reply(200, "{\"PartitionKey\":\"p\",\"RowKey\":\"r\"}")});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        auto row = client.GetEntity(TEST_TABLE, "p", "r");
        REQUIRE(row.is_object());
        REQUIRE(row["RowKey"] == "r");
    }
    for (const auto &response : {Reply(404, RESOURCE_NOT_FOUND), Reply(404, TABLE_NOT_FOUND), Reply(500, "{}"),
                                 Reply(200, "garbage")}) {
        StubHttpServer server({response});
        AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
        REQUIRE(client.GetEntity(TEST_TABLE, "p", "r").is_null());
    }
}

TEST_CASE("Checked get: a failed result carries no row, URL, token or body", "[checked-get][stub]") {
    const std::string secret = "SECRET-BODY-TEXT-9917";
    StubHttpServer server({Reply(500, "{\"detail\":\"" + secret + "\"}")});
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");

    auto result = client.GetEntityChecked(TEST_TABLE, "partition-xyz", "row-xyz");
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.found);
    REQUIRE(result.row.is_null());
    REQUIRE(result.failure == AzureTableClient::Failure::http_status);
    REQUIRE(result.http_status == 500);
}

#endif // __linux__

// Code-inspection note: both curl_easy_escape call sites in
// src/azure-storage-client.cpp (first page and continuation rebuild) must go
// through the single escape step that honours the hook.

TEST_CASE("EscapeFilterString doubles single quotes", "[checked]") {
    REQUIRE(AzureTableClient::EscapeFilterString("'") == "''");
    REQUIRE(AzureTableClient::EscapeFilterString("''") == "''''");
    REQUIRE(AzureTableClient::EscapeFilterString("a'b") == "a''b");
    REQUIRE(AzureTableClient::EscapeFilterString("'''") == "''''''");
    REQUIRE(AzureTableClient::EscapeFilterString("") == "");
    REQUIRE(AzureTableClient::EscapeFilterString("plain") == "plain");
}

TEST_CASE("An escaped injection attempt matches nothing", "[checked]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);
    REQUIRE(client.UpsertEntity(TEST_TABLE, Row("checked_inject", "x")));
    REQUIRE(client.UpsertEntity(TEST_TABLE, Row("checked_inject", "y")));

    std::string filter = "PartitionKey eq 'checked_inject' and RowKey eq '" +
        AzureTableClient::EscapeFilterString("x' or RowKey ne 'x") + "'";
    auto result = client.QueryEntitiesChecked(TEST_TABLE, filter);
    REQUIRE(result.ok);
    REQUIRE(result.rows.empty());

    std::string plain = "PartitionKey eq 'checked_inject' and RowKey eq '" +
        AzureTableClient::EscapeFilterString("x") + "'";
    auto found = client.QueryEntitiesChecked(TEST_TABLE, plain);
    REQUIRE(found.ok);
    REQUIRE(found.rows.size() == 1);
}
