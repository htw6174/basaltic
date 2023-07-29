/*
 * Data structure that allows fast lookup of all values sharing a common key, and threadsafe access to those value lists
 * (Or at least allow writing from one thread and reading from another)
 * (TODO) Macro based to allow for user-defined key comparison (for now: use u32 keys, void* values)
 * Functionally similar to C++'s unordered_multimap
 *
 * Could make this a wrapper around a khash hashmap, where values are the start of a linked list of 'real' values
 * NOTE: **unfinished and unused for now**
 */

#ifndef HTW_MULTIMAP_H_INCLUDED
#define HTW_MULTIMAP_H_INCLUDED

#include <stdbool.h>
#include "htw_core.h"
#include "htw_random.h"

typedef u32 htw_mm_key;
typedef void* htw_mm_value;
typedef size_t htw_mm_iter; // Never greater than mm->maxItemCount, can be used directly as index

typedef struct {
    size_t count;
    size_t firstIndex;
    size_t currentIndex;
} htw_Iter;

typedef struct {
    size_t maxItemCount;
    size_t itemCount;
    size_t bucketCount;
    u32 hashBitMask;
    htw_mm_value *buckets;
} htw_Multimap;

static htw_Multimap *htw_mm_create(size_t maxItemCount); // TODO: macro version needs types and comparison method name
static void htw_mm_insert(htw_Multimap *mm, htw_mm_key key, htw_mm_value value);
static void htw_mm_remove(htw_Multimap *mm, htw_mm_iter i);
static void htw_mm_rekey(htw_Multimap *mm, htw_mm_iter i, htw_mm_key newKey);
static size_t htw_mm_count(htw_Multimap *mm, htw_mm_key key);
static htw_mm_iter htw_mm_find(htw_Multimap *mm, htw_mm_key key);
static bool htw_mm_next(htw_Multimap *mm, htw_mm_iter *i);
static htw_mm_value htw_mm_get(htw_Multimap *mm, htw_mm_iter i);

#define htw_mm_t(name) htw_mm_t_##name
#define htw_ll_t(val_t) htw_ll_t##val_t

#define __MM_IMPL(name, key_t, val_t, __hash_func, __hash_equal)    \
    htw_mm_t(name) *htw_mm_init##name(size_t maxEntries) {  \
        htw_mm_t(name) *newMM = calloc(1, sizeof(htw_mm_t(name)));  \
        khash_t(name) *khm = kh_init(name); \
        kh_resize(name, khm, maxEntries);   \
        newMM->kmap = khm;  \
        newMM->links = calloc(maxEntries, sizeof(htw_ll_t(val_t))); \
        return newMM;   \
    }   \

#define HTW_MM_INIT(name, key_t, val_t, __hash_func, __hash_equal) \
    typedef struct htw_ll_t(val_t) {    \
        value_t val;    \
        struct htw_ll_t(val_t) *next;   \
    } htw_ll_t(val_t);  \
	KHASH_INIT(name, key_t, htw_ll_t(val_t)*, 1, __hash_func, __hash_equal)   \
    typedef struct {    \
        htw_ll_t(val_t) links; \
        khash_t(name) kmap; \
    } htw_mm_t(name);  \
    __MM_IMPL(name, key_t, val_t, __hash_func, __hash_equal)    \

#define htw_mm_init(name, maxEntries) htw_mm_init##name(maxEntries)

#endif // HTW_MULTIMAP_H_INCLUDED
