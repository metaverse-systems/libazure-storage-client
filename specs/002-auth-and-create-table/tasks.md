# Tasks: Auth & CreateTableIfNotExists

**Input**: Design documents from `/specs/002-auth-and-create-table/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/public-api.md, quickstart.md

**Tests**: No test framework in project. Verification is manual against Azurite per plan.md.

**Organization**: Tasks grouped by user story. All implementation in `src/azure-storage-client.cpp` (single-source-file library). No new files created. Public header already declares all needed methods.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Add required includes and prepare the source file for implementation

- [ ] T001 Add `#include` directives for `<curl/curl.h>`, `<openssl/hmac.h>`, `<openssl/evp.h>`, `<openssl/buffer.h>`, `<ctime>`, `<cstring>`, and `<sstream>` to src/azure-storage-client.cpp, and open an anonymous namespace block for internal helpers

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Internal helper functions shared by all user stories. All helpers live in the anonymous namespace in `src/azure-storage-client.cpp`.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T002 Implement `base64_decode()` helper that decodes a base64 string to a `std::vector<unsigned char>` using OpenSSL `EVP_DecodeBlock()` in src/azure-storage-client.cpp
- [ ] T003 Implement `base64_encode()` helper that encodes raw bytes to a `std::string` using OpenSSL `EVP_EncodeBlock()` in src/azure-storage-client.cpp
- [ ] T004 Implement `build_date_string()` helper that returns current UTC time in RFC 1123 format (`"%a, %d %b %Y %H:%M:%S GMT"`) using `std::gmtime()` and `std::strftime()` in src/azure-storage-client.cpp
- [ ] T005 Implement `normalize_endpoint()` helper that strips a single trailing slash from a URL string, and update both constructors to call it on the `table_endpoint` argument before storing in src/azure-storage-client.cpp
- [ ] T006 Implement `perform_request()` helper that takes HTTP method, URL, a `std::vector<std::string>` of headers, and an optional body string; uses `curl_easy_init()`, sets `CURLOPT_URL`, `CURLOPT_CUSTOMREQUEST`, `CURLOPT_HTTPHEADER` (via `curl_slist_append`), `CURLOPT_POSTFIELDS` (if body non-empty), captures response body via `CURLOPT_WRITEFUNCTION`, retrieves status via `CURLINFO_RESPONSE_CODE`, cleans up with `curl_easy_cleanup()` and `curl_slist_free_all()`; returns `long` HTTP status code (0 on network error) in src/azure-storage-client.cpp

**Checkpoint**: Foundation ready — helpers compiled and available for user story implementation

---

## Phase 3: User Story 1 — Shared Key Authentication (Priority: P1) 🎯 MVP

**Goal**: Every HTTP request from a SharedKey-constructed client includes a correctly signed `Authorization: SharedKey {account}:{signature}` header, an `x-ms-date` header, and an `x-ms-version: 2021-12-02` header.

**Independent Test**: Construct a client with Azurite dev-storage credentials, call `CreateTableIfNotExists`, confirm no 403. (Verified in US2 phase.)

### Implementation for User Story 1

- [ ] T007 [US1] Implement `hmac_sha256_sign()` helper that base64-decodes the account key via `base64_decode()`, calls OpenSSL `HMAC(EVP_sha256(), ...)` with the decoded key and the StringToSign, and returns the base64-encoded signature via `base64_encode()` in src/azure-storage-client.cpp. Note: if `base64_decode()` fails on an invalid key, the resulting HMAC will be incorrect and the server will return 403, which maps to `false` via the error model — no explicit error handling needed.
- [ ] T008 [US1] Implement `build_string_to_sign()` helper that constructs the Table Service Shared Key StringToSign: `VERB + "\n" + content_md5 + "\n" + content_type + "\n" + date + "\n" + "/" + account_name + "/" + resource_path` per research R-002 in src/azure-storage-client.cpp
- [ ] T009 [US1] Implement `build_shared_key_auth_header()` helper that calls `build_string_to_sign()` and `hmac_sha256_sign()` and returns the complete header value `"SharedKey " + account_name + ":" + signature` in src/azure-storage-client.cpp

**Checkpoint**: SharedKey signing pipeline complete — ready to be consumed by CreateTableIfNotExists

---

## Phase 4: User Story 2 — CreateTableIfNotExists (Priority: P1)

**Goal**: `CreateTableIfNotExists("MyTable")` creates the table or returns success if it already exists. This is the end-to-end proof that authentication works.

**Independent Test**: Against Azurite, call `CreateTableIfNotExists` twice for the same table name. Both calls return `true`.

### Implementation for User Story 2

