#define _GNU_SOURCE // Needed for RTLD_NEXT

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "./uthash/include/uthash.h"

static void* (*libc_malloc) (size_t)         = NULL;
static void  (*libc_free)   (void *)         = NULL;

struct malloc_item
{
	void *p;
	UT_hash_handle hh; // makes this structure hashable
};

// Hash table for accounting malloc/free
struct malloc_item *HT = NULL;

// Total malloc calls
static unsigned long malloc_count = 0;
static unsigned long malloc_limit = 0;

#define DEFAULT_MALLOC_LIMIT 5 // Default limit is 1000 malloc calls;

// Thread-local var to prevent malloc/free recursion
static __thread int no_hook;

// Wrapper to save original libc functions to global var
#define SAVE_LIBC_FUNC(var, f) do { \
	var = dlsym(RTLD_NEXT, f);\
	if (NULL == var) \
		perror("dlsym");\
} while(0)


static void account_alloc(void *ptr)
{
	// Do not account itself
	no_hook = 1;

	struct malloc_item *item = malloc(sizeof(*item));
	item->p = ptr;
	HASH_ADD_PTR(HT, p, item);

	malloc_count++;

	no_hook = 0;
}


// Initialize library parameters from environment.
// We don't have main(), so we have to call it in every callback.
static inline void init_env()
{
	char *t;

	if (malloc_limit == 0)
		malloc_limit = DEFAULT_MALLOC_LIMIT;
}


void *malloc(size_t size)
{
	void *p = NULL;

	if (libc_malloc == NULL) 
		SAVE_LIBC_FUNC(libc_malloc, "malloc");

	init_env();

	if (malloc_count < malloc_limit) {
		p = libc_malloc(size);
		if (p != NULL) 
			account_alloc(p);
	} else {
		errno = ENOMEM;
		return NULL;
	}

	return p;
}

void free(void *ptr)
{
	if (libc_free == NULL)
		SAVE_LIBC_FUNC(libc_free, "free");

	init_env();

	libc_free(ptr);

	struct malloc_item *found;
	HASH_FIND_PTR(HT, &ptr, found);
	if (found) {
		HASH_DEL(HT, found);
		free(found);
		malloc_count--;
	}
}
