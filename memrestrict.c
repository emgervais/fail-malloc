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
	size_t size;
	UT_hash_handle hh; // makes this structure hashable
};

// Hash table for accounting malloc/free
struct malloc_item *HT = NULL;

// Total memory allocated
static unsigned long mem_allocated = 0;
static unsigned long mem_threshold = 0;

#define DEFAULT_MEM_THRESHOLD 1024 * 500 // Default threshold is 2 MiB;

// Thread-local var to prevent malloc/free recursion
static __thread int no_hook;

// Wrapper to save original libc functions to global var
#define SAVE_LIBC_FUNC(var, f) do { \
	var = dlsym(RTLD_NEXT, f);\
	if (NULL == var) \
		perror("dlsym");\
} while(0)


static void account_alloc(void *ptr, size_t size)
{
	// Do not account itself
	no_hook = 1;

	// Allocating memory
	if (size != 0) {
		struct malloc_item *item, *out;

		item = malloc(sizeof(*item));
		item->p = ptr;
		item->size = size;

		HASH_ADD_PTR(HT, p, item);

		mem_allocated += size;
	} 
	// Freeing memory
	else { 
		struct malloc_item *found;

		HASH_FIND_PTR(HT, &ptr, found);
		if (found) {
			mem_allocated -= found->size;
			HASH_DEL(HT, found);
			free(found);
		}
	}

	no_hook = 0;
}


static void account_realloc(void *p, void *ptr, size_t size)
{
	// Do not account itself
	no_hook = 1;

	// ptr == NULL is equivalent to malloc(size) 
	if (ptr == NULL) {
		account_alloc(p, size);
	}
	// size == 0 is equivalent to free(ptr), 
	// and p will be NULL
	else if (size == 0) {
		account_alloc(ptr, size);
	}
	// Now the real realloc
	else {
		// if ptr was moved previous area will be freed
		if (p != ptr) {
			account_alloc(ptr, 0);
			account_alloc(p, size);
		} else {
			struct malloc_item *found;
			int alloc_diff;

			HASH_FIND_PTR(HT, &ptr, found);
			if (found) {
				alloc_diff = size - found->size;

				mem_allocated += alloc_diff;
				found->size += alloc_diff;
			}
		}
	}

	no_hook = 0;
}


// Initialize library parameters from environment.
// We don't have main(), so we have to call it in every callback.
static inline void init_env()
{
	char *t;

	if (mem_threshold == 0)
		mem_threshold = DEFAULT_MEM_THRESHOLD;
}


void *malloc(size_t size)
{
	void *p = NULL;

	if (libc_malloc == NULL) 
		SAVE_LIBC_FUNC(libc_malloc, "malloc");

	init_env();

	if (mem_allocated + size <= mem_threshold) {
		p = libc_malloc(size);
	} else {
		errno = ENOMEM;
		return NULL;
	}

	if (!no_hook) 
		account_alloc(p, size);

	return p;
}

void free(void *ptr)
{
	if (libc_free == NULL)
		SAVE_LIBC_FUNC(libc_free, "free");

	init_env();

	libc_free(ptr);

	if (!no_hook)
		account_alloc(ptr, 0);
}
