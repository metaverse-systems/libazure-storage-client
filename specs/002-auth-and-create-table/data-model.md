# Data Model: Auth & CreateTableIfNotExists

**Feature**: 002-auth-and-create-table
**Date**: 2026-03-22

## Entities

### AzureTableClient

The single public type. Holds authentication state and issues authenticated HTTP requests.

| Field | Type | Description |
|-------|------|-------------|
| account_name | `std::string` | Azure Storage account name (e.g., `devstoreaccount1`). Set by SharedKey constructor; empty for bearer-only clients. |
| account_key | `std::string` | Base64-encoded account key. Set by SharedKey constructor; empty for bearer-only clients. |
| table_endpoint | `std::string` | Base URL for the Table Storage service (e.g., `http://127.0.0.1:10002/devstoreaccount1`). Trailing slash stripped on storage. |
| bearer_token | `std::string` | OAuth2/Entra ID bearer token. Set by bearer constructor or `SetBearerToken()`. When non-empty, takes priority over SharedKey auth. |

**Construction modes**:
- SharedKey: `AzureTableClient(account_name, account_key, table_endpoint)` — stores all three fields; bearer_token starts empty.
- Bearer: `AzureTableClient(table_endpoint, bearer_token)` — stores endpoint and token; account_name and account_key start empty.

**Auth dispatch rule**: If `bearer_token` is non-empty, use `Authorization: Bearer {bearer_token}`. Otherwise, if `account_name` and `account_key` are non-empty, compute and use `Authorization: SharedKey {account_name}:{signature}`.

**Validation rules**:
- No constructor-time validation. Invalid credentials produce service-side errors (403, etc.) which are returned as `false` from operations.
- `SetBearerToken()` unconditionally overwrites the stored bearer_token.

### Table (Azure-side entity)

A named container in Azure Table Storage. Not modeled as a C++ type — the library interacts with tables by name only.

| Field | Type | Description |
|-------|------|-------------|
| TableName | `std::string` | The name of the table (3–63 chars, alphanumeric, per Azure naming rules). Passed as parameter to `CreateTableIfNotExists`. |

**State transitions**:
- Non-existent → Created: `CreateTableIfNotExists` sends POST, receives 201.
- Created → Created (no-op): `CreateTableIfNotExists` sends POST, receives 409, returns `true`.

## Internal Helpers (not public API)

These are implementation details in the `.cpp` file. They do not appear in the public header.

| Helper | Purpose |
|--------|---------|
| `build_date_string()` | Generate current UTC time in RFC 1123 format for `x-ms-date`. |
| `hmac_sha256_sign(account_key_b64, string_to_sign)` | Base64-decode the account key, HMAC-SHA256 sign the StringToSign, and return base64-encoded signature. |
| `base64_decode(encoded)` | Decode a base64 string to raw bytes (for account key). |
| `base64_encode(raw_bytes, len)` | Encode raw bytes to base64 string (for signature output). |
| `build_shared_key_auth_header(...)` | Construct the full `Authorization` header value for SharedKey mode. |
| `normalize_endpoint(endpoint)` | Strip trailing slash from endpoint URL. |
| `perform_request(method, url, headers, body)` | Execute an HTTP request via libcurl and return the HTTP status code (or 0 on network error). |

## Relationships

```
AzureTableClient --[authenticates]--> Azure Table Storage Service
AzureTableClient --[creates]--> Table (via CreateTableIfNotExists)
AzureTableClient --[uses]--> libcurl (HTTP transport)
AzureTableClient --[uses]--> OpenSSL (HMAC-SHA256 signing, base64)
AzureTableClient --[uses]--> nlohmann-json (request body serialization)
```
