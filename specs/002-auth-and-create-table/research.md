# Research: Auth & CreateTableIfNotExists

**Feature**: 002-auth-and-create-table
**Date**: 2026-03-22

## R-001: Azure Table Storage Authentication Scheme

### Decision
Use **Table Service Shared Key** (not Shared Key Lite) with `Authorization: SharedKey {account}:{signature}`.

### Rationale
- The constitution explicitly mandates `Authorization: SharedKey {account}:{base64-signature}`.
- Azure Table Storage supports two auth schemes: `SharedKey` (full) and `SharedKeyLite` (simplified). Both are valid; both are supported by Azurite.
- The `SharedKey` scheme for Table Service uses StringToSign = `VERB\nContent-MD5\nContent-Type\nDate\nCanonicalizedResource`, which matches the format the user selected in clarification Q2.
- The `SharedKeyLite` scheme uses a simpler StringToSign (`Date\nCanonicalizedResource`) but requires `Authorization: SharedKeyLite`, which conflicts with the constitution's explicit wording.

### Alternatives Considered
- **SharedKeyLite** (`Date\nCanonicalizedResource` only): Simpler to implement but constitutionally non-compliant. Would require a constitution amendment to change the Authorization header scheme name.

## R-002: StringToSign Format for Table Service Shared Key

### Decision
```
StringToSign = VERB + "\n" +
               Content-MD5 + "\n" +
               Content-Type + "\n" +
               Date + "\n" +
               CanonicalizedResource
```

### Rationale
- This is the documented Table Service Shared Key format (not Blob/Queue, which also includes CanonicalizedHeaders).
- `Content-MD5`: empty string for all Table operations (Table Storage does not use it).
- `Content-Type`: the actual Content-Type header value (e.g., `application/json` for CreateTable POST) or empty string if no body.
- `Date`: the value of the `x-ms-date` header (RFC 1123 format). For Table Service, this field is **never empty** even when `x-ms-date` is set.
- `CanonicalizedResource`: `/{account}/{uri-path}` — no query parameters except `?comp=` if present.

### Alternatives Considered
- **Shared Key Lite StringToSign** (`Date\nCanonicalizedResource`): Simpler but uses `SharedKeyLite` auth scheme which conflicts with constitution.

## R-003: Canonicalized Resource Construction

### Decision
Format: `/{accountName}/{resource-path}`

### Rationale
- For `CreateTableIfNotExists`: the resource path is `Tables`, so canonicalized resource = `/{account}/Tables`.
- For entity operations on a specific table: `/{account}/{tableName}(PartitionKey='pk',RowKey='rk')`.
- Query parameters are NOT included in the canonicalized resource for Table Service, except `?comp=` if present.
- For Azurite path-based URLs (`http://127.0.0.1:10002/devstoreaccount1/Tables`), the canonicalized resource is still `/{account}/Tables` — derived from the account name stored in the client, not parsed from the URL. 

## R-004: CreateTableIfNotExists HTTP Details

### Decision

| Field | Value |
|-------|-------|
| HTTP Method | `POST` |
| URL | `{endpoint}/Tables` |
| Content-Type | `application/json` |
| Accept | `application/json;odata=nometadata` |
| x-ms-version | `2021-12-02` |
| x-ms-date | Current UTC in RFC 1123 format |
| Request Body | `{"TableName":"MyTable"}` |
| Success codes | 201 (Created), 409 (Conflict = already exists) |
| Failure | Any other HTTP status or network error → return `false` |

### Rationale
- JSON content type (`application/json`) is preferred over Atom/XML per modern Azure SDK conventions and is simpler to construct.
- `odata=nometadata` in Accept minimizes response payload (no OData type annotations needed).
- The request body is a simple JSON object with a single `TableName` property.

### Alternatives Considered
- **Atom/XML format**: Legacy; more complex to construct; no benefit for this use case.
- **`odata=fullmetadata`**: Returns type annotations we don't use; wastes bandwidth.

