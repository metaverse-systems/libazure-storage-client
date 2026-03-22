#include <libazure-storage-client/azure-storage-client.hpp>

AzureTableClient::AzureTableClient(const std::string &account_name,
                                   const std::string &account_key,
                                   const std::string &table_endpoint)
{
    this->account_name = account_name;
    this->account_key = account_key;
    this->table_endpoint = table_endpoint;
}

AzureTableClient::AzureTableClient(const std::string &table_endpoint,
                                   const std::string &bearer_token)
{
    this->table_endpoint = table_endpoint;
    this->bearer_token = bearer_token;
}

void AzureTableClient::SetBearerToken(const std::string &bearer_token)
{
    this->bearer_token = bearer_token;
}

bool AzureTableClient::CreateTableIfNotExists(const std::string &table_name)
{
    return false;
}

nlohmann::json AzureTableClient::GetEntity(const std::string &table_name,
                                           const std::string &partition_key,
                                           const std::string &row_key)
{
    return {};
}

bool AzureTableClient::UpsertEntity(const std::string &table_name,
                                    const nlohmann::json &entity)
{
    return false;
}

bool AzureTableClient::BatchUpsertEntities(const std::string &table_name,
                                           const std::vector<nlohmann::json> &entities)
{
    return false;
}

std::vector<nlohmann::json> AzureTableClient::QueryEntities(const std::string &table_name,
                                                            const std::string &filter)
{
    return {};
}

bool AzureTableClient::DeleteEntity(const std::string &table_name,
                                    const std::string &partition_key,
                                    const std::string &row_key)
{
    return false;
}

void AzureTableClient::GetEntityAsync(const std::string &table_name,
                                      const std::string &partition_key,
                                      const std::string &row_key,
                                      std::function<void(nlohmann::json)> callback)
{
    callback({});
}

void AzureTableClient::UpsertEntityAsync(const std::string &table_name,
                                         const nlohmann::json &entity,
                                         std::function<void(bool)> callback)
{
    callback(false);
}

void AzureTableClient::BatchUpsertEntitiesAsync(const std::string &table_name,
                                                const std::vector<nlohmann::json> &entities,
                                                std::function<void(bool)> callback)
{
    callback(false);
}

void AzureTableClient::QueryEntitiesAsync(const std::string &table_name,
                                          const std::string &filter,
                                          std::function<void(std::vector<nlohmann::json>)> callback)
{
    callback({});
}

void AzureTableClient::DeleteEntityAsync(const std::string &table_name,
                                         const std::string &partition_key,
                                         const std::string &row_key,
                                         std::function<void(bool)> callback)
{
    callback(false);
}
