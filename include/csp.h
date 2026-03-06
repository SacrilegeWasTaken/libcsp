#pragma once

#include <stdlib.h>
#include <string.h>


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


/* UNIQUE */
typedef struct {
  void *raw;
  size_t size;
} I_CSPUnique;

#define CSPUnique CSP_CLEANUP(cspunique_cleanup) I_CSPUnique
static inline void cspunique_cleanup(I_CSPUnique *ptr);
static inline I_CSPUnique cspunique_init(void *data, size_t size);
static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr);


/* RC */
typedef struct {
  void *raw;
  int *cnt;
} I_CSPRc;

#define CSPRc CSP_CLEANUP(csprc_cleanup) I_CSPRc
static inline void csprc_cleanup(I_CSPRc *ptr);
static inline I_CSPRc csprc_clone(I_CSPRc *ptr);
static inline I_CSPRc csprc_init(void *data);


/* ARC */
typedef struct {
  void *raw;
  _Atomic int *cnt;
} I_CSPArc;

#define CSPArc CSP_CLEANUP(csparc_cleanup) I_CSPArc
static inline void csparc_cleanup(I_CSPArc *ptr);
static inline I_CSPArc csparc_clone(I_CSPArc *ptr);
static inline I_CSPArc csparc_init(void *data);


/* COW (copy-on-write): clone = share; first write = copy then write */
typedef struct {
  void *raw;
  size_t size;
  int *cnt;
} I_CSPCow;

#define CSPCow CSP_CLEANUP(cspcow_cleanup) I_CSPCow
static inline void cspcow_cleanup(I_CSPCow *ptr);
static inline I_CSPCow cspcow_init(void *data, size_t size);
static inline I_CSPCow cspcow_clone(I_CSPCow *ptr);
static inline const void *cspcow_get(const I_CSPCow *ptr);
static inline void *cspcow_get_mut(I_CSPCow *ptr);


/* WEAKRC */
typedef struct {
  I_CSPRc *rc;
} I_CSPWeakRc;

static inline I_CSPWeakRc cspweakrc_init(I_CSPRc *rc);
static inline void *cspweakrc_try_get(I_CSPWeakRc *ptr);
static inline I_CSPWeakRc cspweakrc_clone(I_CSPWeakRc *ptr);


/* WEAKARC */
typedef struct {
  I_CSPArc *arc;
} I_CSPWeakArc;

static inline I_CSPWeakArc cspweakarc_init(I_CSPArc *arc);
static inline void *cspweakarc_try_get(I_CSPWeakArc *ptr);
static inline I_CSPWeakArc cspweakarc_clone(I_CSPWeakArc *ptr);


/* Generic Ref/Weak: one type, one API, dispatch inside functions */
enum { CSP_REF_RC, CSP_REF_ARC };

typedef struct {
  int tag;
  union {
    I_CSPRc rc;
    I_CSPArc arc;
  } u;
} CSPRefInner;

#define CSPRef CSP_CLEANUP(cspref_cleanup) CSPRefInner

static inline void cspref_cleanup(CSPRefInner *ptr);
static inline CSPRefInner cspref_init(void *data, int atomic);
static inline CSPRefInner cspref_clone(const CSPRefInner *ptr);

typedef struct {
  int tag;
  union {
    I_CSPWeakRc rc;
    I_CSPWeakArc arc;
  } u;
} CSPWeak;

static inline CSPWeak cspweak_init(CSPRefInner *ref);
static inline void *cspweak_try_get(const CSPWeak *ptr);
static inline CSPWeak cspweak_clone(const CSPWeak *ptr);


/* IMPLEMENTATION */


#ifdef CSP_IMPLEMENTATION


#ifdef CSP_DEBUG
#include <stdio.h>
#endif


/* WEAKARC IMPLEMENTATION */


static inline I_CSPWeakArc cspweakarc_init(I_CSPArc *arc) {
  return (I_CSPWeakArc) {
    .arc = arc,
  };
}

static inline I_CSPWeakArc cspweakarc_clone(I_CSPWeakArc *ptr) {
  return (I_CSPWeakArc) {
    .arc = ptr->arc,
  };
}

static inline void *cspweakarc_try_get(I_CSPWeakArc *ptr) {    
  if (!ptr || !ptr->arc || !ptr->arc->raw) {
    return NULL;
  }
  return ptr->arc->raw;
}


/* WEAKRC IMPLEMENTATION */


static inline I_CSPWeakRc cspweakrc_init(I_CSPRc *rc) {
  return (I_CSPWeakRc) {
    .rc = rc,
  };
}


static inline I_CSPWeakRc cspweakrc_clone(I_CSPWeakRc *ptr) {
  return (I_CSPWeakRc) {
    .rc = ptr->rc,
  };
}


static inline void *cspweakrc_try_get(I_CSPWeakRc *ptr) {    
  if (!ptr || !ptr->rc || !ptr->rc->raw) {
    return NULL;
  }
  return ptr->rc->raw;
}


/* UNIQUE IMPLEMENTATION */


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


