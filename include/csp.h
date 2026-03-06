#pragma once

#include <stdlib.h>
#include <string.h>

/* Portable-ish cleanup macro:
 * - enabled for Clang/GCC in C mode
 * - a no-op for other compilers / in C++
 */

#if (defined(__clang__) || defined(__GNUC__))
#  define CSP_CLEANUP(fn) __attribute__((cleanup(fn)))
#else
#  error "CSP is not supported on this compiler"
#endif

#if !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#else
#  error "CSP requires C11 atomics (<stdatomic.h>)"
#endif

typedef struct {
  void *raw;
  size_t size;
} I_CSPUnique;

#define CSPUnique CSP_CLEANUP(cspunique_cleanup) I_CSPUnique
static inline void cspunique_cleanup(I_CSPUnique *ptr);
static inline I_CSPUnique cspunique_init(void *data, size_t size);
static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr);


typedef struct {
  void *raw;
  int *cnt;
} I_CSPRc;


#define CSPRc CSP_CLEANUP(csprc_cleanup) I_CSPRc
static inline void csprc_cleanup(I_CSPRc *ptr);
static inline I_CSPRc csprc_clone(I_CSPRc *ptr);
static inline I_CSPRc csprc_init(void *data);



typedef struct {
  void *raw;
  _Atomic int *cnt;
} I_CSPArc;

#define CSPArc CSP_CLEANUP(csparc_cleanup) I_CSPArc
static inline void csparc_cleanup(I_CSPArc *ptr);
static inline I_CSPArc csparc_clone(I_CSPArc *ptr);
static inline I_CSPArc csparc_init(void *data);







#ifdef CSP_IMPLEMENTATION


#ifdef CSP_DEBUG
#include <stdio.h>
#endif


static inline void cspunique_cleanup(I_CSPUnique *ptr) {
  if (ptr && ptr->raw) {
#ifdef CSP_DEBUG
    printf("CSPUnique cleanup: %p\n, raw: %p\n", ptr, ptr->raw);
#endif
    free(ptr->raw);
    ptr->raw = NULL;
  }
}

static inline I_CSPUnique cspunique_init(void *data, size_t size) {
  return (I_CSPUnique) {
    .raw  = data,
    .size = size,
  };
}

static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr) {
  if (!ptr || !ptr->raw || ptr->size == 0) {
    return (I_CSPUnique){ .raw = NULL, .size = 0 };
  }
  void *copy = malloc(ptr->size);
  if (!copy) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPUnique){ .raw = NULL, .size = 0 };
#endif
  }
  memcpy(copy, ptr->raw, ptr->size);
  return (I_CSPUnique) {
    .raw  = copy,
    .size = ptr->size,
  };
}




static inline void csprc_cleanup(I_CSPRc *ptr) {
  if (!ptr || !ptr->raw || !ptr->cnt) {
    return;
  }
  *ptr->cnt -= 1;
  if (*ptr->cnt == 0) {
#ifdef CSP_DEBUG
    printf("CSPRc cleanup: %p\n, cnt: %d\n, raw: %p\n", ptr, *ptr->cnt, ptr->raw);
#endif
    free(ptr->raw);
    free(ptr->cnt);
  }

  ptr->raw = NULL;
  ptr->cnt = NULL;
}

static inline I_CSPRc csprc_clone(I_CSPRc *ptr) {
  if (ptr && ptr->cnt) {
    *ptr->cnt += 1;
  }
  return (I_CSPRc) {
    .raw = ptr->raw,
    .cnt = ptr->cnt,
  };
}

static inline I_CSPRc csprc_init(void *data) {
  int *cnt = (int *)malloc(sizeof(int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPRc) {
      .raw = NULL,
      .cnt = NULL,
    };
#endif
  }
  *cnt = 1;
  return (I_CSPRc) {
    .raw = data,
    .cnt = cnt,
  };
}





static inline void csparc_cleanup(I_CSPArc *ptr) {
  if (!ptr || !ptr->cnt) {
      return;
  }

  if (atomic_fetch_sub(ptr->cnt, 1) == 1) {
#ifdef CSP_DEBUG
      printf("CSPArc cleanup: %p\n, cnt: %d\n, raw: %p\n", ptr, *ptr->cnt, ptr->raw);
#endif
      if (ptr->raw) {
          free(ptr->raw);
      }
      free(ptr->cnt);
  }

  ptr->raw = NULL;
  ptr->cnt = NULL;
}

static inline I_CSPArc csparc_clone(I_CSPArc *ptr) {
  if (ptr && ptr->cnt) {
      atomic_fetch_add(ptr->cnt, 1);
  }
  return (I_CSPArc) {
      .raw = ptr->raw,
      .cnt = ptr->cnt,
  };
}

static inline I_CSPArc csparc_init(void *data) {
  _Atomic int *cnt = (_Atomic int *)malloc(sizeof(_Atomic int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPArc) {
      .raw = NULL,
      .cnt = NULL,
    };
#endif
  }
  
  atomic_init(cnt, 1);
  
  return (I_CSPArc) {
      .raw = data,
      .cnt = cnt,
  };
}

#endif
