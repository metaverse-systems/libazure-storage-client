# Quickstart: Tests, README, and LICENSE

**Feature**: 005-tests-readme-license  
**Date**: 2026-03-22

## What This Feature Adds

1. A Catch2 v3 test suite exercising all 11 public methods of `AzureTableClient`
2. A README.md documenting build, test, and usage instructions
3. A LICENSE file with MIT license text

## Build & Run Tests

### Prerequisites

```bash
# Install Catch2 v3 (Ubuntu/Debian)
sudo apt install catch2

# Install Azurite (requires Node.js)
npm install -g azurite
```

### Build

```bash
autoreconf -fi
./configure
make
```

### Run Tests

```bash
# Start Azurite Table Storage emulator (in a separate terminal)
azurite-table &

# Build and run all tests
make check

# Or run directly with tag filtering
./tests/test_azure_table_client "[sync]"
./tests/test_azure_table_client "[async]"
./tests/test_azure_table_client "[edge]"
```

### Expected Output

```
All tests passed (N assertions in M test cases)
```

## Files Changed

| File | Action | Purpose |
|------|--------|---------|
| `configure.ac` | Modified | Add `PKG_CHECK_MODULES([CATCH2], [catch2-with-main])` and `tests/Makefile` |
| `Makefile.am` | Modified | Add `tests` to `SUBDIRS` (conditional on Catch2 availability) |
| `tests/Makefile.am` | New | Build rules for `test_azure_table_client` executable |
| `tests/test_azure_table_client.cpp` | New | All Catch2 test cases |
| `README.md` | New | Project documentation |
| `LICENSE` | New | MIT license text |

## Verification Checklist

- [ ] `./configure` succeeds with Catch2 installed (check for `checking for CATCH2... yes`)
- [ ] `./configure` succeeds without Catch2 installed (tests simply not built)
- [ ] `make check` builds and runs all tests against Azurite
- [ ] All sync tests pass (`[sync]` tag)
- [ ] All async tests pass (`[async]` tag)
- [ ] All edge-case tests pass (`[edge]` tag)
- [ ] README.md exists and contains all required sections
- [ ] LICENSE exists and contains MIT text with "2026 Metaverse Systems"
- [ ] `pkg-config --cflags --libs azure-storage-client` outputs correct flags (existing behavior, verified by README)
