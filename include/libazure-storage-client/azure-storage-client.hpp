#pragma once

#include <string>
#include <vector>
#include <functional>
#include <libazure-storage-client/json.hpp>

class AzureTableClient {
public:
    AzureTableClient(const std::string &account_name,
                     const std::string &account_key,
                     const std::string &table_endpoint);

    AzureTableClient(const std::string &table_endpoint,
                     const std::string &bearer_token);

    void SetBearerToken(const std::string &bearer_token);

    bool CreateTableIfNotExists(const std::string &table_name);

    nlohmann::json GetEntity(const std::string &table_name,
                             const std::string &partition_key,
                             const std::string &row_key);

    bool UpsertEntity(const std::string &table_name,
                      const nlohmann::json &entity);

    bool BatchUpsertEntities(const std::string &table_name,
                             const std::vector<nlohmann::json> &entities);

    std::vector<nlohmann::json> QueryEntities(const std::string &table_name,
                                              const std::string &filter);

    bool DeleteEntity(const std::string &table_name,
                      const std::string &partition_key,
                      const std::string &row_key);

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

    void QueryEntitiesAsync(const std::string &table_name,
                            const std::string &filter,
                            std::function<void(std::vector<nlohmann::json>)> callback);

    void DeleteEntityAsync(const std::string &table_name,
                           const std::string &partition_key,
                           const std::string &row_key,
                           std::function<void(bool)> callback);

private:
    std::string account_name;
    std::string account_key;
    std::string table_endpoint;
    std::string bearer_token;
};
