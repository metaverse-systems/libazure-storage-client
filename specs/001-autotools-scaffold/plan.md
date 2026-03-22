# Implementation Plan: Autotools Scaffold

**Branch**: `001-autotools-scaffold` | **Date**: 2026-03-22 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-autotools-scaffold/spec.md`

## Summary

Create the foundational build infrastructure for libazure-storage-client: a minimal C++ stub source file, the full public header declaring `AzureTableClient`, and a complete Autotools build system (`configure.ac`, `Makefile.am`, `src/Makefile.am`, pkg-config `.pc.in`) that produces a shared library on Linux and cross-compiles for Windows via MinGW.

## Technical Context

**Language/Version**: C++20  
**Primary Dependencies**: libcurl (HTTP), OpenSSL/libcrypto (HMAC-SHA256), nlohmann-json (bundled in `include/`)  
**Storage**: N/A  
**Testing**: Manual build verification (`autoreconf -fi && ./configure && make && make install DESTDIR=...`)  
**Target Platform**: Linux (native), Windows (MinGW cross-compilation)  
**Project Type**: Shared library  
**Performance Goals**: N/A (scaffold only — no runtime code)  
**Constraints**: Build must complete with standard Autotools; no CMake/Meson  
**Scale/Scope**: Single shared library; ~2 source files (header + stub .cpp), ~5 build files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | Standalone Library — Zero Framework Coupling | PASS | No framework deps introduced; only libcurl, OpenSSL, bundled nlohmann-json |
| II | Azure REST API Fidelity | N/A | Scaffold only — no REST calls yet |
| III | Dual Authentication | N/A | Scaffold only — no auth code yet |
| IV | Minimal Surface Area | PASS | Header declares only the specified API contract; stub bodies do nothing extra |
| V | Cross-Platform Portability | PASS | Autotools with `--host` cross-compilation; C++20; `.so`/`.dll` output; pkg-config `.pc` |

**Gate result**: PASS — no violations. Proceed to Phase 0.

**Post-design re-check (Phase 1 complete)**: All gates still PASS. Design introduces no new dependencies, no framework coupling, no speculative features. The public header matches the API contract exactly. Build system uses Autotools only.

## Project Structure

### Documentation (this feature)

```text
specs/001-autotools-scaffold/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
configure.ac                         # Autotools top-level configure template
Makefile.am                          # Top-level automake file (SUBDIRS = src)
src/
├── Makefile.am                      # Library build rules (lib_LTLIBRARIES, sources, flags)
├── azure-storage-client.cpp         # Stub implementation (all methods return defaults)
└── azure-storage-client.h           # Full AzureTableClient class declaration (public header)
include/
└── libazure-storage-client/
    └── json.hpp                     # Bundled nlohmann-json (already present)
azure-storage-client.pc.in           # pkg-config template (substituted by configure)
m4/                                  # Autotools macro directory
└── ax_cxx_compile_stdcxx.m4         # AX_CXX_COMPILE_STDCXX macro for C++20 check
```

**Structure Decision**: Flat `src/` directory — a single `.cpp` and `.h` file. No subdirectories needed for a scaffold. The `include/` directory is pre-existing and holds the bundled nlohmann-json header. The `m4/` directory holds the standard Autoconf Archive macro for C++20 detection.

## Complexity Tracking

> No constitution violations — this section is intentionally empty.
