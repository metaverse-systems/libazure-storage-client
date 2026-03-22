# Tasks: Autotools Scaffold

**Input**: Design documents from `/specs/001-autotools-scaffold/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Not requested — no test tasks included.

**Organization**: Tasks grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create project directory structure and vendor required Autotools macros.

- [ ] T001 Create m4/ directory and vendor the AX_CXX_COMPILE_STDCXX macro into m4/ax_cxx_compile_stdcxx.m4. Download the canonical version from the GNU Autoconf Archive (https://git.savannah.gnu.org/cgit/autoconf-archive.git/plain/m4/ax_cxx_compile_stdcxx.m4) or copy from the system autoconf-archive package. This macro is invoked as `AX_CXX_COMPILE_STDCXX(20, noext, mandatory)` — see research.md §1.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core Autotools configuration that MUST be complete before ANY user story can be verified.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T002 [P] Create configure.ac at repository root. Must include: `AC_INIT([azure-storage-client], [0.0.1])`, `AM_INIT_AUTOMAKE([foreign subdir-objects])`, `LT_INIT`, `AC_PROG_CXX`, `AC_CONFIG_MACRO_DIRS([m4])`, `AX_CXX_COMPILE_STDCXX(20, noext, mandatory)`, `PKG_CHECK_MODULES([LIBCURL], [libcurl])`, `PKG_CHECK_MODULES([OPENSSL], [openssl])`, and `AC_CONFIG_FILES([Makefile src/Makefile azure-storage-client.pc])`. See research.md §1, §4 for details.
- [ ] T003 [P] Create top-level Makefile.am at repository root. Must include: `ACLOCAL_AMFLAGS = -I m4`, `SUBDIRS = src`, `pkgconfigdir = $(libdir)/pkgconfig`, `pkgconfig_DATA = azure-storage-client.pc`, and `EXTRA_DIST = m4/ax_cxx_compile_stdcxx.m4`.

**Checkpoint**: Foundation ready — user story implementation can now begin.

---

## Phase 3: User Story 1 — Build the shared library from source (Priority: P1) 🎯 MVP

**Goal**: A developer can run `autoreconf -fi && ./configure && make` and produce `libazure-storage-client.so`.

**Independent Test**: Run the full Autotools bootstrap-and-build sequence on a clean checkout. The build must succeed and produce the shared library artifact in `src/.libs/`.

### Implementation for User Story 1

- [ ] T004 [P] [US1] Create public header src/azure-storage-client.hpp with the full AzureTableClient class declaration. Include guards, `#include <string>`, `#include <vector>`, `#include <functional>`, `#include <libazure-storage-client/json.hpp>`. Declare all 14 public methods from data-model.md (two constructors, SetBearerToken, CreateTableIfNotExists, GetEntity, UpsertEntity, BatchUpsertEntities, QueryEntities, DeleteEntity, and all five async variants). Declare private members without underscore suffix (account_name, account_key, table_endpoint, bearer_token). See data-model.md for full method signatures and types.
- [ ] T005 [P] [US1] Create stub implementation src/azure-storage-client.cpp. Include `"azure-storage-client.hpp"`. Implement all 14 methods as stubs: constructors are no-ops, SetBearerToken is no-op, bool methods return false, GetEntity returns `nlohmann::json{}`, QueryEntities returns empty vector, async methods invoke their callback with the default value. Use `this->` for all member access — see data-model.md for stub behavior per method.
- [ ] T006 [US1] Create src/Makefile.am with library build rules. Set `lib_LTLIBRARIES = libazure-storage-client.la`, `libazure_storage_client_la_SOURCES = azure-storage-client.cpp`, `libazure_storage_client_la_CXXFLAGS = $(LIBCURL_CFLAGS) $(OPENSSL_CFLAGS) -I$(top_srcdir)/include`, `libazure_storage_client_la_LIBADD = $(LIBCURL_LIBS) $(OPENSSL_LIBS)`, `libazure_storage_client_la_LDFLAGS = -version-info 0:0:0`. Add header install target: `azurestorageincludedir = $(includedir)/azure-storage-client` and `azurestorageinclude_HEADERS = azure-storage-client.hpp`. See research.md §2 for naming conventions.
- [ ] T007 [US1] Verify build by running `autoreconf -fi && ./configure && make` from the repository root. Confirm libazure-storage-client.so exists in src/.libs/.

**Checkpoint**: User Story 1 complete — shared library builds from source.

---

## Phase 4: User Story 2 — Install the library and headers (Priority: P2)

**Goal**: `make install` places the shared library, public header, and pkg-config `.pc` file in the correct directories.

**Independent Test**: Run `make install DESTDIR=/tmp/staging`, then verify `libazure-storage-client.so` is under `lib/`, the header is under `include/azure-storage-client/`, and `azure-storage-client.pc` is under `lib/pkgconfig/`. Run `pkg-config --cflags --libs azure-storage-client` and confirm correct output.

