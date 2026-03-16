#define CSP_IMPLEMENTATION
#define CSP_DEBUG
#define CSP_PANIC

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/csp.h"

/* ----------------------------------------------------------------------------
 * CSPUnique
 * ---------------------------------------------------------------------------- */

static void test_unique_basic(void) {
  int *data = (int *)malloc(sizeof(int));
  assert(data != CSP_NULL);
  *data = 42;

  CSPUnique u = csp_unique_from(data, sizeof(int));
  assert(data == CSP_NULL);
  assert(u.raw != CSP_NULL);
  assert(u.size == sizeof(int));

  CSPUnique u2 = cspunique_clone(&u);
  assert(u2.raw != CSP_NULL);
  assert(u2.raw != u.raw);
  assert(u2.size == u.size);
  assert(*(int *)u2.raw == 42);

  *(int *)u.raw = 7;
  assert(*(int *)u2.raw == 42);
}

static void test_unique_clone_null_empty(void) {
  I_CSPUnique empty = { .raw = CSP_NULL, .size = 0 };
  I_CSPUnique c = cspunique_clone(&empty);
  assert(c.raw == CSP_NULL);
  assert(c.size == 0);

  I_CSPUnique with_size = { .raw = CSP_NULL, .size = 8 };
  c = cspunique_clone(&with_size);
  assert(c.raw == CSP_NULL);
  assert(c.size == 0);
}

static void test_unique_chain_clones(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 1;
  CSPUnique u = csp_unique_from(data, sizeof(int));
  assert(data == CSP_NULL);

  CSPUnique u2 = cspunique_clone(&u);
  CSPUnique u3 = cspunique_clone(&u2);
  assert(u.raw != u2.raw);
  assert(u2.raw != u3.raw);
  assert(*(int *)u3.raw == 1);
}

static void test_unique_take(void) {
  void *dangerous_ptr = malloc(1024);
  assert(dangerous_ptr != CSP_NULL);

  CSPUnique safe = csp_unique_from(dangerous_ptr, 1024);
  
  // The original pointer must be completely nullified
  assert(dangerous_ptr == CSP_NULL);
  
  // The safe pointer should have the allocated memory
  assert(safe.raw != CSP_NULL);
  assert(safe.size == 1024);
}

/* ----------------------------------------------------------------------------
 * CSPRc
 * ---------------------------------------------------------------------------- */

static void test_rc_basic_refcount(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 1;

  CSPRc a = csp_rc_from(data);
  assert(data == CSP_NULL);
  assert(a.raw != CSP_NULL);
  assert(a.cnt != CSP_NULL);
  assert(*a.cnt == 1);

  {
    CSPRc b = csprc_clone(&a);
    assert(b.raw == a.raw);
    assert(b.cnt == a.cnt);
    assert(*a.cnt == 2);
  }

  assert(*a.cnt == 1);
}

static void test_rc_many_clones(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 0;

  CSPRc a = csp_rc_from(data);
  assert(data == CSP_NULL);
  for (int i = 0; i < 32; i++) {
    CSPRc b = csprc_clone(&a);
    assert(b.raw == a.raw);
    assert(*a.cnt == 2);
  }
  assert(*a.cnt == 1);
}

/* ----------------------------------------------------------------------------
 * CSPArc (single-thread + multithread)
 * ---------------------------------------------------------------------------- */

static void test_arc_basic_refcount(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 123;

  CSPArc a = csp_arc_from(data);
  assert(data == CSP_NULL);
  assert(a.raw != CSP_NULL);
  assert(a.cnt != CSP_NULL);
  assert(atomic_load(a.cnt) == 1);

  {
    CSPArc b = csparc_clone(&a);
    assert(b.raw == a.raw);
    assert(b.cnt == a.cnt);
    assert(atomic_load(a.cnt) == 2);
  }

  assert(atomic_load(a.cnt) == 1);
}

#define ARC_MT_THREADS 8
#define ARC_MT_LOOPS   50

typedef struct {
  I_CSPArc *shared_arc;
  int       id;
  int       err;
} arc_thread_arg_t;

static void *arc_thread_fn(void *arg) {
  arc_thread_arg_t *a = (arc_thread_arg_t *)arg;
  for (int i = 0; i < ARC_MT_LOOPS; i++) {
    CSPArc local = csparc_clone(a->shared_arc);
    if (!local.raw || !local.cnt) {
      a->err = 1;
      return (void *)(intptr_t)1;
    }
    _Atomic int *p = (_Atomic int *)local.raw;
    atomic_fetch_add(p, 1);
  }
  return CSP_NULL;
}

static void test_arc_multithreaded(void) {
  _Atomic int *data = (_Atomic int *)malloc(sizeof(_Atomic int));
  atomic_init(data, 0);

  CSPArc shared = csp_arc_from(data);
  assert(data == CSP_NULL);
  assert(shared.raw != CSP_NULL);
  assert(atomic_load(shared.cnt) == 1);

  pthread_t threads[ARC_MT_THREADS];
  arc_thread_arg_t args[ARC_MT_THREADS];

  for (int i = 0; i < ARC_MT_THREADS; i++) {
    args[i].shared_arc = &shared;
    args[i].id = i;
    args[i].err = 0;
    int r = pthread_create(&threads[i], CSP_NULL, arc_thread_fn, &args[i]);
    assert(r == 0);
  }

  for (int i = 0; i < ARC_MT_THREADS; i++) {
    void *res = CSP_NULL;
    pthread_join(threads[i], &res);
    assert(res == CSP_NULL);
    assert(args[i].err == 0);
  }

  assert(atomic_load(shared.cnt) == 1);
  assert(atomic_load((_Atomic int *)shared.raw) == ARC_MT_THREADS * ARC_MT_LOOPS);
}

