#include <libazure-storage-client/azure-storage-client.hpp>

#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <ctime>
#include <cstring>
#include <sstream>
#include <vector>

namespace {

std::vector<unsigned char> base64_decode(const std::string &encoded)
{
    int encoded_len = static_cast<int>(encoded.size());
    int max_decoded_len = 3 * encoded_len / 4 + 1;
    std::vector<unsigned char> decoded(max_decoded_len);

    int decoded_len = EVP_DecodeBlock(decoded.data(),
                                      reinterpret_cast<const unsigned char *>(encoded.c_str()),
                                      encoded_len);
    if (decoded_len < 0)
        return {};

    // EVP_DecodeBlock does not account for padding — trim trailing zero bytes
    // caused by '=' padding characters
    int padding = 0;
    if (encoded_len >= 1 && encoded[encoded_len - 1] == '=') padding++;
    if (encoded_len >= 2 && encoded[encoded_len - 2] == '=') padding++;
    decoded_len -= padding;

    decoded.resize(decoded_len);
    return decoded;
}

std::string base64_encode(const unsigned char *data, int len)
{
    int encoded_len = 4 * ((len + 2) / 3) + 1;
    std::vector<unsigned char> encoded(encoded_len);

    EVP_EncodeBlock(encoded.data(), data, len);

    return std::string(reinterpret_cast<char *>(encoded.data()));
}

// build_date_string — current UTC time in RFC 1123 format
std::string build_date_string()
{
    std::time_t now = std::time(nullptr);
    std::tm gmt{};
    gmtime_r(&now, &gmt);

    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
    return std::string(buf);
}

// normalize_endpoint — strip single trailing slash
std::string normalize_endpoint(const std::string &endpoint)
{
    if (!endpoint.empty() && endpoint.back() == '/')
        return endpoint.substr(0, endpoint.size() - 1);
    return endpoint;
}

// url_path — extract path component from a URL
std::string url_path(const std::string &url)
{
    auto pos = url.find("://");
    if (pos == std::string::npos)
        return "/";
    pos = url.find('/', pos + 3);
    if (pos == std::string::npos)
        return "/";
    return url.substr(pos);
}

// perform_request — execute HTTP request via libcurl, return status code
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string *response = static_cast<std::string *>(userdata);
    size_t total = size * nmemb;
    response->append(ptr, total);
    return total;
}

long perform_request(const std::string &method,
                     const std::string &url,
                     const std::vector<std::string> &headers,
                     const std::string &body)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

    struct curl_slist *header_list = nullptr;
    for (const auto &h : headers)
        header_list = curl_slist_append(header_list, h.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    if (!body.empty())
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    return http_code;
}

// hmac_sha256_sign — sign StringToSign with base64-encoded account key
std::string hmac_sha256_sign(const std::string &account_key_b64,
                             const std::string &string_to_sign)
{
    std::vector<unsigned char> key_bytes = base64_decode(account_key_b64);
    if (key_bytes.empty())
        return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    HMAC(EVP_sha256(),
         key_bytes.data(), static_cast<int>(key_bytes.size()),
         reinterpret_cast<const unsigned char *>(string_to_sign.c_str()),
         string_to_sign.size(),
         digest, &digest_len);

    return base64_encode(digest, static_cast<int>(digest_len));
}

// build_string_to_sign — Table Service Shared Key StringToSign
// canonicalized_url_path is the path component of the request URL (e.g. "/devstoreaccount1/Tables")
std::string build_string_to_sign(const std::string &verb,
                                 const std::string &content_md5,
                                 const std::string &content_type,
                                 const std::string &date,
                                 const std::string &account_name,
                                 const std::string &canonicalized_url_path)
{
    return verb + "\n" +
           content_md5 + "\n" +
           content_type + "\n" +
           date + "\n" +
           "/" + account_name + canonicalized_url_path;
}

// build_shared_key_auth_header — full Authorization header value
// canonicalized_url_path is the path component of the request URL
std::string build_shared_key_auth_header(const std::string &verb,
                                         const std::string &content_md5,
                                         const std::string &content_type,
                                         const std::string &date,
                                         const std::string &account_name,
                                         const std::string &account_key,
                                         const std::string &canonicalized_url_path)
{
    std::string sts = build_string_to_sign(verb, content_md5, content_type,
                                           date, account_name, canonicalized_url_path);
    std::string signature = hmac_sha256_sign(account_key, sts);
    return "SharedKey " + account_name + ":" + signature;
}

} // anonymous namespace

AzureTableClient::AzureTableClient(const std::string &account_name,
                                   const std::string &account_key,
                                   const std::string &table_endpoint)
{
    this->account_name = account_name;
    this->account_key = account_key;
    this->table_endpoint = normalize_endpoint(table_endpoint);
}

AzureTableClient::AzureTableClient(const std::string &table_endpoint,
                                   const std::string &bearer_token)
{
    this->table_endpoint = normalize_endpoint(table_endpoint);
    this->bearer_token = bearer_token;
}

void AzureTableClient::SetBearerToken(const std::string &bearer_token)
{
    this->bearer_token = bearer_token;
}

bool AzureTableClient::CreateTableIfNotExists(const std::string &table_name)
{
    std::string url = this->table_endpoint + "/Tables";

    nlohmann::json body_json;
    body_json["TableName"] = table_name;
    std::string body = body_json.dump();

    std::string date = build_date_string();

    std::string content_type = "application/json;odata=nometadata";

    std::vector<std::string> headers;
    headers.push_back("Content-Type: " + content_type);
    headers.push_back("Accept: application/json;odata=nometadata");
    headers.push_back("x-ms-date: " + date);
    headers.push_back("x-ms-version: 2021-12-02");

    // Auth dispatch — bearer takes priority when non-empty
    if (!this->bearer_token.empty())
    {
        headers.push_back("Authorization: Bearer " + this->bearer_token);
    }
    else if (!this->account_name.empty() && !this->account_key.empty())
    {
        std::string auth = build_shared_key_auth_header(
            "POST", "", content_type, date,
            this->account_name, this->account_key, url_path(url));
        headers.push_back("Authorization: " + auth);
    }

    long status = perform_request("POST", url, headers, body);

    return (status == 201 || status == 409);
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
