# Research: Asynchronous CRUD Operations

**Feature**: 004-async-crud-operations
**Date**: 2026-03-22

## R1: Copy-at-Dispatch Strategy

**Decision**: Create a copy of the entire `AzureTableClient` object at dispatch time and call sync methods on the copy within the detached thread.

**Rationale**: The sync methods directly access `this->account_name`, `this->account_key`, `this->table_endpoint`, and `this->bearer_token`. Rather than refactoring all sync methods to accept these as parameters (which would change internal structure), the simplest approach is to copy the client object. `AzureTableClient` has only 4 `std::string` members — copying is trivially cheap. The compiler-generated copy constructor is sufficient since there's no managed resources (no CURL handles stored as members).

**Alternatives considered**:
- **Extract free functions**: Refactor sync methods to accept state as parameters. Rejected — invasive refactoring that changes working code for no user benefit. Violates minimal surface area principle.
- **Capture individual strings in lambda**: Copy only the 4 member strings. Rejected — functionally equivalent to copying the object but more error-prone (easy to miss a member) and cannot use the sync methods directly.

## R2: Thread Model

**Decision**: Use `std::thread` with `.detach()`. Each async call spawns one detached thread.

**Rationale**: `std::thread` is C++ standard library (C++20), portable across GCC/Clang/MinGW, and meets the requirement of FR-009 (thread must not block caller or require manual joining). Detaching is appropriate because the caller has no mechanism to join (the async methods return `void`), and the copy-at-dispatch pattern means the thread is fully self-contained.

**Alternatives considered**:
- **`std::async` with `std::launch::async`**: Returns `std::future` which blocks in its destructor if not moved. Since the return type is `void`, the future would be destroyed at end of the async method, potentially blocking. Rejected.
- **Thread pool**: Adds complexity. Constitution Principle IV (Minimal Surface Area) and spec clarification both say no concurrency limit, no pool. Rejected.

## R3: Thread Safety of Sync Methods

**Decision**: No synchronization needed. Sync methods are already thread-safe for concurrent execution.

**Rationale**: Verified that `perform_request()` creates a fresh `curl_easy_init()` handle per call and cleans it up with `curl_easy_cleanup()`. No static or shared mutable state exists. Multiple threads can safely execute sync methods concurrently without data races — each gets its own CURL handle, its own header list, and its own response buffer.

**Alternatives considered**: None — this is a factual finding, not a design choice.

## R4: Callback Safety

**Decision**: Check if the callback is non-empty before invoking. Do not catch exceptions from user callbacks.

**Rationale**: Per FR-010, if the callback is empty (default-constructed `std::function`), the operation still executes but the callback is skipped. Per the edge case specification, exceptions from user callbacks propagate on the background thread — the library does not wrap them.

**Alternatives considered**:
- **Always invoke without checking**: Calling an empty `std::function` is undefined behavior (typically throws `std::bad_function_call`). Rejected.
- **Wrap in try/catch**: Constitution Principle IV prohibits the library from managing errors on the caller's behalf. Rejected.

## R5: Build System Changes

**Decision**: Add `-pthread` flag to compiler and linker in `src/Makefile.am`.

**Rationale**: `std::thread` requires pthread on Linux (GCC/Clang). The current `configure.ac` and `src/Makefile.am` do not include pthread. MinGW links winpthread automatically when `<thread>` is used; the `-pthread` flag is harmless on MinGW cross-compilation.

**Implementation**: Add `-pthread` to `AM_CXXFLAGS` and `LDFLAGS` in `src/Makefile.am`, or use `AX_PTHREAD` macro in `configure.ac`. The simpler approach (direct `-pthread` in `src/Makefile.am`) is preferred to avoid adding another m4 macro.

**Alternatives considered**:
- **`AX_PTHREAD` autoconf macro**: More portable detection of pthread. Rejected for now — the project already targets a known set of platforms (Linux GCC/Clang and MinGW) where `-pthread` is universally supported. Can be revisited if portability issues arise.

## R6: Parameter Lifetime in Async Methods

**Decision**: All parameters passed to async methods are captured by value (copy) in the lambda/thread closure.

**Rationale**: The async method returns immediately. If parameters were captured by reference, the caller's stack variables could be destroyed before the background thread uses them. `std::string` and `nlohmann::json` have value semantics and are safe to copy. `std::function` callbacks are also safely copyable.

**Alternatives considered**: None — capturing by reference would be a correctness bug.
