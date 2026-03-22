# libazure-storage-client Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-03-22

## Active Technologies
- C++20 + libcurl (HTTP transport), OpenSSL (HMAC-SHA256 + base64), nlohmann-json (JSON serialization) (002-auth-and-create-table)
- N/A (library is a REST client; Azure Table Storage is the remote store) (002-auth-and-create-table)
- C++20 + libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON) (003-sync-crud-operations)
- Azure Table Storage (remote via REST API); Azurite for local testing (003-sync-crud-operations)
- C++20 + libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON), `<thread>` (C++ standard library) (004-async-crud-operations)
- C++20 + libcurl, OpenSSL, nlohmann-json (existing); Catch2 v3 (new, test-only) (005-tests-readme-license)
- Azure Table Storage via REST API (tested against local Azurite emulator) (005-tests-readme-license)

- C++20 + libcurl (HTTP), OpenSSL/libcrypto (HMAC-SHA256), nlohmann-json (bundled in `include/`) (001-autotools-scaffold)

## Project Structure

```text
src/
tests/
```

## Commands

# Add commands for C++20

## Code Style

C++20: Follow standard conventions

## Recent Changes
- 005-tests-readme-license: Added C++20 + libcurl, OpenSSL, nlohmann-json (existing); Catch2 v3 (new, test-only)
- 004-async-crud-operations: Added C++20 + libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON), `<thread>` (C++ standard library)
- 003-sync-crud-operations: Added C++20 + libcurl (HTTP), OpenSSL (HMAC-SHA256/base64), nlohmann-json (JSON)


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
