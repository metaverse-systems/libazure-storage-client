# Public API Contract: AzureTableClient

**Feature**: 002-auth-and-create-table
**Date**: 2026-03-22
**Header**: `include/libazure-storage-client/azure-storage-client.hpp`

## Scope

This contract covers only the methods implemented in this feature. All other
declared methods remain as stubs (return `false` / empty) until future features.

## Constructors

### SharedKey Constructor

```
AzureTableClient(account_name, account_key, table_endpoint)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| account_name | `const std::string&` | Azure Storage account name |
| account_key | `const std::string&` | Base64-encoded account key |
| table_endpoint | `const std::string&` | Full endpoint URL (e.g., `http://127.0.0.1:10002/devstoreaccount1`) |

**Postconditions**: Credentials stored. Bearer token is empty. Trailing slash on endpoint stripped.

### Bearer Token Constructor

```
AzureTableClient(table_endpoint, bearer_token)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_endpoint | `const std::string&` | Full endpoint URL |
| bearer_token | `const std::string&` | OAuth2/Entra ID bearer token |

**Postconditions**: Token and endpoint stored. Account name and key are empty. Trailing slash on endpoint stripped.

## Methods (implemented in this feature)

### SetBearerToken

```
void SetBearerToken(bearer_token)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| bearer_token | `const std::string&` | New bearer token value |

**Behavior**: Unconditionally replaces the stored bearer token. When non-empty, subsequent requests use Bearer auth regardless of construction mode.

### CreateTableIfNotExists

```
bool CreateTableIfNotExists(table_name)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| table_name | `const std::string&` | Name of the table to create |

**Returns**: `true` if table was created (HTTP 201) or already exists (HTTP 409). `false` for any other outcome.

**HTTP Request**:
- Method: `POST`
- URL: `{endpoint}/Tables`
- Headers:
  - `Content-Type: application/json`
  - `Accept: application/json;odata=nometadata`
  - `x-ms-date: {RFC 1123 UTC timestamp}`
  - `x-ms-version: 2021-12-02`
  - `Authorization: SharedKey {account}:{signature}` or `Authorization: Bearer {token}`
- Body: `{"TableName":"{table_name}"}`

## Common Request Headers (all operations)

Every HTTP request emitted by the library includes:

| Header | Value | Notes |
|--------|-------|-------|
| `x-ms-date` | RFC 1123 UTC (e.g., `Sun, 22 Mar 2026 14:30:00 GMT`) | Freshly generated per request |
| `x-ms-version` | `2021-12-02` | Fixed |
| `Authorization` | `SharedKey {account}:{sig}` or `Bearer {token}` | Auth-mode dependent |
| `Accept` | `application/json;odata=nometadata` | JSON response format |

## Auth Dispatch Logic

```
if bearer_token is non-empty:
    Authorization = "Bearer " + bearer_token
else if account_name and account_key are non-empty:
    compute StringToSign for Table Service SharedKey
    sign with HMAC-SHA256
    Authorization = "SharedKey " + account_name + ":" + base64(signature)
else:
    request sent without Authorization header (will fail server-side)
    # ^ Intentional: this is a caller error. The library does not validate
    #   auth configuration at construction time (per Principle IV / error model).
```

## Error Model

- No exceptions thrown.
- No retry logic.
- Network errors → `false`.
- Unexpected HTTP status → `false`.
- Caller is responsible for all error handling beyond the `bool` return.
