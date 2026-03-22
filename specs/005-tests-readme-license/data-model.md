# Data Model: Tests, README, and LICENSE

**Feature**: 005-tests-readme-license  
**Date**: 2026-03-22

## Overview

This feature does not introduce new data entities to the library. It defines the test fixture data structures used by the Catch2 test suite and the static file artifacts (README.md, LICENSE).

## Test Fixture Data

### Azurite Connection Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `ACCOUNT_NAME` | `devstoreaccount1` | Well-known Azurite dev account |
| `ACCOUNT_KEY` | `Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==` | Well-known Azurite dev key |
| `TABLE_ENDPOINT` | `http://127.0.0.1:10002/devstoreaccount1` | Default Azurite Table Storage endpoint |
| `TEST_TABLE` | `CatchTestTable` | Shared test table name |

### Test Entity Schema

All test entities follow the standard Azure Table Storage entity format:

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `PartitionKey` | string | Yes | Unique per test case (e.g., `"sync_upsert"`, `"async_get"`) |
| `RowKey` | string | Yes | Unique per entity within a test case |
| `Name` | string | No | Example custom property for testing |
| `Value` | int | No | Example custom property for testing |

### Test Case Key Isolation

Each test case uses a distinct PartitionKey to prevent data interference:

| Tag | Test Case | PartitionKey Pattern |
|-----|-----------|---------------------|
| `[sync]` | CreateTableIfNotExists | `"sync_create"` |
| `[sync]` | UpsertEntity + GetEntity | `"sync_upsert"` |
| `[sync]` | DeleteEntity | `"sync_delete"` |
| `[sync]` | QueryEntities | `"sync_query"` |
| `[sync]` | BatchUpsertEntities | `"sync_batch"` |
| `[async]` | GetEntityAsync | `"async_get"` |
| `[async]` | UpsertEntityAsync | `"async_upsert"` |
| `[async]` | DeleteEntityAsync | `"async_delete"` |
| `[async]` | QueryEntitiesAsync | `"async_query"` |
| `[async]` | BatchUpsertEntitiesAsync | `"async_batch"` |
| `[edge]` | Missing keys validation | N/A (no network call) |
| `[edge]` | Batch >100 entities | N/A (no network call) |
| `[edge]` | Non-existent entity | `"edge_noent"` |
| `[edge]` | Empty query result | `"edge_empty"` |

## Static Artifacts

### README.md

| Section | Content |
|---------|---------|
| Header | Project name, one-line description |
| Description | Library purpose, Azure Table Storage REST API, C++20 |
| Prerequisites | GCC/Clang, autotools, libcurl, openssl, nlohmann-json |
| Build | `autoreconf`, `./configure`, `make`, `make install` |
| Testing | Catch2 install, Azurite start, `make check` |
| Usage | Code example: SharedKey client, create table, upsert, get |
| pkg-config | `pkg-config --cflags --libs azure-storage-client` |
| License | MIT — reference to LICENSE file |

### LICENSE

Standard MIT license text. Copyright line: `Copyright (c) 2026 Metaverse Systems`.
