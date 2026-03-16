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

/* C11/C23 compatibility */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  define CSP_NULL nullptr
#else
#  define CSP_NULL NULL
#endif
#if (defined(__clang__) || defined(__GNUC__))
#  define CSP_NODISCARD __attribute__((warn_unused_result))
#else
#  define CSP_NODISCARD
#endif

typedef void (*CSPDtor)(void *);
typedef void *(*CSPCloneFn)(const void *, size_t);

/* UNIQUE */
typedef struct {
  void *raw;
  size_t size;
  CSPDtor dtor;
  CSPCloneFn clone_fn;
} I_CSPUnique;

#define CSPUnique CSP_CLEANUP(cspunique_cleanup) I_CSPUnique
static inline void cspunique_cleanup(I_CSPUnique *ptr);
CSP_NODISCARD static inline I_CSPUnique __cspunique_init(void *data, size_t size, CSPDtor dtor, CSPCloneFn clone_fn);

#define csp_unique_from(ptr_var, size) \
    __cspunique_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), size, CSP_NULL, CSP_NULL)

#define csp_unique_from_dtor(ptr_var, size, dtor_fn, clone_fn_arg) \
    __cspunique_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), size, dtor_fn, clone_fn_arg)

CSP_NODISCARD static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr);


/* RC */
typedef struct {
  void *raw;
  int *cnt;
  CSPDtor dtor;
} I_CSPRc;

#define CSPRc CSP_CLEANUP(csprc_cleanup) I_CSPRc
static inline void csprc_cleanup(I_CSPRc *ptr);
CSP_NODISCARD static inline I_CSPRc csprc_clone(I_CSPRc *ptr);
CSP_NODISCARD static inline I_CSPRc __csprc_init(void *data, CSPDtor dtor);

#define csp_rc_from(ptr_var) \
    __csprc_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), CSP_NULL)

#define csp_rc_from_dtor(ptr_var, dtor_fn) \
    __csprc_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), dtor_fn)


/* ARC */
typedef struct {
  void *raw;
  _Atomic int *cnt;
  CSPDtor dtor;
} I_CSPArc;

#define CSPArc CSP_CLEANUP(csparc_cleanup) I_CSPArc
static inline void csparc_cleanup(I_CSPArc *ptr);
CSP_NODISCARD static inline I_CSPArc csparc_clone(I_CSPArc *ptr);
CSP_NODISCARD static inline I_CSPArc __csparc_init(void *data, CSPDtor dtor);

#define csp_arc_from(ptr_var) \
    __csparc_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), CSP_NULL)

#define csp_arc_from_dtor(ptr_var, dtor_fn) \
    __csparc_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), dtor_fn)


/* COW (copy-on-write): clone = share; first write = copy then write */
typedef struct {
  void *raw;
  size_t size;
  int *cnt;
  CSPDtor dtor;
  CSPCloneFn clone_fn;
} I_CSPCow;

#define CSPCow CSP_CLEANUP(cspcow_cleanup) I_CSPCow
static inline void cspcow_cleanup(I_CSPCow *ptr);
CSP_NODISCARD static inline I_CSPCow __cspcow_init(void *data, size_t size, CSPDtor dtor, CSPCloneFn clone_fn);

#define csp_cow_from(ptr_var, size) \
    __cspcow_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), size, CSP_NULL, CSP_NULL)

#define csp_cow_from_dtor(ptr_var, size, dtor_fn, clone_fn_arg) \
    __cspcow_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), size, dtor_fn, clone_fn_arg)

CSP_NODISCARD static inline I_CSPCow cspcow_clone(I_CSPCow *ptr);
static inline const void *cspcow_get(const I_CSPCow *ptr);
CSP_NODISCARD static inline void *cspcow_get_mut(I_CSPCow *ptr);


/* WEAKRC */
typedef struct {
  I_CSPRc *rc;
} I_CSPWeakRc;

CSP_NODISCARD static inline I_CSPWeakRc cspweakrc_init(I_CSPRc *rc);
CSP_NODISCARD static inline void *cspweakrc_try_get(I_CSPWeakRc *ptr);
CSP_NODISCARD static inline I_CSPWeakRc cspweakrc_clone(I_CSPWeakRc *ptr);


