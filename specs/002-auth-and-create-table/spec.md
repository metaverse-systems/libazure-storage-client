# Feature Specification: Auth & CreateTableIfNotExists

**Feature Branch**: `002-auth-and-create-table`
**Created**: 2026-03-22
**Status**: Draft
**Input**: User description: "Implement Shared Key authentication, bearer token authentication, and the CreateTableIfNotExists operation. The Shared Key constructor must store credentials, build the StringToSign per the Azure Table Storage spec, HMAC-SHA256 sign it with OpenSSL, and emit the Authorization header on every request. CreateTableIfNotExists must send a POST to create the table and treat HTTP 409 as success. Verify against Azurite."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Shared Key Authentication (Priority: P1)

As a library consumer, I construct an `AzureTableClient` with an account name, account key, and table endpoint. From that point on, every HTTP request the library sends to Azure Table Storage includes a correctly signed `Authorization: SharedKey {account}:{signature}` header, an `x-ms-date` header, and an `x-ms-version` header, so that the storage service accepts the request without a 403.

**Why this priority**: Shared Key authentication is the prerequisite for every other operation. Without a valid signature no request will succeed. Azurite (the local development emulator) only supports Shared Key auth, making this the gating capability for all local testing.

**Independent Test**: Construct a client with the well-known Azurite dev-storage credentials, call any authenticated endpoint (e.g., list tables), and confirm the service returns HTTP 200 rather than 403.

**Acceptance Scenarios**:

1. **Given** valid Azurite dev-storage credentials (account name `devstoreaccount1` and the well-known base64 key), **When** the client sends any request to Azurite, **Then** the request includes `Authorization`, `x-ms-date`, and `x-ms-version` headers and the service responds with a 2xx or 409 (not 403).
2. **Given** an invalid account key, **When** the client sends a request, **Then** the service responds with HTTP 403 (proving the library actually sends the signature rather than omitting it).
3. **Given** a Shared Key client, **When** two requests are sent seconds apart, **Then** each request carries its own freshly generated `x-ms-date` and a matching signature (signatures differ because the date differs).

---

### User Story 2 - CreateTableIfNotExists (Priority: P1)

As a library consumer, I call `CreateTableIfNotExists("MyTable")` and the library creates the table in Azure Table Storage if it does not already exist. If the table already exists the call still returns success, so I can safely call it at application startup without pre-checking.

**Why this priority**: Table creation is the first real data-plane operation consumers need. It also serves as the simplest end-to-end proof that authentication works, making it the natural smoke test for Story 1.

**Independent Test**: Against a running Azurite instance, call `CreateTableIfNotExists` twice for the same table name. Both calls must return `true`.

**Acceptance Scenarios**:

1. **Given** an authenticated client and a table name that does not yet exist, **When** `CreateTableIfNotExists` is called, **Then** the library sends an HTTP POST to the Tables resource, receives HTTP 201, and returns `true`.
2. **Given** an authenticated client and a table that already exists, **When** `CreateTableIfNotExists` is called, **Then** the library receives HTTP 409 (Conflict), treats it as success, and returns `true`.
3. **Given** an authenticated client and a network error or unexpected HTTP status (e.g., 500), **When** `CreateTableIfNotExists` is called, **Then** the library returns `false`.

---

### User Story 3 - Bearer Token Authentication (Priority: P2)

As a library consumer, I construct an `AzureTableClient` with a table endpoint and a bearer token (obtained from Entra ID / OAuth2 outside the library). Every HTTP request the library sends includes `Authorization: Bearer {token}`. I can also update the token on a live client instance via `SetBearerToken()` without recreating the client.

**Why this priority**: Bearer token auth is required for production Entra ID workflows but cannot be verified against Azurite (which only supports Shared Key). It is still essential for production use and must be implemented, but ranks below Shared Key because Shared Key is the only auth mode that is locally testable and gates all other Stories.

**Independent Test**: Construct a bearer-token client, invoke an operation against a real Azure Table Storage endpoint (or a mock HTTP server), and inspect the outgoing request to confirm it carries `Authorization: Bearer {token}`. Call `SetBearerToken()` with a new token and confirm subsequent requests use the updated value.

**Acceptance Scenarios**:

1. **Given** a bearer token client, **When** a request is sent, **Then** the outgoing request includes `Authorization: Bearer {token}`, `x-ms-date`, and `x-ms-version` headers.
2. **Given** a live bearer token client, **When** `SetBearerToken("new-token")` is called and then a request is sent, **Then** the request carries `Authorization: Bearer new-token`.
3. **Given** a bearer token client, **When** a request is sent, **Then** no Shared Key signature computation occurs (the library uses the token path, not the signing path).

---

### Edge Cases

