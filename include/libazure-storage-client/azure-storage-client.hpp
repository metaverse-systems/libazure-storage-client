#pragma once

#include <string>
#include <vector>
#include <functional>
#include <libazure-storage-client/json.hpp>

class AzureTableClient {
public:
    // Why a checked query failed.
    enum class Failure { none, invalid_request, transport, http_status, malformed_response, incomplete };

    // Outcome of a checked query. rows is empty whenever ok is false.
    // Never carries a URL, header, token or response body.
    struct QueryResult {
        bool ok = false;
        std::vector<nlohmann::json> rows;
        Failure failure = Failure::none;
        long http_status = 0;
    };

    // Outcome of a checked single-entity read. row is null unless found.
    // ok && !found is a definite absence (404 ResourceNotFound).
    // Never carries a URL, header, token or response body.
    struct EntityResult {
        bool ok = false;
        bool found = false;
        nlohmann::json row;
        Failure failure = Failure::none;
        long http_status = 0;
    };

    AzureTableClient(const std::string &account_name,
                     const std::string &account_key,
                     const std::string &table_endpoint);

    AzureTableClient(const std::string &table_endpoint,
                     const std::string &bearer_token);

    void SetBearerToken(const std::string &bearer_token);

    bool CreateTableIfNotExists(const std::string &table_name);

    // Cannot tell failure from empty; use the Checked form.
    nlohmann::json GetEntity(const std::string &table_name,
                             const std::string &partition_key,
                             const std::string &row_key);

    // Reads one entity and reports failure explicitly. Never throws.
    EntityResult GetEntityChecked(const std::string &table_name,
                                  const std::string &partition_key,
                                  const std::string &row_key);

    void GetEntityCheckedAsync(const std::string &table_name,
                               const std::string &partition_key,
                               const std::string &row_key,
                               std::function<void(EntityResult)> callback);

    bool UpsertEntity(const std::string &table_name,
                      const nlohmann::json &entity);

    bool BatchUpsertEntities(const std::string &table_name,
                             const std::vector<nlohmann::json> &entities);

    // Cannot tell failure from empty; use the Checked form.
    std::vector<nlohmann::json> QueryEntities(const std::string &table_name,
                                              const std::string &filter);

    // Queries a table and reports failure explicitly. An empty filter queries the whole table.
    QueryResult QueryEntitiesChecked(const std::string &table_name,
                                     const std::string &filter);

    void QueryEntitiesCheckedAsync(const std::string &table_name,
                                   const std::string &filter,
                                   std::function<void(QueryResult)> callback);

    // Doubles single quotes so a value can be placed inside an OData string literal.
    static std::string EscapeFilterString(const std::string &value);

    bool DeleteEntity(const std::string &table_name,
                      const std::string &partition_key,
                      const std::string &row_key);

    // Cannot tell failure from empty; use the Checked form.
    void GetEntityAsync(const std::string &table_name,
                        const std::string &partition_key,
                        const std::string &row_key,
                        std::function<void(nlohmann::json)> callback);

    void UpsertEntityAsync(const std::string &table_name,
                           const nlohmann::json &entity,
                           std::function<void(bool)> callback);

    void BatchUpsertEntitiesAsync(const std::string &table_name,
                                  const std::vector<nlohmann::json> &entities,
                                  std::function<void(bool)> callback);

    // Cannot tell failure from empty; use the Checked form.
    void QueryEntitiesAsync(const std::string &table_name,
                            const std::string &filter,
                            std::function<void(std::vector<nlohmann::json>)> callback);

    void DeleteEntityAsync(const std::string &table_name,
                           const std::string &partition_key,
                           const std::string &row_key,
                           std::function<void(bool)> callback);

protected:
    // Test seam for the URL-escape step. A hook returning false makes a query fail
    // closed (invalid_request, no request sent). nullptr restores the real escape.
    using UrlEscapeFn = bool (*)(const std::string &in, std::string &out);
    static void SetUrlEscapeHook(UrlEscapeFn hook);

private:
    std::string account_name;
    std::string account_key;
    std::string table_endpoint;
    std::string bearer_token;
};