## R-005: OpenSSL HMAC-SHA256 and Base64

### Decision
Use OpenSSL's `HMAC()` function (from `<openssl/hmac.h>`) for HMAC-SHA256, and OpenSSL's `EVP_EncodeBlock()` / `EVP_DecodeBlock()` for base64 encoding/decoding.

### Rationale
- OpenSSL is already a project dependency (per constitution).
- `HMAC()` is a single-call convenience function: `HMAC(EVP_sha256(), key, keyLen, data, dataLen, result, &resultLen)`.
- `EVP_EncodeBlock()` / `EVP_DecodeBlock()` provide base64 encode/decode without pulling in additional dependencies.
- The account key must be base64-decoded first to get the raw key bytes, then used as the HMAC key, then the HMAC output must be base64-encoded for the Authorization header.

### Alternatives Considered
- **EVP_MAC API** (newer OpenSSL 3.x): More flexible but more verbose; `HMAC()` is simpler and still supported.
- **BIO-based base64**: More complex for simple encode/decode operations; EVP functions are sufficient.

## R-006: libcurl HTTP Request Pattern

### Decision
Use `curl_easy_init()` per synchronous call. Set URL, method, headers, and body via `curl_easy_setopt()`. Read response status via `curl_easy_getinfo(CURLINFO_RESPONSE_CODE)`.

### Rationale
- Constitution mandates libcurl as the HTTP transport.
- Each sync method creates a fresh `CURL*` handle, performs the request, reads the response, and cleans up. No handle reuse or connection pooling (constitution Principle IV).
- Headers set via `curl_slist_append()` → `CURLOPT_HTTPHEADER`.
- POST body set via `CURLOPT_POSTFIELDS`.
- Response body captured via `CURLOPT_WRITEFUNCTION` callback writing to a `std::string`.

### Alternatives Considered
- **Handle reuse / connection pooling**: Would improve performance but violates Minimal Surface Area principle.
- **libcurl multi interface**: For async; not needed for sync operations in this feature.

## R-007: RFC 1123 Date Format

### Decision
Generate the `x-ms-date` value using `std::strftime` with format `"%a, %d %b %Y %H:%M:%S GMT"` from a `std::gmtime()` result.

### Rationale
- RFC 1123 example: `Sun, 11 Oct 2009 19:52:39 GMT`.
- C++20 `<chrono>` could be used but `strftime` is simpler and universally available.
- Must use UTC (`gmtime`), not local time.

### Alternatives Considered
- **`std::format` with `<chrono>`**: C++20 but not all compilers fully support `std::format` for chrono yet. `strftime` is safer for cross-platform.

## R-008: Trailing Slash Normalization

### Decision
Before appending resource paths, strip any trailing `/` from the stored endpoint. This is done once during construction or at request time.

### Rationale
- FR-009 requires no double slashes when endpoint ends with `/`.
- Simple string operation: if `endpoint.back() == '/'`, remove it.
- Consistent behavior regardless of how the caller provides the endpoint.

### Alternatives Considered
- **Check at each request**: More defensive but duplicates logic. Strip once is cleaner.
- **Don't strip; require caller to omit trailing slash**: Places burden on caller; fragile.

## R-009: Azurite API Version Compatibility

### Decision
Target `x-ms-version: 2021-12-02`.

### Rationale
- Azurite V3 latest (3.35.0) supports versions up to 2025-11-05 (preview).
- `2021-12-02` is a stable, well-tested version supported by both Azurite and production Azure.
- Table Storage features needed for this feature (Create Table, entity CRUD) have not changed between versions.
- Using a stable non-preview version avoids compatibility surprises.

### Alternatives Considered
- **`2025-01-05`**: May not be supported by all Azurite installations.
- **`2020-12-06`**: Overly conservative; no benefit over `2021-12-02`.