### Implementation for User Story 2

- [ ] T008 [US2] Create pkg-config template azure-storage-client.pc.in at repository root. Use `@prefix@`, `@exec_prefix@`, `@libdir@`, `@includedir@`, `@PACKAGE_VERSION@` substitution variables. Set `Name: azure-storage-client`, `Requires: libcurl openssl`, `Libs: -L${libdir} -lazure-storage-client`, `Cflags: -I${includedir}`. See research.md §3 for template pattern.
- [ ] T009 [US2] Verify install by running `make install DESTDIR=/tmp/staging`. Check that `libazure-storage-client.so` exists under `<prefix>/lib/`, header exists under `<prefix>/include/azure-storage-client/`, and `azure-storage-client.pc` exists under `<prefix>/lib/pkgconfig/`. Run `PKG_CONFIG_PATH=/tmp/staging/usr/local/lib/pkgconfig pkg-config --cflags --libs azure-storage-client` and confirm correct flags.

**Checkpoint**: User Stories 1 AND 2 complete — library builds, installs, and is discoverable via pkg-config.

---

## Phase 5: User Story 3 — Cross-compile for Windows with MinGW (Priority: P3)

**Goal**: The Autotools build system accepts `--host=x86_64-w64-mingw32` and produces a Windows DLL.

**Independent Test**: Run `./configure --host=x86_64-w64-mingw32 && make` with the MinGW toolchain installed. Confirm a `.dll` or `.dll.a` is produced.

### Implementation for User Story 3

- [ ] T010 [US3] Verify MinGW cross-compilation by running `make distclean` (or from a clean build), then `./configure --host=x86_64-w64-mingw32 && make`. Confirm that `src/.libs/` contains a Windows DLL artifact (e.g., `libazure-storage-client-0.dll`). Requires `mingw-w64` package installed.

**Checkpoint**: All three user stories complete — native build, install, and cross-compilation all work.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: End-to-end validation across all stories.

- [ ] T011 Run full quickstart.md validation end-to-end: execute every command sequence from quickstart.md (build, install to staging, pkg-config test, MinGW cross-compile) and confirm all succeed.
- [ ] T012 Verify missing-dependency error messages: run `./configure` with `PKG_CONFIG_PATH` cleared or pointed at an empty directory to confirm that missing libcurl and OpenSSL produce clear, descriptive error messages from `PKG_CHECK_MODULES` (validates edge case EC-001 from spec.md).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (m4 macro must exist before configure.ac references it)
- **User Story 1 (Phase 3)**: Depends on Phase 2 (configure.ac and Makefile.am must exist)
- **User Story 2 (Phase 4)**: Depends on Phase 3 (library must build before install can be verified)
- **User Story 3 (Phase 5)**: Depends on Phase 2 (only needs configure.ac and source files; independent of US2)
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Depends on Foundational (Phase 2) — no dependencies on other stories
- **User Story 2 (P2)**: Depends on User Story 1 (must build before install)
- **User Story 3 (P3)**: Depends on Foundational (Phase 2) — can run in parallel with US2 after US1 completes

### Within Each Phase

- Tasks marked [P] within the same phase can run in parallel
- T004 and T005 (header and stub) can be written in parallel — both are independent files
- T006 (src/Makefile.am) can run in parallel with T004/T005 but logically references their filenames
- T007, T009, T010, T011 are verification tasks — they must run after their respective implementation tasks

### Parallel Opportunities

- T002 and T003 can run in parallel (different files)
- T004 and T005 can run in parallel (different files)
- After US1 is verified, US2 and US3 can proceed in parallel

---

## Parallel Example: User Story 1

```text
# Launch header and stub implementation together:
Task T004: "Create public header src/azure-storage-client.h"
Task T005: "Create stub implementation src/azure-storage-client.cpp"

# Then build rules (references T004/T005 filenames):
Task T006: "Create src/Makefile.am"

# Then verify:
Task T007: "Verify build with autoreconf -fi && ./configure && make"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (vendor m4 macro)
2. Complete Phase 2: Foundational (configure.ac, Makefile.am)
3. Complete Phase 3: User Story 1 (header, stub, src/Makefile.am, verify build)
4. **STOP and VALIDATE**: `autoreconf -fi && ./configure && make` succeeds

### Incremental Delivery

1. Setup + Foundational → Build system skeleton ready
2. Add User Story 1 → Library builds → **MVP!**
3. Add User Story 2 → Install and pkg-config work
4. Add User Story 3 → Cross-compilation verified
5. Each story adds value without breaking previous stories

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- No tests were requested — verification tasks (T007, T009, T010, T011) are manual build checks
- Member naming convention: no underscore suffix, always use `this->` (see data-model.md)
- Automake variable prefix uses underscores (`libazure_storage_client_la_*`) even though the library target uses hyphens
- Commit after each task or logical group
