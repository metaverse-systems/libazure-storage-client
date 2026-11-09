#include <catch2/catch_all.hpp>
#include <libazure-storage-client/azure-storage-client.hpp>

#include "stub_http_server.hpp"

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>

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

// ---------------------------------------------------------------------------
// Large-integer wire encoding [wire] and reading [reader]
// ---------------------------------------------------------------------------

namespace {

using json = nlohmann::json;

constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::min();
constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
constexpr int64_t kTwo53 = 9007199254740992LL;

// The integers that must travel as annotated strings (outside int32).
std::vector<int64_t> LargeIntegers() {
    return {1700000000000LL, -1700000000000LL, 2147483648LL, -2147483649LL, kTwo53, -kTwo53, kInt64Min, kInt64Max};
}

// The integers that must stay bare numbers (inside int32).
std::vector<int64_t> SmallIntegers() {
    return {0, 5, -5, 2147483647LL, -2147483648LL};
}

std::string PropName(size_t index) { return "p" + std::to_string(index); }

} // namespace

#ifdef __linux__

namespace {

// The JSON bodies of every sub-request in a captured multipart batch body.
std::vector<json> BatchBodies(const std::string &body) {
    std::vector<json> out;
    size_t pos = 0;
    while ((pos = body.find("PUT ", pos)) != std::string::npos) {
        size_t blank = body.find("\r\n\r\n", pos);
        if (blank == std::string::npos) break;
        size_t start = blank + 4;
        size_t end = body.find("\r\n", start);
        out.push_back(json::parse(body.substr(start, end - start), nullptr, false));
        pos = end;
    }
    return out;
}

json LastBody(StubHttpServer &server) { return json::parse(server.LastRequest().body, nullptr, false); }

AzureTableClient StubClient(StubHttpServer &server) {
    return AzureTableClient(ACCOUNT_NAME, ACCOUNT_KEY, server.Endpoint() + "/devstoreaccount1");
}

json WideRow() {
    json e = Row("wire", "r");
    size_t i = 0;
    for (int64_t v : LargeIntegers()) e[PropName(i++)] = v;
    for (int64_t v : SmallIntegers()) e[PropName(i++)] = v;
    return e;
}

void CheckWideBody(const json &body) {
    REQUIRE(body.is_object());
    REQUIRE(body["PartitionKey"] == "wire");
    REQUIRE(body["RowKey"] == "r");
    REQUIRE_FALSE(body.contains("PartitionKey@odata.type"));
    REQUIRE_FALSE(body.contains("RowKey@odata.type"));
    size_t i = 0;
    for (int64_t v : LargeIntegers()) {
        const std::string name = PropName(i++);
        INFO("large value " << v);
        REQUIRE(body.contains(name));
        REQUIRE(body[name].is_string());
        REQUIRE(body[name].get<std::string>() == std::to_string(v));
        REQUIRE(body.contains(name + "@odata.type"));
        REQUIRE(body[name + "@odata.type"] == "Edm.Int64");
    }
    for (int64_t v : SmallIntegers()) {
        const std::string name = PropName(i++);
        INFO("small value " << v);
        REQUIRE(body[name].is_number_integer());
        REQUIRE(body[name].get<int64_t>() == v);
        REQUIRE_FALSE(body.contains(name + "@odata.type"));
    }
}

} // namespace

TEST_CASE("Wire: UpsertEntity sends integers outside int32 as annotated Edm.Int64 strings", "[wire][stub]") {
    StubHttpServer server({Reply(204, "")});
    auto client = StubClient(server);
    REQUIRE(client.UpsertEntity(TEST_TABLE, WideRow()));
    REQUIRE(server.RequestCount() == 1);
    CheckWideBody(LastBody(server));
}

TEST_CASE("Wire: the 1700000000000 clock value is a string with the annotation and 5 stays bare", "[wire][stub]") {
    StubHttpServer server({Reply(204, "")});
    auto client = StubClient(server);
    json e = Row("wire", "clock");
    e["anchor_wall_ms"] = 1700000000000LL;
    e["count"] = 5;
    REQUIRE(client.UpsertEntity(TEST_TABLE, e));
    json body = LastBody(server);
    REQUIRE(body["anchor_wall_ms"] == "1700000000000");
    REQUIRE(body["anchor_wall_ms@odata.type"] == "Edm.Int64");
    REQUIRE(body["count"].is_number_integer());
    REQUIRE_FALSE(body.contains("count@odata.type"));
}

