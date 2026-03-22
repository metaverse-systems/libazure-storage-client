# libazure-storage-client Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-03-22

## Active Technologies
- C++20 + libcurl (HTTP transport), OpenSSL (HMAC-SHA256 + base64), nlohmann-json (JSON serialization) (002-auth-and-create-table)
- N/A (library is a REST client; Azure Table Storage is the remote store) (002-auth-and-create-table)

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
- 002-auth-and-create-table: Added C++20 + libcurl (HTTP transport), OpenSSL (HMAC-SHA256 + base64), nlohmann-json (JSON serialization)

- 001-autotools-scaffold: Added C++20 + libcurl (HTTP), OpenSSL/libcrypto (HMAC-SHA256), nlohmann-json (bundled in `include/`)

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