/* WEAKARC */
typedef struct {
  I_CSPArc *arc;
} I_CSPWeakArc;

CSP_NODISCARD static inline I_CSPWeakArc cspweakarc_init(I_CSPArc *arc);
CSP_NODISCARD static inline void *cspweakarc_try_get(I_CSPWeakArc *ptr);
CSP_NODISCARD static inline I_CSPWeakArc cspweakarc_clone(I_CSPWeakArc *ptr);


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
CSP_NODISCARD static inline CSPRefInner __cspref_init(void *data, int atomic, CSPDtor dtor);

#define csp_ref_from(ptr_var, atomic) \
    __cspref_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), atomic, CSP_NULL)

#define csp_ref_from_dtor(ptr_var, atomic, dtor_fn) \
    __cspref_init(({ \
        void *__tmp_ptr = (ptr_var); \
        (ptr_var) = CSP_NULL; \
        __tmp_ptr; \
    }), atomic, dtor_fn)

CSP_NODISCARD static inline CSPRefInner cspref_clone(const CSPRefInner *ptr);

typedef struct {
  int tag;
  union {
    I_CSPWeakRc rc;
    I_CSPWeakArc arc;
  } u;
} CSPWeak;

CSP_NODISCARD static inline CSPWeak cspweak_init(CSPRefInner *ref);
CSP_NODISCARD static inline void *cspweak_try_get(const CSPWeak *ptr);
CSP_NODISCARD static inline CSPWeak cspweak_clone(const CSPWeak *ptr);


/* IMPLEMENTATION */


#ifdef CSP_IMPLEMENTATION


#ifdef CSP_DEBUG
#include <stdio.h>
#endif


/* WEAKARC IMPLEMENTATION */


CSP_NODISCARD static inline I_CSPWeakArc cspweakarc_init(I_CSPArc *arc) {
  return (I_CSPWeakArc) {
    .arc = arc,
  };
}

CSP_NODISCARD static inline I_CSPWeakArc cspweakarc_clone(I_CSPWeakArc *ptr) {
  return (I_CSPWeakArc) {
    .arc = ptr->arc,
  };
}

CSP_NODISCARD static inline void *cspweakarc_try_get(I_CSPWeakArc *ptr) {    
  if (!ptr || !ptr->arc || !ptr->arc->raw) {
    return CSP_NULL;
  }
  return ptr->arc->raw;
}


/* WEAKRC IMPLEMENTATION */


CSP_NODISCARD static inline I_CSPWeakRc cspweakrc_init(I_CSPRc *rc) {
  return (I_CSPWeakRc) {
    .rc = rc,
  };
}


CSP_NODISCARD static inline I_CSPWeakRc cspweakrc_clone(I_CSPWeakRc *ptr) {
  return (I_CSPWeakRc) {
    .rc = ptr->rc,
  };
}


CSP_NODISCARD static inline void *cspweakrc_try_get(I_CSPWeakRc *ptr) {    
  if (!ptr || !ptr->rc || !ptr->rc->raw) {
    return CSP_NULL;
  }
  return ptr->rc->raw;
}


/* UNIQUE IMPLEMENTATION */


static inline void cspunique_cleanup(I_CSPUnique *ptr) {
  if (ptr && ptr->raw) {
#ifdef CSP_DEBUG
    printf("CSPUnique cleanup: %p\n, raw: %p\n", ptr, ptr->raw);
#endif
    if (ptr->dtor) {
      ptr->dtor(ptr->raw);
    } else {
      free(ptr->raw);
    }
    ptr->raw = CSP_NULL;
  }
}

CSP_NODISCARD static inline I_CSPUnique __cspunique_init(void *data, size_t size, CSPDtor dtor, CSPCloneFn clone_fn) {
  return (I_CSPUnique) {
    .raw      = data,
    .size     = size,
    .dtor     = dtor,
    .clone_fn = clone_fn,
  };
}