TEST_CASE("Wire: UpsertEntityAsync sends the same body", "[wire][stub]") {
    StubHttpServer server({Reply(204, "")});
    auto client = StubClient(server);
    std::promise<bool> promise;
    auto future = promise.get_future();
    client.UpsertEntityAsync(TEST_TABLE, WideRow(), [&promise](bool ok) { promise.set_value(ok); });
    REQUIRE(future.get());
    CheckWideBody(LastBody(server));
}

TEST_CASE("Wire: a caller-supplied @odata.type annotation is left alone", "[wire][stub]") {
    StubHttpServer server({Reply(204, "")});
    auto client = StubClient(server);
    json e = Row("wire", "own");
    e["when"] = "2026-01-01T00:00:00Z";
    e["when@odata.type"] = "Edm.DateTime";
    e["big"] = 1700000000000LL;
    REQUIRE(client.UpsertEntity(TEST_TABLE, e));
    json body = LastBody(server);
    REQUIRE(body["when"] == "2026-01-01T00:00:00Z");
    REQUIRE(body["when@odata.type"] == "Edm.DateTime");
    REQUIRE(body["big"] == "1700000000000");
    REQUIRE(body["big@odata.type"] == "Edm.Int64");
}

TEST_CASE("Wire: BatchUpsertEntities carries the annotation in every sub-request", "[wire][stub]") {
    StubResponse ok = Reply(202, "--b\r\nHTTP/1.1 204 No Content\r\n\r\n--b--\r\n");
    StubHttpServer server({ok});
    auto client = StubClient(server);
    json second = WideRow();
    second["RowKey"] = "r2";
    REQUIRE(client.BatchUpsertEntities(TEST_TABLE, {WideRow(), second}));
    auto bodies = BatchBodies(server.LastRequest().body);
    REQUIRE(bodies.size() == 2);
    CheckWideBody(bodies[0]);
    REQUIRE(bodies[1]["RowKey"] == "r2");
    REQUIRE(bodies[1]["p0"] == "1700000000000");
    REQUIRE(bodies[1]["p0@odata.type"] == "Edm.Int64");
}

TEST_CASE("Wire: BatchUpsertEntitiesAsync carries the annotation", "[wire][stub]") {
    StubHttpServer server({Reply(202, "--b\r\nHTTP/1.1 204 No Content\r\n\r\n--b--\r\n")});
    auto client = StubClient(server);
    std::promise<bool> promise;
    auto future = promise.get_future();
    client.BatchUpsertEntitiesAsync(TEST_TABLE, {WideRow()}, [&promise](bool ok) { promise.set_value(ok); });
    REQUIRE(future.get());
    auto bodies = BatchBodies(server.LastRequest().body);
    REQUIRE(bodies.size() == 1);
    CheckWideBody(bodies[0]);
}

TEST_CASE("Wire: an unsigned integer above INT64_MAX is refused and nothing is sent", "[wire][stub]") {
    const uint64_t tooBig = static_cast<uint64_t>(kInt64Max) + 1;
    json e = Row("wire", "u");
    e["huge"] = tooBig;
    REQUIRE(e["huge"].is_number_unsigned());

    {
        StubHttpServer server({Reply(204, "")});
        auto client = StubClient(server);
        REQUIRE_FALSE(client.UpsertEntity(TEST_TABLE, e));
        REQUIRE(server.RequestCount() == 0);
    }
    {
        StubHttpServer server({Reply(202, "")});
        auto client = StubClient(server);
        REQUIRE_FALSE(client.BatchUpsertEntities(TEST_TABLE, {Row("wire", "ok"), e}));
        REQUIRE(server.RequestCount() == 0);
    }
    {
        StubHttpServer server({Reply(204, "")});
        auto client = StubClient(server);
        std::promise<bool> promise;
        auto future = promise.get_future();
        client.UpsertEntityAsync(TEST_TABLE, e, [&promise](bool ok) { promise.set_value(ok); });
        REQUIRE_FALSE(future.get());
        REQUIRE(server.RequestCount() == 0);
    }
    {
        StubHttpServer server({Reply(202, "")});
        auto client = StubClient(server);
        std::promise<bool> promise;
        auto future = promise.get_future();
        client.BatchUpsertEntitiesAsync(TEST_TABLE, {e}, [&promise](bool ok) { promise.set_value(ok); });
        REQUIRE_FALSE(future.get());
        REQUIRE(server.RequestCount() == 0);
    }
}

