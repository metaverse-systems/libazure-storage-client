# Quickstart: Autotools Scaffold

**Feature**: 001-autotools-scaffold  
**Date**: 2026-03-22

## Prerequisites

- Linux workstation (or WSL)
- C++20-capable compiler (GCC 10+ or Clang 10+)
- Autotools: `autoconf`, `automake`, `libtool`
- `pkg-config`
- libcurl development headers (`libcurl4-openssl-dev` on Debian/Ubuntu)
- OpenSSL development headers (`libssl-dev` on Debian/Ubuntu)

On Debian/Ubuntu:

```bash
sudo apt-get install build-essential autoconf automake libtool \
  pkg-config libcurl4-openssl-dev libssl-dev
```

## Build

```bash
# 1. Bootstrap the Autotools build system
autoreconf -fi

# 2. Configure (detects compiler, libcurl, OpenSSL, C++20 support)
./configure

# 3. Build the shared library
make
```

After `make`, the library artifact is in `src/.libs/libazure-storage-client.so`.

## Install (staging)

```bash
make install DESTDIR=/tmp/staging
```

Verify installed files:

```bash
ls /tmp/staging/usr/local/lib/libazure-storage-client.*
ls /tmp/staging/usr/local/include/azure-storage-client/
ls /tmp/staging/usr/local/lib/pkgconfig/azure-storage-client.pc
```

## pkg-config test

```bash
export PKG_CONFIG_PATH=/tmp/staging/usr/local/lib/pkgconfig
pkg-config --cflags --libs azure-storage-client
```

Expected output (approximate):

```
-I/usr/local/include  -L/usr/local/lib -lazure-storage-client
```

## Cross-compile for Windows (MinGW)

```bash
# Requires: mingw-w64 cross-compiler toolchain
sudo apt-get install mingw-w64

./configure --host=x86_64-w64-mingw32
make
```

The output DLL is in `src/.libs/libazure-storage-client-0.dll`.

## Verify

At this stage, the library is a stub — all methods return default
values. The build, install, and pkg-config workflows are what this
feature validates. Real functionality is added in subsequent features.
