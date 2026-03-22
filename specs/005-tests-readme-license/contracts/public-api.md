# Public API Contract: Tests, README, and LICENSE

**Feature**: 005-tests-readme-license  
**Date**: 2026-03-22

## Overview

This feature does not modify the library's public API. The existing `AzureTableClient` class interface remains unchanged. This contract documents:
1. The build system interface changes (new configure/build targets)
2. The test executable interface
3. The static file contracts (README, LICENSE)

## Build System Interface

### New configure.ac Modules

```
PKG_CHECK_MODULES([CATCH2], [catch2-with-main], [have_catch2=yes], [have_catch2=no])
AM_CONDITIONAL([HAVE_CATCH2], [test "x$have_catch2" = "xyes"])
```

**Behavior**: Catch2 is optional. If not found, `./configure` succeeds but `make check` will not build or run tests.

### New Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| Build tests | `make check` | Builds and runs the test executable (only if Catch2 is available) |
| Run tests only | `./tests/test_azure_table_client` | Direct execution of test binary |
| Run tagged tests | `./tests/test_azure_table_client "[sync]"` | Run only tests with `[sync]` tag |
| Run tagged tests | `./tests/test_azure_table_client "[async]"` | Run only tests with `[async]` tag |
| Run tagged tests | `./tests/test_azure_table_client "[edge]"` | Run only tests with `[edge]` tag |

### Test Executable Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All tests passed |
| Non-zero | One or more tests failed |

## Test Prerequisites

| Prerequisite | Required For | How to Install |
|-------------|-------------|----------------|
| Catch2 v3 | Building tests | System package manager (e.g., `apt install catch2`) |
| Azurite | Running tests | `npm install -g azurite` |
| Node.js/npm | Installing Azurite | System package manager |

## Static File Contracts

### README.md

**Location**: Repository root (`/README.md`)  
**Format**: GitHub-Flavored Markdown

Required sections (in order):
1. `# libazure-storage-client` — project name
2. Description paragraph
3. `## Prerequisites` — build dependencies list
4. `## Building` — autoreconf, configure, make steps
5. `## Testing` — Azurite setup, make check
6. `## Usage` — C++ code example with pkg-config
7. `## License` — MIT reference with link to LICENSE

### LICENSE

**Location**: Repository root (`/LICENSE`)  
**Format**: Plain text  
**Content**: Standard MIT license (SPDX identifier: `MIT`)  
**Copyright line**: `Copyright (c) 2026 Metaverse Systems`

## Existing Public API (unchanged)

The `AzureTableClient` class API is documented in prior specs (002-004) and remains unchanged. For reference, the 11 public methods tested:

| Method | Return Type | Category |
|--------|------------|----------|
| `CreateTableIfNotExists(table_name)` | `bool` | sync |
| `GetEntity(table_name, pk, rk)` | `nlohmann::json` | sync |
| `UpsertEntity(table_name, entity)` | `bool` | sync |
| `BatchUpsertEntities(table_name, entities)` | `bool` | sync |
| `QueryEntities(table_name, filter)` | `vector<json>` | sync |
| `DeleteEntity(table_name, pk, rk)` | `bool` | sync |
| `GetEntityAsync(table_name, pk, rk, callback)` | `void` | async |
| `UpsertEntityAsync(table_name, entity, callback)` | `void` | async |
| `BatchUpsertEntitiesAsync(table_name, entities, callback)` | `void` | async |
| `QueryEntitiesAsync(table_name, filter, callback)` | `void` | async |
| `DeleteEntityAsync(table_name, pk, rk, callback)` | `void` | async |
