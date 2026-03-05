## libcps – tiny smart pointers for C (Clang)

`libcps` is a small, header‑only helper library that brings **RAII‑style “smart pointers”** into plain C using Clang’s `cleanup` attribute.

It gives you three simple wrappers:

- **`CSPUnique`** – uniquely owned buffer with automatic `free`.
- **`CSPRc`** – reference‑counted shared ownership.
- **`CSPArc`** – atomic (thread‑safe) reference counting.

All three are designed to feel as close as possible to:

```c
{
  CSPUnique buf = /* ... */;
  // use buf.raw
} // buf is automatically cleaned up here
```

> **Note**  
> Implementation currently targets **Clang C only** (not C++ and not GCC/MSVC), and uses C11 features (`stdatomic.h`).

---

## Getting started

### Requirements

- **Compiler**: Clang (C mode), C11 or later.  
- **OS**: Tested on macOS, should work on other Clang‑based toolchains.

### Layout

The library is a single header:

- `include/csp.h`

Your project can just vendor this directory or add it as a submodule.

### Basic use

Pick exactly **one** C translation unit where you define the implementation:

```c
// e.g. csp_impl.c
#define CSP_IMPLEMENTATION
// optional debug / panic helpers
// #define CSP_DEBUG
// #define CSP_PANIC

#include "csp.h"
```

Everywhere else you only include the header:

```c
// some_module.c
#include "csp.h"
```

Compile with Clang, C11, and include path pointing at `include/`:

```bash
clang -std=c11 -Wall -Wextra -Iinclude -c some_module.c
```

---

## API overview

### Common ideas

Each smart pointer is a thin wrapper around a raw pointer:

- The wrapper itself is a `typedef` around an `I_...` struct.
- You always access the underlying memory as `ptr.raw` (and for counted types – `ptr.cnt`).
- RAII is implemented via the `cleanup` attribute:

```c
#define CSPUnique __attribute__((cleanup(cspunique_cleanup))) I_CSPUnique
```

So the **cleanup function is called automatically** when the variable goes out of scope.

---

### `CSPUnique` – unique ownership with deep copy

**Type:**

- Internal struct: `I_CSPUnique { void *raw; size_t size; }`
- Wrapper typedef: `CSPUnique`

**Key functions:**

```c
static inline I_CSPUnique cspunique_init(void *data, size_t size);
static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr);
```

**Usage:**

```c
void example_unique() {
  int *x = malloc(sizeof(int));
  *x = 42;

  CSPUnique u = cspunique_init(x, sizeof(int));

  // Deep copy: allocates separate buffer and copies contents
  CSPUnique u2 = cspunique_clone(&u);

  // u.raw and u2.raw point to different buffers
  *(int *)u.raw = 7;
  // u2 still sees 42
}
// leaving the scope automatically frees both u.raw and u2.raw
```

**When to use:**

- You want **single ownership** of a buffer and RAII cleanup.
- `clone` should produce a **fully independent copy** of the data.

---

### `CSPRc` – reference‑counted shared ownership

**Type:**

- Internal struct: `I_CSPRc { void *raw; int *cnt; }`
- Wrapper typedef: `CSPRc`

`cnt` is allocated on the heap and shared between all clones.

**Key functions:**

```c
static inline I_CSPRc csprc_init(void *data);
static inline I_CSPRc csprc_clone(I_CSPRc *ptr);
static inline void    csprc_cleanup(I_CSPRc *ptr);
```

**Semantics:**

- `csprc_init(data)`:
  - allocates `int *cnt`, initializes it to `1`,
  - returns `{ .raw = data, .cnt = cnt }`.
- `csprc_clone(&p)`:
  - increments `*p.cnt`,
  - returns another struct pointing at the same `raw` and `cnt`.
- `csprc_cleanup(&p)` (invoked automatically by `cleanup`):
  - decrements `*p.cnt`,
  - when it reaches `0`, frees both `raw` and `cnt`.

**Usage:**

```c
void example_rc() {
  int *x = malloc(sizeof(int));
  *x = 1;

  CSPRc a = csprc_init(x);

  {
    CSPRc b = csprc_clone(&a);
    // a.raw == b.raw, *a.cnt == 2
  } // b goes out of scope, cleanup runs, *a.cnt == 1

  // when a goes out of scope, refcount hits 0 and x is freed
}
```

**When to use:**