- [ ] T010 [US2] Implement `CreateTableIfNotExists()` method body: build URL as `this->table_endpoint + "/Tables"`, build JSON body `{"TableName":"<table_name>"}` using nlohmann-json, generate `x-ms-date` via `build_date_string()`, construct headers list (`Content-Type: application/json`, `Accept: application/json;odata=nometadata`, `x-ms-date`, `x-ms-version: 2021-12-02`), compute SharedKey `Authorization` header via `build_shared_key_auth_header()`, call `perform_request("POST", ...)`, return `true` for HTTP 201 or 409, `false` otherwise in src/azure-storage-client.cpp. Note: initially uses SharedKey auth only; bearer auth dispatch is added in T011.

**Checkpoint**: SharedKey auth + CreateTable verified end-to-end against Azurite (SC-001, SC-002, SC-003, SC-004, SC-006)

---

## Phase 5: User Story 3 — Bearer Token Authentication (Priority: P2)

**Goal**: Bearer token clients emit `Authorization: Bearer {token}`. `SetBearerToken()` updates the token on live instances. Bearer takes priority over SharedKey when non-empty.

**Independent Test**: Construct a bearer-token client, call an operation, inspect outgoing request for `Authorization: Bearer {token}`. Call `SetBearerToken()` with a new token and confirm subsequent requests use it.

### Implementation for User Story 3

- [ ] T011 [US3] Refactor auth dispatch in `CreateTableIfNotExists()`: before computing SharedKey auth, check if `this->bearer_token` is non-empty — if so, add `Authorization: Bearer {bearer_token}` header instead of SharedKey header; SharedKey auth is used only when bearer_token is empty and account_name/account_key are non-empty in src/azure-storage-client.cpp

**Checkpoint**: Both auth modes functional; bearer priority rule verified (SC-005)

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Build verification and end-to-end validation

- [ ] T012 Verify library compiles cleanly with `make` from repository root and fix any build errors in src/azure-storage-client.cpp
- [ ] T013 Run quickstart.md validation: start Azurite, build a test program using SharedKey + CreateTableIfNotExists, confirm both calls return success per specs/002-auth-and-create-table/quickstart.md
- [ ] T014 Verify failure path (SC-004): call `CreateTableIfNotExists` against a non-existent endpoint (e.g., `http://127.0.0.1:19999/fake`) and confirm it returns `false`
- [ ] T015 Verify bearer auth (SC-005): construct a bearer-token client, enable `CURLOPT_VERBOSE` or inspect outgoing request to confirm it includes `Authorization: Bearer {token}` header; call `SetBearerToken("new-token")` and confirm the updated token appears in subsequent requests

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Foundational — signing helpers require base64 + HMAC
- **US2 (Phase 4)**: Depends on US1 — CreateTable needs SharedKey auth to authenticate against Azurite
- **US3 (Phase 5)**: Depends on US2 — refactors auth dispatch already present in CreateTableIfNotExists
- **Polish (Phase 6)**: Depends on all user stories complete (T012–T015)

### User Story Dependencies

- **US1 (SharedKey Auth)**: Depends only on Foundational phase. No other story dependencies.
- **US2 (CreateTableIfNotExists)**: Depends on US1 (needs SharedKey auth for Azurite verification).
- **US3 (Bearer Token Auth)**: Depends on US2 (refactors auth dispatch code written in US2).

### Within Each User Story

- Helpers before composition (e.g., hmac_sha256_sign before build_shared_key_auth_header)
- Auth before operations (SharedKey signing before CreateTable)
- All tasks sequential — single source file prevents parallel edits

### Parallel Opportunities

- **None**: All tasks modify `src/azure-storage-client.cpp`. Single-source-file architecture means all edits are sequential.
- **Cross-phase**: No story parallelism — US2 depends on US1, US3 depends on US2.

---

## Implementation Strategy

### MVP First (User Story 1 + 2)

1. Complete Phase 1: Setup (T001)
2. Complete Phase 2: Foundational (T002–T006)
3. Complete Phase 3: US1 SharedKey Auth (T007–T009)
4. Complete Phase 4: US2 CreateTableIfNotExists (T010)
5. **STOP and VALIDATE**: Build library, run against Azurite, confirm CreateTableIfNotExists works
6. This covers SC-001 through SC-004 and SC-006

### Incremental Delivery

1. Setup + Foundational → Helpers compiled
2. Add US1 → SharedKey signing pipeline ready
3. Add US2 → CreateTableIfNotExists works against Azurite (MVP!)
4. Add US3 → Bearer token auth + priority dispatch added
5. Polish → Build verification + full quickstart validation

---

## Notes

- No [P] markers — all tasks in single source file `src/azure-storage-client.cpp`
- No test tasks — no test framework; verification is manual against Azurite
- Public header `include/libazure-storage-client/azure-storage-client.hpp` needs no changes (API already declared)
- Internal helpers go in anonymous namespace (not in public header, per constitution Principle IV)
- `Content-MD5` is always empty string for Table Storage operations (per research R-002)
- Canonicalized resource for CreateTable is `/{account}/Tables` (per research R-003)