TEST_CASE("Wire: an unsigned integer within INT64_MAX is sent like a signed one", "[wire][stub]") {
    StubHttpServer server({Reply(204, "")});
    auto client = StubClient(server);
    json e = Row("wire", "u2");
    e["val"] = static_cast<uint64_t>(kInt64Max);
    REQUIRE(client.UpsertEntity(TEST_TABLE, e));
    json body = LastBody(server);
    REQUIRE(body["val"] == "9223372036854775807");
    REQUIRE(body["val@odata.type"] == "Edm.Int64");
}

// ---- reader ----

namespace {

std::string AnnotatedRow() {
    json row = {{"PartitionKey", "p"}, {"RowKey", "r"}, {"odata.metadata", "http://x/$metadata#t/@Element"},
                {"odata.etag", "W/\"datetime'2026'\""}, {"Timestamp", "2026-01-01T00:00:00.0000000Z"},
                {"Timestamp@odata.type", "Edm.DateTime"}, {"plain", 7}, {"text", "[5]"}};
    size_t i = 0;
    for (int64_t v : {int64_t{0}, int64_t{2147483648LL}, int64_t{-2147483649LL}, kTwo53, kInt64Min, kInt64Max}) {
        row[PropName(i)] = std::to_string(v);
        row[PropName(i) + "@odata.type"] = "Edm.Int64";
        ++i;
    }
    row["badint"] = "12x";
    row["badint@odata.type"] = "Edm.Int64";
    row["overflow"] = "9223372036854775808";
    row["overflow@odata.type"] = "Edm.Int64";
    row["dbl"] = 1.5;
    row["dbl@odata.type"] = "Edm.Double";
    row["guid"] = "0b2e1d7c-0000-4000-8000-000000000000";
    row["guid@odata.type"] = "Edm.Guid";
    return row.dump();
}

void CheckConvertedRow(const json &row) {
    REQUIRE(row.is_object());
    const std::vector<int64_t> expect = {0, 2147483648LL, -2147483649LL, kTwo53, kInt64Min, kInt64Max};
    for (size_t i = 0; i < expect.size(); ++i) {
        INFO("property " << PropName(i));
        REQUIRE(row[PropName(i)].is_number_integer());
        REQUIRE(row[PropName(i)].get<int64_t>() == expect[i]);
    }
    REQUIRE(row["plain"] == 7);
    REQUIRE(row["text"] == "[5]");
    REQUIRE(row["badint"].is_string());
    REQUIRE(row["badint"] == "12x");
    REQUIRE(row["overflow"].is_string());
    REQUIRE(row["overflow"] == "9223372036854775808");
    REQUIRE(row["dbl"] == 1.5);
    REQUIRE(row["guid"] == "0b2e1d7c-0000-4000-8000-000000000000");
    REQUIRE(row["Timestamp"] == "2026-01-01T00:00:00.0000000Z");
    for (auto it = row.begin(); it != row.end(); ++it) {
        const std::string &k = it.key();
        INFO("key " << k);
        REQUIRE(k.find("@odata.type") == std::string::npos);
        REQUIRE(k.rfind("odata.", 0) != 0);
    }
}

} // namespace

TEST_CASE("Reader: requests minimalmetadata", "[reader][stub]") {
    StubHttpServer server({Reply(200, "{\"PartitionKey\":\"p\",\"RowKey\":\"r\"}")});
    auto client = StubClient(server);
    client.GetEntityChecked(TEST_TABLE, "p", "r");
    REQUIRE(server.LastRequest().headers["accept"] == "application/json;odata=minimalmetadata");
}

TEST_CASE("Reader: query pages request minimalmetadata", "[reader][stub]") {
    StubHttpServer server({Reply(200, "{\"value\":[]}")});
    auto client = StubClient(server);
    client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'p'");
    REQUIRE(server.LastRequest().headers["accept"] == "application/json;odata=minimalmetadata");
}