- Need **shared ownership** of a heap object inside a single thread / non‑atomic context.

---

### `CSPArc` – atomic (thread‑safe) reference counting

**Type:**

- Internal struct: `I_CSPArc { void *raw; atomic_int *cnt; }`
- Wrapper typedef: `CSPArc`

**Key functions:**

```c
static inline I_CSPArc csparc_init(void *data);
static inline I_CSPArc csparc_clone(I_CSPArc *ptr);
static inline void     csparc_cleanup(I_CSPArc *ptr);
```

**Semantics:**

- `csparc_init(data)`:
  - allocates `atomic_int *cnt`,
  - initializes it to `1` with `atomic_init`,
  - returns `{ .raw = data, .cnt = cnt }`.
- `csparc_clone(&p)`:
  - `atomic_fetch_add(p.cnt, 1)`; returns another struct to same data.
- `csparc_cleanup(&p)`:
  - `atomic_fetch_sub(p.cnt, 1)`; if previous value was `1`, frees `raw` and `cnt`.

**Usage:**

```c
void example_arc() {
  int *x = malloc(sizeof(int));
  *x = 123;

  // main thread
  CSPArc a = csparc_init(x);

  // in worker thread (pseudo‑code)
  // CSPArc b = csparc_clone(&a);
  // use b.raw safely from another thread

  // when all CSPArc instances drop out of scope on all threads,
  // x is freed exactly once.
}
```

**When to use:**

- Shared ownership **across threads**, where increments/decrements to the refcount must be atomic.

---

## Error handling and debug helpers

### `CSP_DEBUG`

If you define `CSP_DEBUG` before including `csp.h` in the implementation TU, the library logs basic lifecycle events to `stdout`:

- `CSPUnique cleanup: <wrapper>, raw: <ptr>`
- `CSPRc cleanup: <wrapper>, cnt: <n>, raw: <ptr>`
- `CSPArc cleanup: <wrapper>, cnt: <n>, raw: <ptr>`

This is useful for visually verifying that objects are freed at the expected times.

### `CSP_PANIC`

If you define `CSP_PANIC`, allocation failures inside the helpers call `abort()` instead of returning a “null” smart pointer:

- `cspunique_clone` on `malloc` failure,
- `csprc_init` on counter allocation failure,
- `csparc_init` on atomic counter allocation failure.

Without `CSP_PANIC`, those cases return a struct with `{ .raw = NULL, .cnt = NULL }` so callers can detect and handle the error.

---

## Examples

### Minimal end‑to‑end example

```c
// main.c
#define CSP_IMPLEMENTATION
#define CSP_DEBUG      // optional, for logging
// #define CSP_PANIC   // optional, aborts on allocation failures

#include "csp.h"

int main(void) {
  // Unique pointer
  int *u_data = malloc(sizeof(int));
  *u_data = 42;
  CSPUnique u = cspunique_init(u_data, sizeof(int));

  // Reference counted pointer
  int *rc_data = malloc(sizeof(int));
  *rc_data = 7;
  CSPRc rc = csprc_init(rc_data);
  CSPRc rc2 = csprc_clone(&rc);

  // Atomic reference counted pointer
  int *arc_data = malloc(sizeof(int));
  *arc_data = 123;
  CSPArc arc = csparc_init(arc_data);

  // Use u.raw / rc.raw / arc.raw like normal pointers
  return 0;
} // all three are cleaned up automatically here
```

Build:

```bash
clang -std=c11 -Wall -Wextra -Iinclude main.c -o main
```

---

## Testing

This repo already contains a small self‑test in `test/test.c`.

To build and run it from repo root:

```bash
clang -std=c11 -Wall -Wextra -Iinclude test/test.c -o test/test
./test/test
```

You should see `CSP_DEBUG` logs showing cleanups for all three pointer types.

---

## Limitations & notes

- **Clang C only**: the header currently uses `#if defined(__clang__) && !defined(__cplusplus)` to guard implementation; GCC/MSVC are not supported.
- **Not a general smart‑pointer framework**: there is no destructor callback mechanism – the code assumes plain `malloc`/`free` semantics.
- **No cycles**: like most refcounted schemes, `CSPRc`/`CSPArc` will leak on ownership cycles.
- **Inline definitions**: implementations live in the header guarded by `CSP_IMPLEMENTATION`; only one TU should define this macro in a real project.

---

## License

Add your preferred license here (MIT/BSD/…).

