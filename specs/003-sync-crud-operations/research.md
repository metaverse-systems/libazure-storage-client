# Research: Synchronous CRUD Operations

**Feature**: 003-sync-crud-operations
**Date**: 2026-03-22

## 1. HTTP Response Struct Design

**Decision**: Extend `perform_request` to return an `HttpResponse` struct containing status code, response body, and response headers map.

**Rationale**: `GetEntity` and `QueryEntities` need the response body for JSON parsing. `QueryEntities` pagination requires reading `x-ms-continuation-NextPartitionKey` and `x-ms-continuation-NextRowKey` response headers. A single struct avoids multiple helper functions and keeps the call site clean.

**Alternatives considered**:
- Separate `perform_request_with_body` function: rejected because it duplicates most of the existing code.
- Output parameters (pass pointers): rejected as less idiomatic C++ than returning a struct.

**Implementation**: Define `struct HttpResponse { long status; std::string body; std::map<std::string, std::string> headers; }` in the anonymous namespace. Add a `header_callback` function for libcurl (`CURLOPT_HEADERFUNCTION`) that parses `Name: value\r\n` lines into the map. Refactor the existing `perform_request` to return `HttpResponse`. Update `CreateTableIfNotExists` to use `.status` from the returned struct.

## 2. Entity GET — Point Read

**Decision**: GET to `{endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}')` with OData single-quote escaping.

**Rationale**: Azure Table Storage uses OData key-predicate syntax for single-entity access.

**URL pattern**: `{endpoint}/{tableName}(PartitionKey='{escapedPK}',RowKey='{escapedRK}')`
- Single quotes in key values are doubled: `O'Brien` → `O''Brien`
- Forward slashes URL-encoded as `%2F`, percent signs as `%25`
- HTTP method: GET
- Success status: 200 OK with JSON body
- Error status: 404 Not Found (entity doesn't exist) → return empty JSON

**Headers**: `Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`, `Authorization`

## 3. Entity PUT — Upsert (InsertOrReplace)

**Decision**: PUT to entity URL with full JSON body; expect 204 No Content on success.

**Rationale**: `InsertOrReplace` is the Azure Table Storage upsert operation. It creates the entity if it doesn't exist, or fully replaces it if it does. This matches the spec's `UpsertEntity` semantics.

**URL pattern**: Same as GET — `{endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}')`
- PartitionKey and RowKey extracted from the JSON entity object
- HTTP method: PUT
- Request body: the entity JSON (includes PartitionKey/RowKey and all properties)
- Content-Type: `application/json;odata=nometadata`
- Success status: 204 No Content
- No `If-Match` header (unconditional upsert)

## 4. Entity DELETE

**Decision**: DELETE to entity URL with `If-Match: *`; expect 204 No Content on success.

**Rationale**: Azure Table Storage requires `If-Match` for delete. Wildcard `*` performs unconditional delete regardless of ETag, as specified in the constitution.

**URL pattern**: Same as GET/PUT
- HTTP method: DELETE
- Required header: `If-Match: *`
- No request body
- Success status: 204 No Content
- Error: 404 if entity doesn't exist → return `false`

## 5. Query Entities with OData Filter

**Decision**: GET to `{endpoint}/{tableName}()` with `$filter` query parameter; follow continuation tokens.

**Rationale**: Azure Table Storage returns up to 1000 entities per page. Pagination uses custom response headers as continuation tokens, passed as query parameters on subsequent requests.

**URL pattern**: `{endpoint}/{tableName}()?$filter={urlEncodedFilter}`
- If filter is empty, just `{endpoint}/{tableName}()`
- HTTP method: GET
- Success status: 200 OK with JSON body containing `"value": [...]` array
- Response body: `{ "value": [ {entity1}, {entity2}, ... ] }`

**Pagination**:
- Response headers: `x-ms-continuation-NextPartitionKey` and `x-ms-continuation-NextRowKey`
- If present, append `&NextPartitionKey={value}&NextRowKey={value}` to next request URL
- Continue until no continuation headers are returned
- No library-level cap; caller controls via `$top` parameter

**URL encoding**: The `$filter` value must be URL-encoded (spaces → `%20`, quotes → `%27`, etc.). Use `curl_easy_escape()` for encoding.

## 6. Batch / Entity Group Transaction

**Decision**: POST to `{endpoint}/$batch` with multipart/mixed body containing a changeset with individual PUT operations.

**Rationale**: Azure Table Storage batch transactions use the OData Entity Group Transaction format — multipart/mixed MIME with outer batch boundary and inner changeset boundary.

**URL**: `{endpoint}/$batch`
- HTTP method: POST
- Content-Type: `multipart/mixed; boundary=batch_{uuid}`
- Success status: 202 Accepted

**Body structure**:
```
--batch_{batchId}
Content-Type: multipart/mixed; boundary=changeset_{changesetId}

--changeset_{changesetId}
Content-Type: application/http
Content-Transfer-Encoding: binary

PUT {endpoint}/{tableName}(PartitionKey='{pk}',RowKey='{rk}') HTTP/1.1
Content-Type: application/json
Accept: application/json;odata=nometadata

{entityJson}

--changeset_{changesetId}
Content-Type: application/http
Content-Transfer-Encoding: binary

PUT {endpoint}/{tableName}(PartitionKey='{pk2}',RowKey='{rk2}') HTTP/1.1
Content-Type: application/json
Accept: application/json;odata=nometadata

{entity2Json}

--changeset_{changesetId}--
--batch_{batchId}--
```

**Boundary IDs**: Use simple UUIDs or monotonic IDs (e.g., `batch_001`, `changeset_001`). Azure is not strict about format.

**Constraints**: Max 100 operations per batch. All entities must share same PartitionKey. Max 4 MiB payload.

**Failure detection**: If the outer response is 202 but the inner changeset contains error responses (non-2xx status lines), the batch failed atomically. Parse the multipart response to check for errors, or simply check if the outer HTTP status is 202 and trust the atomic guarantee — if any operation fails, the entire changeset rolls back and the 202 response body will contain error details.

**Simplified approach**: Check that the 202 response body does not contain error status lines (e.g., `HTTP/1.1 4xx` or `HTTP/1.1 5xx`). If only `HTTP/1.1 204` lines appear in the response body, the batch succeeded.

## 7. OData Key Encoding Helper

**Decision**: Implement an `escape_odata_key` helper that doubles single quotes and URL-encodes special characters.

**Rationale**: PartitionKey and RowKey values can contain single quotes, forward slashes, and other special characters that must be properly escaped in the OData URL syntax.

**Rules**:
1. Double single quotes: `'` → `''` (OData escaping within the key predicate)
2. URL-encode the result for use in the URL path: `/` → `%2F`, `%` → `%25`, etc.
3. Use `curl_easy_escape()` for URL encoding, but apply OData quote-doubling first

## 8. libcurl Header Capture

**Decision**: Use `CURLOPT_HEADERFUNCTION` with a static callback that parses response headers into a `std::map`.

**Rationale**: libcurl provides a per-header callback that fires for each response header line. This is the standard approach for capturing headers.

**Callback pattern**:
```cpp
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);
    std::string line(buffer, size * nitems);
    // Trim trailing \r\n
    // Find ':' separator
    // Store lowercase(name) → trimmed(value) in map
    return size * nitems;
}
```

Set via: `curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback)` and `curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers)`.