- What happens when the account key is not valid base64? The library should fail gracefully (return `false` on the first operation) rather than crash.
- What happens when the table endpoint URL has a trailing slash vs. no trailing slash? The library must produce correct request URLs in both cases.
- What happens when `CreateTableIfNotExists` is called with an empty table name? The library should send the request and let the service reject it (HTTP 400); the library returns `false`.
- What happens when the bearer token is an empty string? The library sends the header as-is; the service will return 401/403 and the library returns `false` for the operation.
- What happens when the `x-ms-date` timestamp is generated just before a clock-skew window? The library uses the system clock; clock-skew handling is the caller's responsibility.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Shared Key constructor MUST store the account name, account key, and table endpoint for subsequent use.
- **FR-002**: Every HTTP request sent by a Shared Key client MUST include an `Authorization: SharedKey {accountName}:{base64Signature}` header computed per the Azure Table Storage Shared Key specification.
- **FR-003**: The Shared Key signing process MUST use the **Shared Key Lite** format. The StringToSign is: `VERB\nContent-MD5\nContent-Type\nDate\n/{account}/{resource-path}`. The `Date` field MUST be the value of the `x-ms-date` header. Canonicalized headers and query parameters are NOT included (those belong to the Full Shared Key format used by Blob/Queue, not Table Storage).
- **FR-004**: The Shared Key signing process MUST base64-decode the account key, use it as the HMAC-SHA256 key to sign the StringToSign, and base64-encode the resulting digest to produce the signature.
- **FR-005**: Every HTTP request MUST include an `x-ms-date` header set to the current UTC time in RFC 1123 format and an `x-ms-version` header set to `2021-12-02`.
- **FR-006**: The bearer token constructor MUST store the table endpoint and bearer token for subsequent use.
- **FR-007**: Every HTTP request sent by a bearer token client MUST include an `Authorization: Bearer {token}` header.
- **FR-008**: `SetBearerToken()` MUST update the stored bearer token on a live client instance so that subsequent requests use the new token without requiring client reconstruction. When a bearer token is set (non-empty) on any client — including one originally constructed with Shared Key credentials — the bearer token MUST take priority for the `Authorization` header. SharedKey signing is used only when no bearer token is set.
- **FR-009**: `CreateTableIfNotExists` MUST send an HTTP POST to `{endpoint}/Tables` with a JSON body containing the table name. The library appends `/Tables` to the caller-provided endpoint as-is (no URL rewriting for Azurite vs. production). Trailing-slash normalization: if the endpoint already ends with `/`, the library MUST NOT produce a double slash.
- **FR-010**: `CreateTableIfNotExists` MUST treat HTTP 201 (Created) as success and return `true`.
- **FR-011**: `CreateTableIfNotExists` MUST treat HTTP 409 (Conflict / table already exists) as success and return `true`.
- **FR-012**: `CreateTableIfNotExists` MUST return `false` for any other HTTP status or network error.
- **FR-013**: The `Content-Type` header for `CreateTableIfNotExists` MUST be set to the appropriate JSON content type expected by Azure Table Storage.
- **FR-014**: The request MUST include an `Accept` header specifying JSON format so the service responds with JSON rather than Atom/XML.

### Key Entities

- **AzureTableClient**: The primary public type. Holds authentication credentials (account name + key, or bearer token), the table endpoint, and issues authenticated HTTP requests. Supports two construction modes (Shared Key, Bearer Token).
- **Table**: A named container in Azure Table Storage identified by its table name. Created via `CreateTableIfNotExists`. Serves as the target for all subsequent entity operations.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A Shared Key client can successfully authenticate against Azurite using the well-known dev-storage credentials — zero 403 responses for correctly formed requests.
- **SC-002**: `CreateTableIfNotExists` returns `true` when the table does not yet exist (HTTP 201 path).
- **SC-003**: `CreateTableIfNotExists` returns `true` when the table already exists (HTTP 409 path).
- **SC-004**: `CreateTableIfNotExists` returns `false` when the service returns an unexpected error (e.g., HTTP 500).
- **SC-005**: A bearer token client emits `Authorization: Bearer {token}` on every request and `SetBearerToken()` updates the token for subsequent requests without client reconstruction.
- **SC-006**: All Shared Key and CreateTableIfNotExists acceptance scenarios pass against a running Azurite instance with no manual intervention.

## Clarifications

### Session 2026-03-22

- Q: What happens when `SetBearerToken()` is called on a Shared Key-constructed client? → A: Bearer token takes priority when non-empty; SharedKey is used as fallback when no bearer token is set.
- Q: Which Shared Key StringToSign format — Lite or Full? → A: Shared Key Lite (`VERB\nContent-MD5\nContent-Type\nDate\n/{account}/{resource-path}`).
- Q: Which Azure Storage API version should the library target? → A: `2021-12-02` (well-supported by Azurite and production).
- Q: Should the library handle Azurite path-based URLs vs. production host-based URLs differently? → A: No. The library uses the caller-provided endpoint as-is and appends resource paths directly. The caller is responsible for providing the correct base URL for their environment.

## Assumptions

- The Azure Storage API version to target is `2021-12-02`, confirmed as well-supported by both Azurite and production Azure Table Storage.
- Azurite is available locally at `http://127.0.0.1:10002/devstoreaccount1` with the well-known dev-store credentials.
- Bearer token auth cannot be tested against Azurite; verification will rely on inspecting outgoing request headers (via a mock or integration test against a real Azure endpoint).
- The library does not acquire or refresh OAuth2 tokens. The caller provides and manages tokens.
- The `Content-MD5` field in the Shared Key StringToSign is empty for Table Storage operations (Azure Table Storage does not require it).