TEST_CASE("Reader: GetEntityChecked converts annotated Int64 strings and strips metadata", "[reader][stub]") {
    auto result = StubGet(Reply(200, AnnotatedRow()));
    REQUIRE(result.ok);
    REQUIRE(result.found);
    CheckConvertedRow(result.row);
}

TEST_CASE("Reader: GetEntityCheckedAsync converts", "[reader][stub]") {
    StubHttpServer server({Reply(200, AnnotatedRow())});
    auto client = StubClient(server);
    std::promise<AzureTableClient::EntityResult> promise;
    auto future = promise.get_future();
    client.GetEntityCheckedAsync(TEST_TABLE, "p", "r",
        [&promise](AzureTableClient::EntityResult r) { promise.set_value(std::move(r)); });
    auto result = future.get();
    REQUIRE(result.found);
    CheckConvertedRow(result.row);
}

TEST_CASE("Reader: the legacy GetEntity converts", "[reader][stub]") {
    StubHttpServer server({Reply(200, AnnotatedRow())});
    auto client = StubClient(server);
    CheckConvertedRow(client.GetEntity(TEST_TABLE, "p", "r"));
}

TEST_CASE("Reader: GetEntityAsync converts", "[reader][stub]") {
    StubHttpServer server({Reply(200, AnnotatedRow())});
    auto client = StubClient(server);
    std::promise<json> promise;
    auto future = promise.get_future();
    client.GetEntityAsync(TEST_TABLE, "p", "r", [&promise](json r) { promise.set_value(std::move(r)); });
    CheckConvertedRow(future.get());
}

TEST_CASE("Reader: QueryEntitiesChecked converts every row on every page", "[reader][stub]") {
    const std::string page = "{\"odata.metadata\":\"http://x/$metadata#t\",\"value\":[" + AnnotatedRow() + "]}";
    StubResponse first = Reply(200, page);
    first.headers["x-ms-continuation-NextPartitionKey"] = "np";
    first.headers["x-ms-continuation-NextRowKey"] = "nr";
    StubResponse second = Reply(200, page);
    StubHttpServer server({first, second});
    auto client = StubClient(server);
    auto result = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'p'");
    REQUIRE(result.ok);
    REQUIRE(result.rows.size() == 2);
    for (const auto &row : result.rows) CheckConvertedRow(row);
}

TEST_CASE("Reader: QueryEntitiesCheckedAsync, QueryEntities and QueryEntitiesAsync convert", "[reader][stub]") {
    const std::string page = "{\"value\":[" + AnnotatedRow() + "]}";
    {
        StubHttpServer server({Reply(200, page)});
        auto client = StubClient(server);
        std::promise<AzureTableClient::QueryResult> promise;
        auto future = promise.get_future();
        client.QueryEntitiesCheckedAsync(TEST_TABLE, "PartitionKey eq 'p'",
            [&promise](AzureTableClient::QueryResult r) { promise.set_value(std::move(r)); });
        auto result = future.get();
        REQUIRE(result.ok);
        REQUIRE(result.rows.size() == 1);
        CheckConvertedRow(result.rows[0]);
    }
    {
        StubHttpServer server({Reply(200, page)});
        auto client = StubClient(server);
        auto rows = client.QueryEntities(TEST_TABLE, "PartitionKey eq 'p'");
        REQUIRE(rows.size() == 1);
        CheckConvertedRow(rows[0]);
    }
    {
        StubHttpServer server({Reply(200, page)});
        auto client = StubClient(server);
        std::promise<std::vector<json>> promise;
        auto future = promise.get_future();
        client.QueryEntitiesAsync(TEST_TABLE, "PartitionKey eq 'p'",
            [&promise](std::vector<json> r) { promise.set_value(std::move(r)); });
        auto rows = future.get();
        REQUIRE(rows.size() == 1);
        CheckConvertedRow(rows[0]);
    }
}

TEST_CASE("Reader: a row without annotations keeps today's shape", "[reader][stub]") {
    auto result = StubGet(Reply(200, "{\"PartitionKey\":\"p\",\"RowKey\":\"r\",\"n\":3,\"s\":\"x\"}"));
    REQUIRE(result.found);
    REQUIRE(result.row == json({{"PartitionKey", "p"}, {"RowKey", "r"}, {"n", 3}, {"s", "x"}}));
}

