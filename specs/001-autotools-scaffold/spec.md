# Feature Specification: Autotools Scaffold

**Feature Branch**: `001-autotools-scaffold`  
**Created**: 2026-03-22  
**Status**: Draft  
**Input**: User description: "Create a minimum C++ source file and implement autotools to build it into a library."

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Build the shared library from source (Priority: P1)

A developer clones the repository for the first time and wants to build `libazure-storage-client` as a shared library on a Linux workstation using the standard Autotools workflow (`autoreconf`, `configure`, `make`).

**Why this priority**: Without a working build, no other feature (CRUD operations, authentication, async API) can be developed or tested. This is the foundation for everything else.

**Independent Test**: Run the full Autotools bootstrap-and-build sequence on a clean checkout. The build must succeed and produce the shared library artifact.

**Acceptance Scenarios**:

1. **Given** a clean checkout of the repository with Autotools, a C++20 compiler, libcurl, OpenSSL, and nlohmann-json installed, **When** the developer runs `autoreconf -fi && ./configure && make`, **Then** the build completes without errors and produces a shared library file (`.so` on Linux).
2. **Given** the build has completed, **When** the developer inspects the build output directory, **Then** a shared library named `libazure-storage-client.so` (or equivalent libtool-managed `.la`) exists.

---

### User Story 2 — Install the library and headers (Priority: P2)

A developer or package maintainer wants to install the built library, its public header, and the pkg-config `.pc` file to a prefix so that downstream projects can find and link against it.

**Why this priority**: Installation and discoverability are required before any consumer can use the library. This validates the install targets and pkg-config integration.

**Independent Test**: Run `make install` with a `DESTDIR` staging directory, then verify the expected files appear in the correct locations.

**Acceptance Scenarios**:

1. **Given** a successful `make` build, **When** the developer runs `make install DESTDIR=/tmp/staging`, **Then** the shared library is installed under `<prefix>/lib/`, the public header is installed under `<prefix>/include/azure-storage-client/`, and a `azure-storage-client.pc` file is installed under `<prefix>/lib/pkgconfig/`.
2. **Given** the install is complete, **When** a downstream project runs `pkg-config --cflags --libs azure-storage-client`, **Then** it receives the correct include path and linker flags to compile and link against the library.

---

### User Story 3 — Cross-compile for Windows with MinGW (Priority: P3)

A developer wants to cross-compile the library from Linux targeting Windows using a MinGW toolchain.

**Why this priority**: Cross-platform portability is a constitution requirement. Validating the MinGW cross-build early prevents platform-specific regressions when real functionality is added later.

**Independent Test**: Run `./configure --host=x86_64-w64-mingw32` followed by `make` with the MinGW toolchain available.

**Acceptance Scenarios**:

1. **Given** a MinGW cross-compiler toolchain is installed on the Linux host, **When** the developer runs `./configure --host=x86_64-w64-mingw32 && make`, **Then** the build completes without errors and produces a `.dll` (or `.dll.a` import library) for Windows.

---

### Edge Cases

- What happens when required dependencies (libcurl, OpenSSL, nlohmann-json) are missing? The `configure` step must detect the absence and produce a clear error message rather than failing mid-build.
- What happens when the developer runs `make` without first running `configure`? Standard Autotools behavior applies — the Makefile will not exist, and the error is self-explanatory.
- What happens on a system with only a C++17 compiler? The `configure` step MUST check for C++20 support via a compiler-flag test and fail with a descriptive error if the compiler does not support it.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The repository MUST contain a minimal C++ source file (stub implementation) that compiles into a shared library. The source file does not need to contain real functionality — a placeholder is sufficient for this feature.
- **FR-002**: The repository MUST contain a public header file with the full `AzureTableClient` class declaration — all method signatures from the public API contract (constructors, `SetBearerToken`, `CreateTableIfNotExists`, entity CRUD, batch, query, delete, and all async variants) plus the necessary includes (`nlohmann/json.hpp`, `<string>`, `<vector>`, `<functional>`). Method bodies are stubs (unimplemented). This header is the file that gets installed for downstream consumers.
- **FR-003**: The build system MUST use Autotools: a `configure.ac` at the repository root, a top-level `Makefile.am`, and a `src/Makefile.am` for the library sources.
- **FR-004**: Running `autoreconf -fi && ./configure && make` on a system with all dependencies present MUST produce a shared library artifact.
- **FR-005**: The `configure` script MUST detect and require libcurl and OpenSSL (libcrypto) as external dependencies. If either is missing, `configure` MUST fail with a descriptive error. nlohmann-json is bundled in the repository under `/include` and does not require system detection — the build system MUST add the in-tree `/include` directory to the compiler include path.
- **FR-006**: The build MUST enforce C++20 as the language standard. The `configure` script MUST verify compiler support via a compiler-flag test (e.g., `AX_CXX_COMPILE_STDCXX(20, noext, mandatory)`) and fail with a descriptive error if the compiler does not support C++20.
- **FR-007**: `make install` MUST install the shared library to `<prefix>/lib/`, the public header to `<prefix>/include/azure-storage-client/`, and a pkg-config file (`azure-storage-client.pc`) to `<prefix>/lib/pkgconfig/`.
- **FR-008**: The pkg-config file MUST correctly advertise the include path, library path, and link flag so that `pkg-config --cflags --libs azure-storage-client` works for downstream consumers.
- **FR-009**: The `configure` script MUST support standard `--host` cross-compilation (specifically `x86_64-w64-mingw32` for Windows/MinGW).

## Clarifications

### Session 2026-03-22

- Q: Should the public header contain a forward declaration only, the full class with all method signatures, or a partial declaration? → A: Full class declaration with all method signatures from the public API; bodies are stubs/unimplemented.
- Q: How should configure detect nlohmann-json (pkg-config, header check, or fallback)? → A: Bundled copy in `/include` — no system detection needed; build adds the in-tree include path.
- Q: Library filename: hyphens (`libazure-storage-client.so`) or underscores (`libazure_storage_client.so`)? → A: Hyphens — matches constitution, repo name, and pkg-config package name.
- Q: How should configure verify C++20 support — compiler flag test or feature probe? → A: Compiler flag test only (`AX_CXX_COMPILE_STDCXX(20, noext, mandatory)` or equivalent).

## Assumptions

- The minimum C++ source file is a stub — an empty or near-empty implementation file that compiles. Real functionality (authentication, entity operations) will be added in subsequent features.
- The public header contains the full `AzureTableClient` class declaration with all method signatures. Method bodies in the `.cpp` are stubs (e.g., return default values). Downstream projects can compile against the header immediately.
- Standard Autotools conventions are followed: `autoreconf` regenerates the build system, `configure` checks the environment, `make` builds, `make install` installs.
- The developer's system has `autoconf`, `automake`, `libtool`, a C++20-capable compiler, libcurl, and OpenSSL installed. nlohmann-json is bundled in `/include` and requires no system package.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new developer can go from a clean clone to a successfully built shared library in under 5 commands (`autoreconf -fi`, `./configure`, `make`, and optionally `make install`).
- **SC-002**: The installed artifacts (`libazure-storage-client.so`, the public header, and the `.pc` file) are all present in the correct directories after `make install`.
- **SC-003**: A downstream project can discover and link against the library using only `pkg-config --cflags --libs azure-storage-client` — no manual path configuration required.
- **SC-004**: The build completes successfully for both native Linux compilation and MinGW cross-compilation for Windows.