/* RC IMPLEMENTATION */


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


/* ARC IMPLEMENTATION */


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


/* COW IMPLEMENTATION */

static inline void cspcow_cleanup(I_CSPCow *ptr) {
  if (!ptr || !ptr->raw || !ptr->cnt) return;
  *ptr->cnt -= 1;
  if (*ptr->cnt == 0) {
#ifdef CSP_DEBUG
    printf("CSPCow cleanup: %p, raw: %p\n", (void *)ptr, ptr->raw);
#endif
    free(ptr->raw);
    free(ptr->cnt);
  }
  ptr->raw = NULL;
  ptr->cnt = NULL;
}

static inline I_CSPCow cspcow_init(void *data, size_t size) {
  int *cnt = (int *)malloc(sizeof(int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPCow){ .raw = NULL, .size = 0, .cnt = NULL };
#endif
  }
  *cnt = 1;
  return (I_CSPCow){
    .raw = data,
    .size = size,
    .cnt = cnt,
  };
}

static inline I_CSPCow cspcow_clone(I_CSPCow *ptr) {
  if (ptr && ptr->cnt) *ptr->cnt += 1;
  return (I_CSPCow){
    .raw = ptr ? ptr->raw : NULL,
    .size = ptr ? ptr->size : 0,
    .cnt = ptr ? ptr->cnt : NULL,
  };
}

static inline const void *cspcow_get(const I_CSPCow *ptr) {
  return ptr && ptr->raw ? ptr->raw : NULL;
}

static inline void *cspcow_get_mut(I_CSPCow *ptr) {
  if (!ptr || !ptr->raw || !ptr->cnt) return NULL;
  if (*ptr->cnt > 1) {
    void *copy = malloc(ptr->size);
    if (!copy) {
#ifdef CSP_PANIC
      abort();
#else
      return NULL;
#endif
    }
    memcpy(copy, ptr->raw, ptr->size);
    *ptr->cnt -= 1;
    if (*ptr->cnt == 0) {
      free(ptr->raw);
      free(ptr->cnt);
    }
    ptr->raw = copy;
    ptr->cnt = (int *)malloc(sizeof(int));
    if (!ptr->cnt) {
      free(copy);
      ptr->raw = NULL;
#ifdef CSP_PANIC
      abort();
#endif
      return NULL;
    }
    *ptr->cnt = 1;
  }
  return ptr->raw;
}


/* Generic Ref/Weak implementation */
static inline void cspref_cleanup(CSPRefInner *ptr) {
  if (!ptr) return;
  if (ptr->tag == CSP_REF_RC)
    csprc_cleanup(&ptr->u.rc);
  else
    csparc_cleanup(&ptr->u.arc);
}

static inline CSPRefInner cspref_init(void *data, int atomic) {
  CSPRefInner r;
  if (atomic) {
    r.tag = CSP_REF_ARC;
    r.u.arc = csparc_init(data);
  } else {
    r.tag = CSP_REF_RC;
    r.u.rc = csprc_init(data);
  }
  return r;
}

static inline CSPRefInner cspref_clone(const CSPRefInner *ptr) {
  CSPRefInner r;
  if (!ptr) {
    r.tag = CSP_REF_RC;
    r.u.rc = (I_CSPRc){ .raw = NULL, .cnt = NULL };
    return r;
  }
  r.tag = ptr->tag;
  if (ptr->tag == CSP_REF_RC)
    r.u.rc = csprc_clone((I_CSPRc *)&ptr->u.rc);
  else
    r.u.arc = csparc_clone((I_CSPArc *)&ptr->u.arc);
  return r;
}

static inline CSPWeak cspweak_init(CSPRefInner *ref) {
  CSPWeak w;
  if (!ref) {
    w.tag = CSP_REF_RC;
    w.u.rc = (I_CSPWeakRc){ .rc = NULL };
    return w;
  }
  w.tag = ref->tag;
  if (ref->tag == CSP_REF_RC)
    w.u.rc = cspweakrc_init(&ref->u.rc);
  else
    w.u.arc = cspweakarc_init(&ref->u.arc);
  return w;
}

static inline void *cspweak_try_get(const CSPWeak *ptr) {
  if (!ptr) return NULL;
  if (ptr->tag == CSP_REF_RC)
    return cspweakrc_try_get((I_CSPWeakRc *)&ptr->u.rc);
  return cspweakarc_try_get((I_CSPWeakArc *)&ptr->u.arc);
}

static inline CSPWeak cspweak_clone(const CSPWeak *ptr) {
  CSPWeak w;
  if (!ptr) {
    w.tag = CSP_REF_RC;
    w.u.rc = (I_CSPWeakRc){ .rc = NULL };
    return w;
  }
  w.tag = ptr->tag;
  if (ptr->tag == CSP_REF_RC)
    w.u.rc = cspweakrc_clone((I_CSPWeakRc *)&ptr->u.rc);
  else
    w.u.arc = cspweakarc_clone((I_CSPWeakArc *)&ptr->u.arc);
  return w;
}

#endif
