#ifndef __QSORT_H__
#define __QSORT_H__
#include "types.h"

#ifndef __compar_fn_t_defined
#define __compar_fn_t_defined
typedef int (*__compar_fn_t)(const void *, const void *);
#endif

void qsort(void *__base, size_t __nmemb, size_t __size, __compar_fn_t _compar);
#endif /* __QSORT_H__ */