/* ----------------------------------------------------------------------------
 * CSPCow
 * ---------------------------------------------------------------------------- */

static void test_cow_basic(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 100;

  CSPCow c = csp_cow_from(data, sizeof(int));
  assert(data == CSP_NULL);
  assert(cspcow_get(&c) != CSP_NULL);
  assert(*(const int *)cspcow_get(&c) == 100);

  CSPCow c2 = cspcow_clone(&c);
  assert(c2.raw == c.raw);
  assert(c2.cnt == c.cnt);
  assert(*c.cnt == 2);

  int *p = (int *)cspcow_get_mut(&c);
  assert(p != CSP_NULL);
  assert(p != data);
  assert(*p == 100);
  *p = 200;
  assert(*c.cnt == 1);

  assert(*(const int *)cspcow_get(&c2) == 100);
  assert(*c2.cnt == 1);
}

static void test_cow_get_mut_sole_owner(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 42;

  CSPCow c = csp_cow_from(data, sizeof(int));
  assert(data == CSP_NULL);
  assert(*c.cnt == 1);

  void *p = cspcow_get_mut(&c);
  assert(p == c.raw);
  assert(*(int *)p == 42);
  *(int *)p = 99;
  assert(*(int *)cspcow_get(&c) == 99);
}

static void test_cow_get_null(void) {
  I_CSPCow empty = { .raw = CSP_NULL, .size = 0, .cnt = CSP_NULL };
  assert(cspcow_get(&empty) == CSP_NULL);
  assert(cspcow_get_mut(&empty) == CSP_NULL);
}

static void test_cow_clone_chain(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 1;

  CSPCow c = csp_cow_from(data, sizeof(int));
  assert(data == CSP_NULL);
  CSPCow c2 = cspcow_clone(&c);
  CSPCow c3 = cspcow_clone(&c2);
  assert(*c.cnt == 3);

  int *p = (int *)cspcow_get_mut(&c2);
  assert(p != c.raw);
  assert(*p == 1);
  assert(*c2.cnt == 1);
  assert(*c.cnt == 2);
  assert(*c3.cnt == 2);
}

/* ----------------------------------------------------------------------------
 * CSPRef / CSPWeak (generic)
 * ---------------------------------------------------------------------------- */

static void test_ref_generic_rc(void) {
  int *d1 = (int *)malloc(sizeof(int));
  *d1 = 10;
  CSPRef r1 = csp_ref_from(d1, 0);
  assert(d1 == CSP_NULL);
  assert(r1.tag == CSP_REF_RC);
  assert(r1.u.rc.raw != CSP_NULL);

  CSPRef r2 = cspref_clone(&r1);
  assert(r2.tag == CSP_REF_RC);
  assert(r2.u.rc.raw == r1.u.rc.raw);
}

static void test_ref_generic_arc(void) {
  int *d2 = (int *)malloc(sizeof(int));
  *d2 = 20;
  CSPRef r3 = csp_ref_from(d2, 1);
  assert(d2 == CSP_NULL);
  assert(r3.tag == CSP_REF_ARC);
  assert(r3.u.arc.raw != CSP_NULL);

  CSPWeak w = cspweak_init(&r3);
  assert(w.tag == CSP_REF_ARC);
  assert(cspweak_try_get(&w) == r3.u.arc.raw);

  CSPWeak w2 = cspweak_clone(&w);
  assert(cspweak_try_get(&w2) == r3.u.arc.raw);
}

static void test_weak_after_ref_drop(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 7;
  CSPRef r = csp_ref_from(data, 1);
  assert(data == CSP_NULL);
  CSPWeak w = cspweak_init(&r);
  assert(cspweak_try_get(&w) == r.u.arc.raw);
  (void)r;
  (void)w;
}

/* ----------------------------------------------------------------------------
 * Weak (Rc/Arc specific)
 * ---------------------------------------------------------------------------- */

static void test_weakrc_basic(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 1;
  CSPRc rc = csp_rc_from(data);
  assert(data == CSP_NULL);
  I_CSPWeakRc w = cspweakrc_init(&rc);
  assert(cspweakrc_try_get(&w) == rc.raw);

  I_CSPWeakRc w2 = cspweakrc_clone(&w);
  assert(cspweakrc_try_get(&w2) == rc.raw);
}

static void test_weakarc_basic(void) {
  int *data = (int *)malloc(sizeof(int));
  *data = 2;
  CSPArc arc = csp_arc_from(data);
  assert(data == CSP_NULL);
  I_CSPWeakArc w = cspweakarc_init(&arc);
  assert(cspweakarc_try_get(&w) == arc.raw);
}

/* ----------------------------------------------------------------------------
 * Runner
 * ---------------------------------------------------------------------------- */

int main(void) {
  test_unique_basic();
  test_unique_clone_null_empty();
  test_unique_chain_clones();
  test_unique_take();

  test_rc_basic_refcount();
  test_rc_many_clones();

  test_arc_basic_refcount();
  test_arc_multithreaded();

  test_cow_basic();
  test_cow_get_mut_sole_owner();
  test_cow_get_null();
  test_cow_clone_chain();

  test_ref_generic_rc();
  test_ref_generic_arc();
  test_weak_after_ref_drop();

  test_weakrc_basic();
  test_weakarc_basic();

  return 0;
}
