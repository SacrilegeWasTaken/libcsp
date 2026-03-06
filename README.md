# libcps – smart pointers for C

Single-header C library that adds **RAII-style smart pointers** using the `cleanup` attribute (Clang/GCC). Requires C11 and `<stdatomic.h>`.

## Types

| Type | Description |
|------|-------------|
| **CSPUnique** | Unique ownership; automatic `free`; clone = deep copy. |
| **CSPRc** | Reference-counted shared ownership (single-threaded). |
| **CSPArc** | Atomic reference counting (thread-safe). |
| **CSPCow** | Copy-on-write: clone shares; first write copies then mutates. |
| **CSPWeakRc / CSPWeakArc** | Non-owning reference to Rc/Arc; `try_get` to access. |
| **CSPRef / CSPWeak** | Generic API: one type, same functions; choose Rc vs Arc via `cspref_init(data, 0)` or `cspref_init(data, 1)`. |

All owning types use the `cleanup` attribute so destructors run when the variable goes out of scope.

---

## Requirements

- **Compiler**: Clang or GCC, C11 or later.
- **Headers**: C11 atomics (`<stdatomic.h>`).

---

## Quick start

**1. One TU with implementation:**

```c
#define CSP_IMPLEMENTATION
#include "csp.h"
```

**2. Other files:**

```c
#include "csp.h"
```

**3. Build:**

```bash
cc -std=c11 -Wall -Wextra -Iinclude -c your.c
```

---

## API summary

### CSPUnique

```c
CSPUnique u = cspunique_init(malloc(sizeof(int)), sizeof(int));
CSPUnique u2 = cspunique_clone(&u);   // deep copy
// use u.raw, u2.raw; both freed on scope exit
```

### CSPRc / CSPArc

```c
CSPRc a = csprc_init(malloc(sizeof(int)));
CSPRc b = csprc_clone(&a);   // same buffer, refcount 2
// b cleaned up → refcount 1; a cleaned up → freed

CSPArc arc = csparc_init(malloc(sizeof(int)));
// Safe to pass &arc to threads; each thread clones, uses, drops
```

### CSPCow (copy-on-write)

```c
CSPCow c = cspcow_init(malloc(8), 8);
CSPCow c2 = cspcow_clone(&c);           // no copy, shared
const void *p = cspcow_get(&c);        // read
void *m = cspcow_get_mut(&c);          // if refcount > 1, copies then returns; else returns raw
```

### CSPRef / CSPWeak (generic)

```c
CSPRef r = cspref_init(data, 0);       // 0 = Rc, 1 = Arc
CSPRef r2 = cspref_clone(&r);
CSPWeak w = cspweak_init(&r);
void *raw = cspweak_try_get(&w);
```

### Weak (Rc/Arc specific)

```c
I_CSPWeakRc w = cspweakrc_init(&rc);
void *p = cspweakrc_try_get(&w);

I_CSPWeakArc wa = cspweakarc_init(&arc);
void *q = cspweakarc_try_get(&wa);
```

---

## Build & test

From repo root:

```bash
make test      # build test binary
make run-test  # build and run tests
```

Tests include single-thread coverage for all types and a **multithreaded stress test for CSPArc** (multiple threads clone, use, and drop; checks refcount and final value). Build uses `-pthread`.

Manual build:

```bash
cc -std=c11 -Wall -Wextra -pthread -Iinclude test/test.c -o test/test
./test/test
```

---

## Optional defines

- **`CSP_DEBUG`** – log cleanup calls to stdout (e.g. `CSPUnique cleanup: …, raw: …`).
- **`CSP_PANIC`** – on allocation failure in init/clone/get_mut, call `abort()` instead of returning an empty struct.

Define before `#include "csp.h"` in the TU where `CSP_IMPLEMENTATION` is defined.

---

## Layout

```
include/csp.h   – single header (types + implementation when CSP_IMPLEMENTATION)
test/test.c     – tests (Unique, Rc, Arc, Cow, Ref/Weak, WeakRc/WeakArc, Arc MT)
makefile        – test, run-test
flake.nix       – Nix flake for using the library from Nix
```

### Using from Nix

```bash
# Build the library (copies include/ to $out/include)
nix build

# Dev shell with C_INCLUDE_PATH set
nix develop

# Run tests
nix flake check
```

As a dependency in another flake:

```nix
inputs.libcps.url = "git+https://github.com/.../libcps";
# ...
buildInputs = [ inputs.libcps.packages.${system}.default ];
# Compile with -I${libcps}/include
```

---

## Limitations

- **Clang or GCC only** – uses `__attribute__((cleanup))`.
- **No custom destructors** – only `malloc`/`free` semantics.
- **Cycles** – if A holds Rc/Arc to B and B holds Rc/Arc to A (or a longer ring), refcounts never reach zero and memory leaks. Use **Weak** for one of the links (e.g. back-pointers in a tree) to break the cycle; no automatic cycle detection.
- **One implementation TU** – define `CSP_IMPLEMENTATION` in exactly one file.

---

## License

Use as you like (MIT/BSD-style); add your preferred license text.