TEST_CASE("Reader: failure reporting is unchanged and carries no body", "[reader][stub]") {
    const std::string secret = "SECRET-BODY-TEXT-4412";
    auto failed = StubGet(Reply(500, "{\"detail\":\"" + secret + "\"}"));
    REQUIRE_FALSE(failed.ok);
    REQUIRE(failed.failure == AzureTableClient::Failure::http_status);
    REQUIRE(failed.http_status == 500);
    REQUIRE(failed.row.is_null());
    auto malformed = StubGet(Reply(200, "nope"));
    REQUIRE(malformed.failure == AzureTableClient::Failure::malformed_response);
    auto absent = StubGet(Reply(404, RESOURCE_NOT_FOUND));
    REQUIRE(absent.ok);
    REQUIRE_FALSE(absent.found);
}

#endif // __linux__

// ---------------------------------------------------------------------------
// Large-integer round trip against Azurite [azurite-int64]
// ---------------------------------------------------------------------------

namespace {

std::string Base64Hmac(const std::string &keyB64, const std::string &text) {
    std::vector<unsigned char> key(keyB64.size());
    int n = EVP_DecodeBlock(key.data(), reinterpret_cast<const unsigned char *>(keyB64.data()),
                            static_cast<int>(keyB64.size()));
    size_t pad = 0;
    while (pad < keyB64.size() && keyB64[keyB64.size() - 1 - pad] == '=') ++pad;
    key.resize(static_cast<size_t>(n) - pad);
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char *>(text.data()), text.size(), digest, &len);
    std::vector<unsigned char> out(((len + 2) / 3) * 4 + 1);
    int m = EVP_EncodeBlock(out.data(), digest, static_cast<int>(len));
    return std::string(reinterpret_cast<char *>(out.data()), static_cast<size_t>(m));
}

size_t RawSink(char *p, size_t s, size_t n, void *u) {
    static_cast<std::string *>(u)->append(p, s * n);
    return s * n;
}

// A raw signed GET of one entity with minimalmetadata, independent of the client under test.
json RawGetMinimal(const std::string &endpoint, const std::string &account, const std::string &key,
                   const std::string &table, const std::string &pk, const std::string &rk) {
    const std::string path = "/" + account + "/" + table + "(PartitionKey='" + pk + "',RowKey='" + rk + "')";
    std::time_t now = std::time(nullptr);
    std::tm gmt{};
    gmtime_r(&now, &gmt);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
    const std::string date = buf;
    const std::string sts = "GET\n\n\n" + date + "\n/" + account + path;
    const std::string auth = "SharedKey " + account + ":" + Base64Hmac(key, sts);

    CURL *curl = curl_easy_init();
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, (endpoint + "/" + table + "(PartitionKey='" + pk + "',RowKey='" + rk + "')").c_str());
    struct curl_slist *h = nullptr;
    h = curl_slist_append(h, "Accept: application/json;odata=minimalmetadata");
    h = curl_slist_append(h, ("x-ms-date: " + date).c_str());
    h = curl_slist_append(h, "x-ms-version: 2021-12-02");
    h = curl_slist_append(h, ("Authorization: " + auth).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RawSink);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_perform(curl);
    curl_slist_free_all(h);
    curl_easy_cleanup(curl);
    return json::parse(body, nullptr, false);
}

} // namespace