CSP_NODISCARD static inline I_CSPUnique cspunique_clone(I_CSPUnique *ptr) {
  if (!ptr || !ptr->raw || ptr->size == 0) {
    return (I_CSPUnique){ .raw = CSP_NULL, .size = 0, .dtor = CSP_NULL, .clone_fn = CSP_NULL };
  }
  
  void *copy = CSP_NULL;
  if (ptr->clone_fn) {
    copy = ptr->clone_fn(ptr->raw, ptr->size);
  } else {
    copy = malloc(ptr->size);
    if (copy) {
      memcpy(copy, ptr->raw, ptr->size);
    }
  }

  if (!copy) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPUnique){ .raw = CSP_NULL, .size = 0, .dtor = CSP_NULL, .clone_fn = CSP_NULL };
#endif
  }
  return (I_CSPUnique) {
    .raw      = copy,
    .size     = ptr->size,
    .dtor     = ptr->dtor,
    .clone_fn = ptr->clone_fn,
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
    if (ptr->dtor) {
      ptr->dtor(ptr->raw);
    } else {
      free(ptr->raw);
    }
    free(ptr->cnt);
  }

  ptr->raw = CSP_NULL;
  ptr->cnt = CSP_NULL;
}

CSP_NODISCARD static inline I_CSPRc csprc_clone(I_CSPRc *ptr) {
  if (ptr && ptr->cnt) {
    *ptr->cnt += 1;
  }
  return (I_CSPRc) {
    .raw  = ptr->raw,
    .cnt  = ptr->cnt,
    .dtor = ptr->dtor,
  };
}

CSP_NODISCARD static inline I_CSPRc __csprc_init(void *data, CSPDtor dtor) {
  int *cnt = (int *)malloc(sizeof(int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPRc) {
      .raw  = CSP_NULL,
      .cnt  = CSP_NULL,
      .dtor = CSP_NULL,
    };
#endif
  }
  *cnt = 1;
  return (I_CSPRc) {
    .raw  = data,
    .cnt  = cnt,
    .dtor = dtor,
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
          if (ptr->dtor) {
              ptr->dtor(ptr->raw);
          } else {
              free(ptr->raw);
          }
      }
      free(ptr->cnt);
  }

  ptr->raw = CSP_NULL;
  ptr->cnt = CSP_NULL;
}

CSP_NODISCARD static inline I_CSPArc csparc_clone(I_CSPArc *ptr) {
  if (ptr && ptr->cnt) {
      atomic_fetch_add(ptr->cnt, 1);
  }
  return (I_CSPArc) {
      .raw  = ptr->raw,
      .cnt  = ptr->cnt,
      .dtor = ptr->dtor,
  };
}

CSP_NODISCARD static inline I_CSPArc __csparc_init(void *data, CSPDtor dtor) {
  _Atomic int *cnt = (_Atomic int *)malloc(sizeof(_Atomic int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPArc) {
      .raw  = CSP_NULL,
      .cnt  = CSP_NULL,
      .dtor = CSP_NULL,
    };
#endif
  }
  
  atomic_init(cnt, 1);
  
  return (I_CSPArc) {
      .raw  = data,
      .cnt  = cnt,
      .dtor = dtor,
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
    if (ptr->dtor) {
      ptr->dtor(ptr->raw);
    } else {
      free(ptr->raw);
    }
    free(ptr->cnt);
  }
  ptr->raw = CSP_NULL;
  ptr->cnt = CSP_NULL;
}

CSP_NODISCARD static inline I_CSPCow __cspcow_init(void *data, size_t size, CSPDtor dtor, CSPCloneFn clone_fn) {
  int *cnt = (int *)malloc(sizeof(int));
  if (!cnt) {
#ifdef CSP_PANIC
    abort();
#else
    return (I_CSPCow){ .raw = CSP_NULL, .size = 0, .cnt = CSP_NULL, .dtor = CSP_NULL, .clone_fn = CSP_NULL };
#endif
  }
  *cnt = 1;
  return (I_CSPCow){
    .raw      = data,
    .size     = size,
    .cnt      = cnt,
    .dtor     = dtor,
    .clone_fn = clone_fn,
  };
}