TEST_CASE("Azurite: boundary integers are stored as Edm.Int64 and read back equal", "[azurite-int64]") {
    REQUIRE_AZURITE();
    AzureTableClient client(ACCOUNT_NAME, ACCOUNT_KEY, TABLE_ENDPOINT);
    const std::vector<int64_t> values = {0, 2147483647LL, 2147483648LL, -2147483648LL, -2147483649LL,
                                         kTwo53, kInt64Min, kInt64Max};
    json e = Row("int64_round", "boundary");
    for (size_t i = 0; i < values.size(); ++i) e[PropName(i)] = values[i];
    REQUIRE(client.UpsertEntity(TEST_TABLE, e));

    json raw = RawGetMinimal(TABLE_ENDPOINT, ACCOUNT_NAME, ACCOUNT_KEY, TEST_TABLE, "int64_round", "boundary");
    REQUIRE(raw.is_object());
    for (size_t i = 0; i < values.size(); ++i) {
        const bool large = values[i] > 2147483647LL || values[i] < -2147483648LL;
        INFO("value " << values[i]);
        if (large) {
            REQUIRE(raw.contains(PropName(i) + "@odata.type"));
            REQUIRE(raw[PropName(i) + "@odata.type"] == "Edm.Int64");
            REQUIRE(raw[PropName(i)] == std::to_string(values[i]));
        }
    }

    auto got = client.GetEntityChecked(TEST_TABLE, "int64_round", "boundary");
    REQUIRE(got.ok);
    REQUIRE(got.found);
    for (size_t i = 0; i < values.size(); ++i) {
        INFO("value " << values[i]);
        REQUIRE(got.row[PropName(i)].is_number_integer());
        REQUIRE(got.row[PropName(i)].get<int64_t>() == values[i]);
    }
    for (auto it = got.row.begin(); it != got.row.end(); ++it) {
        REQUIRE(it.key().find("@odata.type") == std::string::npos);
        REQUIRE(it.key().rfind("odata.", 0) != 0);
    }

    auto query = client.QueryEntitiesChecked(TEST_TABLE, "PartitionKey eq 'int64_round'");
    REQUIRE(query.ok);
    REQUIRE(query.rows.size() == 1);
    REQUIRE(query.rows[0][PropName(6)].get<int64_t>() == kInt64Min);
}

// ---------------------------------------------------------------------------
// Real-account check [real-account]
// ---------------------------------------------------------------------------
//
// Opt-in: this case talks to a real Azure storage account, so it cannot run in CI.
// It is skipped (reported as skipped, not failed) unless AZURE_TABLE_ENDPOINT,
// AZURE_TABLE_ACCOUNT and AZURE_TABLE_KEY are all set, and it is excluded from the
// default `make check` run by the "[.]" hidden tag. Run it by name:
//   AZURE_TABLE_ENDPOINT=... AZURE_TABLE_ACCOUNT=... AZURE_TABLE_KEY=... \
//     ./test_azure_table_client "[real-account]"
// It prints only the pass/fail line and the integers; never the key, the endpoint
// or any row body.

TEST_CASE("Real account: boundary integers round-trip through the storage service", "[.][real-account]") {
    const char *endpoint = std::getenv("AZURE_TABLE_ENDPOINT");
    const char *account = std::getenv("AZURE_TABLE_ACCOUNT");
    const char *key = std::getenv("AZURE_TABLE_KEY");
    if (!endpoint || !account || !key || !*endpoint || !*account || !*key) {
        SKIP("AZURE_TABLE_ENDPOINT, AZURE_TABLE_ACCOUNT and AZURE_TABLE_KEY are not all set");
    }

    const std::string table = "fidelitycheck";
    AzureTableClient client(account, key, endpoint);
    REQUIRE(client.CreateTableIfNotExists(table));

    const std::vector<int64_t> values = {2147483648LL, -2147483649LL, 1700000000000LL, kTwo53, -kTwo53,
                                         kInt64Min, kInt64Max};
    json e = Row("fidelity", "boundary");
    for (size_t i = 0; i < values.size(); ++i) e[PropName(i)] = values[i];
    e["plain"] = 5;
    e["text"] = "[5]";
    REQUIRE(client.UpsertEntity(table, e));

    auto got = client.GetEntityChecked(table, "fidelity", "boundary");
    bool pass = got.ok && got.found;
    size_t matched = 0;
    if (pass) {
        for (size_t i = 0; i < values.size(); ++i) {
            if (got.row[PropName(i)].is_number_integer() && got.row[PropName(i)].get<int64_t>() == values[i])
                ++matched;
        }
        pass = matched == values.size() && got.row["plain"] == 5 && got.row["text"] == "[5]";
    }
    client.DeleteEntity(table, "fidelity", "boundary");

    std::cout << "real-account: " << matched << " values round-tripped" << (pass ? " PASS" : " FAIL");
    for (int64_t v : values) std::cout << ' ' << v;
    std::cout << std::endl;
    REQUIRE(pass);
}