CSP_NODISCARD static inline I_CSPCow cspcow_clone(I_CSPCow *ptr) {
  if (ptr && ptr->cnt) *ptr->cnt += 1;
  return (I_CSPCow){
    .raw      = ptr ? ptr->raw : CSP_NULL,
    .size     = ptr ? ptr->size : 0,
    .cnt      = ptr ? ptr->cnt : CSP_NULL,
    .dtor     = ptr ? ptr->dtor : CSP_NULL,
    .clone_fn = ptr ? ptr->clone_fn : CSP_NULL,
  };
}

static inline const void *cspcow_get(const I_CSPCow *ptr) {
  return ptr && ptr->raw ? ptr->raw : CSP_NULL;
}

CSP_NODISCARD static inline void *cspcow_get_mut(I_CSPCow *ptr) {
  if (!ptr || !ptr->raw || !ptr->cnt) return CSP_NULL;
  if (*ptr->cnt > 1) {
    void *copy = CSP_NULL;
    if (ptr->clone_fn) {
      copy = ptr->clone_fn(ptr->raw, ptr->size);
    } else {
      copy = malloc(ptr->size);
      if (copy) {
        memcpy(copy, ptr->raw, ptr->size);
      }
    }

    if (!copy) {
#ifdef CSP_PANIC
      abort();
#else
      return CSP_NULL;
#endif
    }

    *ptr->cnt -= 1;
    if (*ptr->cnt == 0) {
      if (ptr->dtor) {
        ptr->dtor(ptr->raw);
      } else {
        free(ptr->raw);
      }
      free(ptr->cnt);
    }
    ptr->raw = copy;
    ptr->cnt = (int *)malloc(sizeof(int));
    if (!ptr->cnt) {
      if (ptr->dtor) {
        ptr->dtor(copy);
      } else {
        free(copy);
      }
      ptr->raw = CSP_NULL;
#ifdef CSP_PANIC
      abort();
#endif
      return CSP_NULL;
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

CSP_NODISCARD static inline CSPRefInner __cspref_init(void *data, int atomic, CSPDtor dtor) {
  CSPRefInner r;
  if (atomic) {
    r.tag = CSP_REF_ARC;
    r.u.arc = __csparc_init(data, dtor);
  } else {
    r.tag = CSP_REF_RC;
    r.u.rc = __csprc_init(data, dtor);
  }
  return r;
}

CSP_NODISCARD static inline CSPRefInner cspref_clone(const CSPRefInner *ptr) {
  CSPRefInner r;
  if (!ptr) {
    r.tag = CSP_REF_RC;
    r.u.rc = (I_CSPRc){ .raw = CSP_NULL, .cnt = CSP_NULL, .dtor = CSP_NULL };
    return r;
  }
  r.tag = ptr->tag;
  if (ptr->tag == CSP_REF_RC)
    r.u.rc = csprc_clone((I_CSPRc *)&ptr->u.rc);
  else
    r.u.arc = csparc_clone((I_CSPArc *)&ptr->u.arc);
  return r;
}

CSP_NODISCARD static inline CSPWeak cspweak_init(CSPRefInner *ref) {
  CSPWeak w;
  if (!ref) {
    w.tag = CSP_REF_RC;
    w.u.rc = (I_CSPWeakRc){ .rc = CSP_NULL };
    return w;
  }
  w.tag = ref->tag;
  if (ref->tag == CSP_REF_RC)
    w.u.rc = cspweakrc_init(&ref->u.rc);
  else
    w.u.arc = cspweakarc_init(&ref->u.arc);
  return w;
}

CSP_NODISCARD static inline void *cspweak_try_get(const CSPWeak *ptr) {
  if (!ptr) return CSP_NULL;
  if (ptr->tag == CSP_REF_RC)
    return cspweakrc_try_get((I_CSPWeakRc *)&ptr->u.rc);
  return cspweakarc_try_get((I_CSPWeakArc *)&ptr->u.arc);
}

CSP_NODISCARD static inline CSPWeak cspweak_clone(const CSPWeak *ptr) {
  CSPWeak w;
  if (!ptr) {
    w.tag = CSP_REF_RC;
    w.u.rc = (I_CSPWeakRc){ .rc = CSP_NULL };
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
