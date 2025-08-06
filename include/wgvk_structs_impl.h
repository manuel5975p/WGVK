#ifndef WGPU_STRUCTS_IMPL_H
#define WGPU_STRUCTS_IMPL_H
#define VK_NO_PROTOTYPES
#include <wgvk.h>
#include <vulkan/vulkan_core.h>
#include <external/VmaUsage.h>
#include <wgvk_config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef TERMCTL_RESET
    #define TERMCTL_RESET   ""
    #define TERMCTL_RED     ""
    #define TERMCTL_GREEN   ""
    #define TERMCTL_YELLOW  ""
    #define TERMCTL_BLUE    ""
    #define TERMCTL_CYAN    ""
    #define TERMCTL_MAGENTA ""
    #define TERMCTL_WHITE   ""
#endif
#ifndef MAX_COLOR_ATTACHMENTS
    #define MAX_COLOR_ATTACHMENTS 4
#endif
#ifndef CLITERAL
    #ifdef __cplusplus
        #define CLITERAL(X) X
    #else
        #define CLITERAL(X) (X)
    #endif
#endif
#if defined(_MSC_VER) || defined(_MSC_FULL_VER) 
    #define rg_unreachable(...) __assume(false)
    #define rg_assume(Condition) __assume(Condition)
    #define rg_trap(...) __debugbreak();
#else 
    #define rg_unreachable(...) __builtin_unreachable()
    #define rg_assume(Condition) __builtin_assume(Condition)
    #define rg_trap(...) __builtin_trap();
#endif

#define rg_countof(X) (sizeof(X) / sizeof((X)[0]))
#ifndef RGAPI
    #if !defined(RG_SHARED) || RG_SHARED == 0
        #ifdef __cplusplus
        #define RGAPI extern "C"
        #else
        #define RGAPI
        #endif
    #elif defined(_WIN32)
        #if defined(RG_EXPORTS) && RG_EXPORTS != 0
            #ifdef __cplusplus
                #define RGAPI __declspec(dllexport)
            #else
                #define RGAPI extern "C" __declspec(dllexport)
            #endif
        #else
            #ifdef __cplusplus
                #define RGAPI __declspec(dllimport)
            #else
                #define RGAPI extern "C" __declspec(dllimport)
            #endif
        #endif
    #else
        #ifdef __cplusplus
            #define RGAPI extern "C" __attribute__((visibility("default")))
        #else
            #define RGAPI __attribute__((visibility("default")))
        #endif
    #endif
#endif
#ifndef assert
    #define assert(...)
#endif

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define Rectangle w__Rectangle
    #define LoadImage w__LoadImage
    #define DrawText w__DrawText
    #define DrawTextEx w__DrawTextEx
    #define ShowCursor w__ShowCursor
    #define AdapterType w__AdapterType
    #include <windows.h>
    #undef AdapterType
    #undef ShowCursor
    #undef LoadImage
    #undef DrawTextEx
    #undef DrawText
    #undef Rectangle
#else
    #include <pthread.h>
#endif

/* Basic Threading */

typedef struct {
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t id;
#endif
} wgvk_thread_t;

typedef void* (*wgvk_thread_func_t)(void*);

int wgvk_thread_create(wgvk_thread_t* thread, wgvk_thread_func_t func, void* arg);
int wgvk_thread_join  (wgvk_thread_t* thread, void** result);
int wgvk_thread_detach(wgvk_thread_t* thread);

/* Mutex */

typedef struct {
#if defined(_WIN32)
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mutex;
#endif
} wgvk_mutex_t;

int wgvk_mutex_init(wgvk_mutex_t* mutex);
int wgvk_mutex_destroy(wgvk_mutex_t* mutex);
int wgvk_mutex_lock(wgvk_mutex_t* mutex);
int wgvk_mutex_unlock(wgvk_mutex_t* mutex);

/* Condition Variable */

typedef struct {
#if defined(_WIN32)
    CONDITION_VARIABLE cond;
#else
    pthread_cond_t cond;
#endif
} wgvk_cond_t;

int wgvk_cond_init(wgvk_cond_t* cond);
int wgvk_cond_destroy(wgvk_cond_t* cond);
int wgvk_cond_wait(wgvk_cond_t* cond, wgvk_mutex_t* mutex);
int wgvk_cond_signal(wgvk_cond_t* cond);
int wgvk_cond_broadcast(wgvk_cond_t* cond);

/* Thread Pool */

typedef enum {
    WGVK_JOB_PENDING,
    WGVK_JOB_RUNNING,
    WGVK_JOB_COMPLETED
} wgvk_job_status;

typedef struct wgvk_job_t {
    wgvk_thread_func_t func;
    void* arg;
    void* result;
    volatile wgvk_job_status status;
    wgvk_mutex_t status_mutex;
    wgvk_cond_t status_cond;
    struct wgvk_job_t* next;
} wgvk_job_t;

typedef struct {
    wgvk_thread_t* workers;
    size_t num_threads;
    wgvk_job_t* head;
    wgvk_job_t* tail;
    wgvk_mutex_t queue_mutex;
    wgvk_cond_t queue_cond;
    volatile int stop;
} wgvk_thread_pool_t;

wgvk_thread_pool_t* wgvk_thread_pool_create(size_t num_threads);
void wgvk_thread_pool_destroy(wgvk_thread_pool_t* pool);
wgvk_job_t* wgvk_job_enqueue(wgvk_thread_pool_t* pool, wgvk_thread_func_t func, void* arg);
int wgvk_job_wait(wgvk_job_t* job, void** result);
void wgvk_job_destroy(wgvk_job_t* job);



// ================================
//    PTR HASH MAP IMPLEMENTAION
// ================================


#ifndef HASHMAP_SET_AND_VECTOR_H
#define HASHMAP_SET_AND_VECTOR_H



#ifndef PHM_INLINE_CAPACITY
#define PHM_INLINE_CAPACITY 3
#endif

#ifndef PHM_INITIAL_HEAP_CAPACITY
#define PHM_INITIAL_HEAP_CAPACITY 8
#endif

#ifndef PHM_LOAD_FACTOR_NUM
#define PHM_LOAD_FACTOR_NUM 3
#endif

#ifndef PHM_LOAD_FACTOR_DEN
#define PHM_LOAD_FACTOR_DEN 4
#endif

#ifndef PHM_HASH_MULTIPLIER
#define PHM_HASH_MULTIPLIER 0x9E3779B97F4A7C15ULL
#endif
#ifndef PHM_EMPTY_SLOT_KEY
#define PHM_EMPTY_SLOT_KEY NULL
#endif
#ifndef PHM_DELETED_SLOT_KEY
#define PHM_DELETED_SLOT_KEY ((void*)0xFFFFFFFFFFFFFFFF)
#endif
#define DEFINE_PTR_HASH_MAP(SCOPE, Name, ValueType)                                                                              \
                                                                                                                                 \
    typedef struct Name##_kv_pair {                                                                                              \
        void *key;                                                                                                               \
        ValueType value;                                                                                                         \
    } Name##_kv_pair;                                                                                                            \
                                                                                                                                 \
    typedef struct Name {                                                                                                        \
        uint64_t current_size;     /* Number of non-NULL keys */                                                                 \
        uint64_t current_capacity; /* Capacity of the heap-allocated table */                                                    \
        bool has_null_key;                                                                                                       \
        ValueType null_value;                                                                                                    \
        Name##_kv_pair* table; /* Pointer to the hash table data (heap-allocated) */                                             \
    } Name;                                                                                                                      \
                                                                                                                                 \
    static inline uint64_t Name##_hash_key(void *key) {                                                                          \
        assert(key != NULL);                                                                                                     \
        return ((uintptr_t)key) * PHM_HASH_MULTIPLIER;                                                                           \
    }                                                                                                                            \
                                                                                                                                 \
    /* Helper to round up to the next power of 2. Result can be 0 if v is 0 or on overflow from UINT64_MAX. */                   \
    static inline uint64_t Name##_round_up_to_power_of_2(uint64_t v) {                                                           \
        if (v == 0)                                                                                                              \
            return 0;                                                                                                            \
        v--;                                                                                                                     \
        v |= v >> 1;                                                                                                             \
        v |= v >> 2;                                                                                                             \
        v |= v >> 4;                                                                                                             \
        v |= v >> 8;                                                                                                             \
        v |= v >> 16;                                                                                                            \
        v |= v >> 32;                                                                                                            \
        v++;                                                                                                                     \
        return v;                                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_insert_entry(Name##_kv_pair *table, uint64_t capacity, void *key, ValueType value) {                      \
        assert(key != NULL && key != PHM_EMPTY_SLOT_KEY && capacity > 0 && (capacity & (capacity - 1)) == 0);                    \
        uint64_t cap_mask = capacity - 1;                                                                                        \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        while (table[index].key != PHM_EMPTY_SLOT_KEY) {                                                                         \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        table[index].key = key;                                                                                                  \
        table[index].value = value;                                                                                              \
    }                                                                                                                            \
                                                                                                                                 \
    static Name##_kv_pair *Name##_find_slot(Name *map, void *key) {                                                              \
        assert(key != NULL && key != PHM_EMPTY_SLOT_KEY && map->table != NULL && map->current_capacity > 0);                     \
        uint64_t cap_mask = map->current_capacity - 1;                                                                           \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        while (map->table[index].key != PHM_EMPTY_SLOT_KEY && map->table[index].key != key) {                                    \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        return &map->table[index];                                                                                               \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_grow(Name *map); /* Forward declaration */                                                                \
                                                                                                                                 \
    SCOPE void Name##_init(Name *map) {                                                                                          \
        map->current_size = 0;                                                                                                   \
        map->current_capacity = 0;                                                                                               \
        map->has_null_key = false;                                                                                               \
        /* map->null_value is uninitialized, which is fine */                                                                    \
        map->table = NULL;                                                                                                       \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_grow(Name *map) {                                                                                         \
        uint64_t old_capacity = map->current_capacity;                                                                           \
        Name##_kv_pair *old_table = map->table;                                                                                  \
        uint64_t new_capacity;                                                                                                   \
                                                                                                                                 \
        if (old_capacity == 0) {                                                                                                 \
            new_capacity = (PHM_INITIAL_HEAP_CAPACITY > 0) ? PHM_INITIAL_HEAP_CAPACITY : 8; /* Default 8 if initial is 0 */      \
        } else {                                                                                                                 \
            if (old_capacity >= (UINT64_MAX / 2))                                                                                \
                new_capacity = UINT64_MAX; /* Avoid overflow */                                                                  \
            else                                                                                                                 \
                new_capacity = old_capacity * 2;                                                                                 \
        }                                                                                                                        \
                                                                                                                                 \
        new_capacity = Name##_round_up_to_power_of_2(new_capacity);                                                              \
        if (new_capacity == 0 && old_capacity == 0 && ((PHM_INITIAL_HEAP_CAPACITY > 0) ? PHM_INITIAL_HEAP_CAPACITY : 8) > 0) {   \
            /* This case means round_up_to_power_of_2 resulted in 0 from a non-zero initial desired capacity (e.g. UINT64_MAX)   \
             */                                                                                                                  \
            /* If PHM_INITIAL_HEAP_CAPACITY was huge and overflowed round_up. Use max power of 2. */                             \
            new_capacity = (UINT64_C(1) << 63);                                                                                  \
        }                                                                                                                        \
                                                                                                                                 \
        if (new_capacity == 0 || (new_capacity <= old_capacity && old_capacity > 0)) {                                           \
            return; /* Cannot grow or no actual increase in capacity */                                                          \
        }                                                                                                                        \
                                                                                                                                 \
        Name##_kv_pair *new_table = (Name##_kv_pair *)calloc(new_capacity, sizeof(Name##_kv_pair));                              \
        if (!new_table)                                                                                                          \
            return; /* Allocation failure */                                                                                     \
                                                                                                                                 \
        if (old_table && map->current_size > 0) {                                                                                \
            uint64_t rehashed_count = 0;                                                                                         \
            for (uint64_t i = 0; i < old_capacity; ++i) {                                                                        \
                if (old_table[i].key != PHM_EMPTY_SLOT_KEY) {                                                                    \
                    Name##_insert_entry(new_table, new_capacity, old_table[i].key, old_table[i].value);                          \
                    rehashed_count++;                                                                                            \
                    if (rehashed_count == map->current_size)                                                                     \
                        break;                                                                                                   \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
        if (old_table)                                                                                                           \
            free(old_table);                                                                                                     \
        map->table = new_table;                                                                                                  \
        map->current_capacity = new_capacity;                                                                                    \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE int Name##_put(Name *map, void *key, ValueType value) {                                                                \
        if (key == NULL) {                                                                                                       \
            map->null_value = value;                                                                                             \
            if (!map->has_null_key) {                                                                                            \
                map->has_null_key = true;                                                                                        \
                return 1; /* New NULL key */                                                                                     \
            }                                                                                                                    \
            return 0; /* Updated NULL key */                                                                                     \
        }                                                                                                                        \
        assert(key != PHM_EMPTY_SLOT_KEY);                                                                                       \
                                                                                                                                 \
        if (map->current_capacity == 0 ||                                                                                        \
            (map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM) {                      \
            uint64_t old_cap = map->current_capacity;                                                                            \
            Name##_grow(map);                                                                                                    \
            if (map->current_capacity == old_cap && old_cap > 0) { /* Grow failed or no increase */                              \
                /* Re-check if still insufficient */                                                                             \
                if ((map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM)                \
                    return 0;                                                                                                    \
            } else if (map->current_capacity == 0)                                                                               \
                return 0; /* Grow failed to allocate any capacity */                                                             \
            else if ((map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM)               \
                return 0;                                                                                                        \
        }                                                                                                                        \
        assert(map->current_capacity > 0 && map->table != NULL); /* Must have capacity after grow check */                       \
                                                                                                                                 \
        Name##_kv_pair *slot = Name##_find_slot(map, key);                                                                       \
        if (slot->key == PHM_EMPTY_SLOT_KEY) {                                                                                   \
            slot->key = key;                                                                                                     \
            slot->value = value;                                                                                                 \
            map->current_size++;                                                                                                 \
            return 1; /* New key */                                                                                              \
        } else {                                                                                                                 \
            assert(slot->key == key);                                                                                            \
            slot->value = value;                                                                                                 \
            return 0; /* Updated existing key */                                                                                 \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE ValueType *Name##_get(Name *map, void *key) {                                                                          \
        if (key == NULL)                                                                                                         \
            return map->has_null_key ? &map->null_value : NULL;                                                                  \
        assert(key != PHM_EMPTY_SLOT_KEY);                                                                                       \
        if (map->current_capacity == 0 || map->table == NULL)                                                                    \
            return NULL;                                                                                                         \
        Name##_kv_pair *slot = Name##_find_slot(map, key);                                                                       \
        return (slot->key == key) ? &slot->value : NULL;                                                                         \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_for_each(Name *map, void (*callback)(void *key, ValueType *value, void *user_data), void *user_data) {     \
        if (map->has_null_key)                                                                                                   \
            callback(NULL, &map->null_value, user_data);                                                                         \
        if (map->current_capacity > 0 && map->table != NULL && map->current_size > 0) {                                          \
            uint64_t count = 0;                                                                                                  \
            for (uint64_t i = 0; i < map->current_capacity; ++i) {                                                               \
                if (map->table[i].key != PHM_EMPTY_SLOT_KEY) {                                                                   \
                    callback(map->table[i].key, &map->table[i].value, user_data);                                                \
                    if (++count == map->current_size)                                                                            \
                        break;                                                                                                   \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_free(Name *map) {                                                                                          \
        if (map->table != NULL)                                                                                                  \
            free(map->table);                                                                                                    \
        Name##_init(map); /* Reset to initial empty state */                                                                     \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_move(Name *dest, Name *source) {                                                                           \
        if (dest == source)                                                                                                      \
            return;                                                                                                              \
        if (dest->table != NULL)                                                                                                 \
            free(dest->table); /* Free existing dest resources */                                                                \
        *dest = *source;       /* Copy all members, dest now owns source's table */                                              \
        Name##_init(source);   /* Reset source to prevent double free */                                                         \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_clear(Name *map) {                                                                                         \
        map->current_size = 0;                                                                                                   \
        map->has_null_key = false;                                                                                               \
        if (map->table != NULL && map->current_capacity > 0) {                                                                   \
            /* calloc already zeroed memory if PHM_EMPTY_SLOT_KEY is 0. */                                                       \
            /* If PHM_EMPTY_SLOT_KEY is not 0, or for robustness: */                                                             \
            for (uint64_t i = 0; i < map->current_capacity; ++i) {                                                               \
                map->table[i].key = PHM_EMPTY_SLOT_KEY;                                                                          \
                /* map->table[i].value = (ValueType){0}; // Optional: if values need resetting */                                \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_copy(Name *dest, const Name *source) {                                                                     \
        if (dest == source)                                                                                                      \
            return;                                                                                                              \
        if (dest->table != NULL)                                                                                                 \
            free(dest->table);                                                                                                   \
        Name##_init(dest); /* Initialize dest to a clean empty state */                                                          \
                                                                                                                                 \
        dest->has_null_key = source->has_null_key;                                                                               \
        if (source->has_null_key)                                                                                                \
            dest->null_value = source->null_value;                                                                               \
        dest->current_size = source->current_size;                                                                               \
                                                                                                                                 \
        if (source->table != NULL && source->current_capacity > 0) {                                                             \
            dest->table = (Name##_kv_pair *)calloc(source->current_capacity, sizeof(Name##_kv_pair));                            \
            if (!dest->table) {                                                                                                  \
                Name##_init(dest);                                                                                               \
                return;                                                                                                          \
            } /* Alloc fail, reset dest to safe empty */                                                                         \
            memcpy(dest->table, source->table, source->current_capacity * sizeof(Name##_kv_pair));                               \
            dest->current_capacity = source->current_capacity;                                                                   \
        }                                                                                                                        \
        /* If source had no table, dest remains in its _init state (table=NULL, capacity=0) */                                   \
    }










#define DEFINE_PTR_HASH_MAP_ERASABLE(SCOPE, Name, ValueType)                                                                     \
                                                                                                                                 \
    typedef struct Name##_kv_pair {                                                                                              \
        void *key;                                                                                                               \
        ValueType value;                                                                                                         \
    } Name##_kv_pair;                                                                                                            \
                                                                                                                                 \
    typedef struct Name {                                                                                                        \
        uint64_t current_size;                                                                                                   \
        uint64_t tombstone_count;    /* Number of deleted (tombstone) entries */                                                 \
        uint64_t current_capacity;                                                                                               \
        bool has_null_key;                                                                                                       \
        ValueType null_value;                                                                                                    \
        Name##_kv_pair* table;                                                                                                   \
    } Name;                                                                                                                      \
                                                                                                                                 \
    static inline uint64_t Name##_hash_key(void *key) {                                                                          \
        assert(key != NULL && key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY);                                         \
        return ((uintptr_t)key) * PHM_HASH_MULTIPLIER;                                                                           \
    }                                                                                                                            \
                                                                                                                                 \
    static inline uint64_t Name##_round_up_to_power_of_2(uint64_t v) {                                                           \
        if (v == 0) return 0;                                                                                                    \
        v--;                                                                                                                     \
        v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v |= v >> 32;                                            \
        v++;                                                                                                                     \
        return v;                                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_insert_entry(Name##_kv_pair *table, uint64_t capacity, void *key, ValueType value) {                      \
        assert(key != NULL && key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY && capacity > 0 && (capacity & (capacity - 1)) == 0); \
        uint64_t cap_mask = capacity - 1;                                                                                        \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        while (table[index].key != PHM_EMPTY_SLOT_KEY && table[index].key != PHM_DELETED_SLOT_KEY) {                             \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        table[index].key = key;                                                                                                  \
        table[index].value = value;                                                                                              \
    }                                                                                                                            \
                                                                                                                                 \
    static Name##_kv_pair *Name##_find_slot(Name *map, void *key) {                                                              \
        assert(key != NULL && key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY && map->table != NULL && map->current_capacity > 0); \
        uint64_t cap_mask = map->current_capacity - 1;                                                                           \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        Name##_kv_pair *tombstone = NULL;                                                                                        \
        while (map->table[index].key != PHM_EMPTY_SLOT_KEY) {                                                                    \
            if (map->table[index].key == key) return &map->table[index];                                                         \
            if (map->table[index].key == PHM_DELETED_SLOT_KEY && tombstone == NULL) tombstone = &map->table[index];               \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        return tombstone ? tombstone : &map->table[index];                                                                       \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_grow(Name *map);                                                                                          \
                                                                                                                                 \
    SCOPE void Name##_init(Name *map) {                                                                                          \
        map->current_size = 0;                                                                                                   \
        map->tombstone_count = 0;                                                                                                \
        map->current_capacity = 0;                                                                                               \
        map->has_null_key = false;                                                                                               \
        map->table = NULL;                                                                                                       \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_grow(Name *map) {                                                                                         \
        uint64_t old_capacity = map->current_capacity;                                                                           \
        Name##_kv_pair *old_table = map->table;                                                                                  \
        uint64_t new_capacity = (old_capacity == 0) ? ((PHM_INITIAL_HEAP_CAPACITY > 0) ? PHM_INITIAL_HEAP_CAPACITY : 8) : old_capacity * 2; \
        new_capacity = Name##_round_up_to_power_of_2(new_capacity);                                                              \
        if (new_capacity <= old_capacity) return;                                                                                \
        Name##_kv_pair *new_table = (Name##_kv_pair *)calloc(new_capacity, sizeof(Name##_kv_pair));                              \
        if (!new_table) return;                                                                                                  \
        if (old_table && map->current_size > 0) {                                                                                \
            for (uint64_t i = 0; i < old_capacity; ++i) {                                                                        \
                if (old_table[i].key != PHM_EMPTY_SLOT_KEY && old_table[i].key != PHM_DELETED_SLOT_KEY) {                        \
                    Name##_insert_entry(new_table, new_capacity, old_table[i].key, old_table[i].value);                          \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
        if (old_table) free(old_table);                                                                                          \
        map->table = new_table;                                                                                                  \
        map->current_capacity = new_capacity;                                                                                    \
        map->tombstone_count = 0;                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE int Name##_put(Name *map, void *key, ValueType value) {                                                                \
        if (key == NULL) {                                                                                                       \
            map->null_value = value;                                                                                             \
            if (!map->has_null_key) { map->has_null_key = true; return 1; }                                                      \
            return 0;                                                                                                            \
        }                                                                                                                        \
        assert(key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY);                                                        \
        if ((map->current_size + map->tombstone_count + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM) {\
            Name##_grow(map);                                                                                                    \
        }                                                                                                                        \
        if (map->current_capacity == 0) return 0;                                                                                \
        Name##_kv_pair *slot = Name##_find_slot(map, key);                                                                       \
        if (slot->key == PHM_EMPTY_SLOT_KEY || slot->key == PHM_DELETED_SLOT_KEY) {                                               \
            if (slot->key == PHM_DELETED_SLOT_KEY) map->tombstone_count--;                                                        \
            slot->key = key;                                                                                                     \
            slot->value = value;                                                                                                 \
            map->current_size++;                                                                                                 \
            return 1;                                                                                                            \
        } else {                                                                                                                 \
            slot->value = value;                                                                                                 \
            return 0;                                                                                                            \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE ValueType *Name##_get(Name *map, void *key) {                                                                          \
        if (key == NULL) return map->has_null_key ? &map->null_value : NULL;                                                     \
        assert(key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY);                                                        \
        if (map->current_capacity == 0) return NULL;                                                                             \
        uint64_t cap_mask = map->current_capacity - 1;                                                                           \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        while (map->table[index].key != PHM_EMPTY_SLOT_KEY) {                                                                    \
            if (map->table[index].key == key) return &map->table[index].value;                                                   \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        return NULL;                                                                                                             \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE bool Name##_erase(Name *map, void *key) {                                                                              \
        if (key == NULL) {                                                                                                       \
            if (map->has_null_key) { map->has_null_key = false; return true; }                                                   \
            return false;                                                                                                        \
        }                                                                                                                        \
        assert(key != PHM_EMPTY_SLOT_KEY && key != PHM_DELETED_SLOT_KEY);                                                        \
        if (map->current_capacity == 0) return false;                                                                            \
        uint64_t cap_mask = map->current_capacity - 1;                                                                           \
        uint64_t index = Name##_hash_key(key) & cap_mask;                                                                        \
        while (map->table[index].key != PHM_EMPTY_SLOT_KEY) {                                                                    \
            if (map->table[index].key == key) {                                                                                  \
                map->table[index].key = PHM_DELETED_SLOT_KEY;                                                                    \
                /* Optional: Zero out value for non-primitive types */                                                           \
                /* map->table[index].value = (ValueType){0}; */                                                                  \
                map->current_size--;                                                                                             \
                map->tombstone_count++;                                                                                          \
                return true;                                                                                                     \
            }                                                                                                                    \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        return false;                                                                                                            \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_free(Name *map) {                                                                                          \
        if (map->table != NULL) free(map->table);                                                                                \
        Name##_init(map);                                                                                                        \
    }













#define DEFINE_PTR_HASH_SET(SCOPE, Name, Type)                                                                                   \
                                                                                                                                 \
    typedef struct Name {                                                                                                        \
        uint64_t current_size;                                                                                                   \
        bool has_null_element;                                                                                                   \
        Type *table;                                                                                                             \
        uint64_t current_capacity;                                                                                               \
    } Name;                                                                                                                      \
                                                                                                                                 \
    static inline uint64_t Name##_hash_key(Type key) { return (uint64_t)key * PHM_HASH_MULTIPLIER; }                             \
                                                                                                                                 \
    static void Name##_grow(Name *set);                                                                                          \
                                                                                                                                 \
    SCOPE void Name##_init(Name *set) {                                                                                          \
        set->current_size = 0;                                                                                                   \
        set->has_null_element = false;                                                                                           \
        set->table = NULL;                                                                                                       \
        set->current_capacity = 0;                                                                                               \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_insert_key(Type *table, uint64_t capacity, Type key) {                                                    \
        assert(key != (Type)PHM_EMPTY_SLOT_KEY);                                                                                 \
        assert(capacity > 0 && (capacity & (capacity - 1)) == 0);                                                                \
        uint64_t cap_mask = capacity - 1;                                                                                        \
        uint64_t h = Name##_hash_key(key);                                                                                       \
        uint64_t index = h & cap_mask;                                                                                           \
        while (1) {                                                                                                              \
            Type *slot_ptr = &table[index];                                                                                      \
            if (*slot_ptr == (Type)PHM_EMPTY_SLOT_KEY) {                                                                         \
                *slot_ptr = key;                                                                                                 \
                return;                                                                                                          \
            }                                                                                                                    \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    static void Name##_grow(Name *set) {                                                                                         \
        uint64_t old_non_null_size = set->current_size;                                                                          \
        uint64_t old_capacity = set->current_capacity;                                                                           \
        Type *old_table_ptr = set->table;                                                                                        \
                                                                                                                                 \
        uint64_t new_capacity = (old_capacity < PHM_INITIAL_HEAP_CAPACITY) ? PHM_INITIAL_HEAP_CAPACITY : old_capacity * 2;       \
        if (new_capacity < PHM_INITIAL_HEAP_CAPACITY) {                                                                          \
            new_capacity = PHM_INITIAL_HEAP_CAPACITY;                                                                            \
        }                                                                                                                        \
        if (new_capacity > 0 && (new_capacity & (new_capacity - 1)) != 0) {                                                      \
            uint64_t pow2 = 1;                                                                                                   \
            while (pow2 < new_capacity)                                                                                          \
                pow2 <<= 1;                                                                                                      \
            new_capacity = pow2;                                                                                                 \
        }                                                                                                                        \
        if (new_capacity == 0)                                                                                                   \
            return;                                                                                                              \
                                                                                                                                 \
        Type *new_table = (Type *)calloc(new_capacity, sizeof(Type));                                                            \
        if (!new_table)                                                                                                          \
            return;                                                                                                              \
                                                                                                                                 \
        uint64_t count = 0;                                                                                                      \
        for (uint64_t i = 0; i < old_capacity; ++i) {                                                                            \
            Type key = old_table_ptr[i];                                                                                         \
            if (key != (Type)PHM_EMPTY_SLOT_KEY) {                                                                               \
                Name##_insert_key(new_table, new_capacity, key);                                                                 \
                count++;                                                                                                         \
                if (count == old_non_null_size)                                                                                  \
                    break;                                                                                                       \
            }                                                                                                                    \
        }                                                                                                                        \
                                                                                                                                 \
        if (old_table_ptr != NULL)                                                                                               \
            free(old_table_ptr);                                                                                                 \
                                                                                                                                 \
        set->table = new_table;                                                                                                  \
        set->current_capacity = new_capacity;                                                                                    \
        set->current_size = old_non_null_size;                                                                                   \
    }                                                                                                                            \
                                                                                                                                 \
    static inline Type *Name##_find_slot(Name *set, Type key) {                                                                  \
        assert(key != (Type)PHM_EMPTY_SLOT_KEY && set->table != NULL && set->current_capacity > 0);                              \
        uint64_t cap_mask = set->current_capacity - 1;                                                                           \
        uint64_t h = Name##_hash_key(key);                                                                                       \
        uint64_t index = h & cap_mask;                                                                                           \
        while (1) {                                                                                                              \
            Type *slot_ptr = &set->table[index];                                                                                 \
            if (*slot_ptr == (Type)PHM_EMPTY_SLOT_KEY || *slot_ptr == key) {                                                     \
                return slot_ptr;                                                                                                 \
            }                                                                                                                    \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE bool Name##_add(Name *set, Type key) {                                                                                 \
        if (key == (Type)0) {                                                                                                    \
            if (set->has_null_element) {                                                                                         \
                return false;                                                                                                    \
            } else {                                                                                                             \
                set->has_null_element = true;                                                                                    \
                return true;                                                                                                     \
            }                                                                                                                    \
        }                                                                                                                        \
                                                                                                                                 \
        bool needs_grow = (set->current_capacity == 0);                                                                          \
        if (!needs_grow && set->current_capacity > 0) {                                                                          \
            needs_grow = (set->current_size + 1) * PHM_LOAD_FACTOR_DEN >= set->current_capacity * PHM_LOAD_FACTOR_NUM;           \
        }                                                                                                                        \
        if (needs_grow) {                                                                                                        \
            Name##_grow(set);                                                                                                    \
            if (set->current_capacity == 0)                                                                                      \
                return false;                                                                                                    \
        }                                                                                                                        \
                                                                                                                                 \
        Type *slot_ptr = Name##_find_slot(set, key);                                                                             \
                                                                                                                                 \
        if (*slot_ptr == (Type)PHM_EMPTY_SLOT_KEY) {                                                                             \
            *slot_ptr = key;                                                                                                     \
            set->current_size++;                                                                                                 \
            return true;                                                                                                         \
        } else {                                                                                                                 \
            return false;                                                                                                        \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE bool Name##_contains(Name *set, Type key) {                                                                            \
        if (key == (Type)0) {                                                                                                    \
            return set->has_null_element;                                                                                        \
        }                                                                                                                        \
                                                                                                                                 \
        if (set->current_capacity == 0)                                                                                          \
            return false;                                                                                                        \
                                                                                                                                 \
        Type *slot_ptr = Name##_find_slot(set, key);                                                                             \
        return (*slot_ptr == key);                                                                                               \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_for_each(Name *set, void (*callback)(Type key, void *user_data), void *user_data) {                        \
        if (set->has_null_element) {                                                                                             \
            callback((Type)0, user_data);                                                                                        \
        }                                                                                                                        \
                                                                                                                                 \
        if (set->table != NULL && set->current_capacity > 0) {                                                                   \
            uint64_t count = 0;                                                                                                  \
            for (uint64_t i = 0; i < set->current_capacity; ++i) {                                                               \
                if (set->table[i] != (Type)PHM_EMPTY_SLOT_KEY) {                                                                 \
                    callback(set->table[i], user_data);                                                                          \
                    count++;                                                                                                     \
                    if (count == set->current_size)                                                                              \
                        break;                                                                                                   \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_free(Name *set) {                                                                                          \
        if (set->table != NULL) {                                                                                                \
            free(set->table);                                                                                                    \
        }                                                                                                                        \
        Name##_init(set);                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_move(Name *dest, Name *source) {                                                                           \
        if (dest == source) {                                                                                                    \
            return;                                                                                                              \
        }                                                                                                                        \
        dest->current_size = source->current_size;                                                                               \
        dest->has_null_element = source->has_null_element;                                                                       \
        dest->table = source->table;                                                                                             \
        dest->current_capacity = source->current_capacity;                                                                       \
        Name##_init(source);                                                                                                     \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_copy(Name *dest, const Name *source) {                                                                     \
        Name##_init(dest);                                                                                                       \
        dest->has_null_element = source->has_null_element;                                                                       \
        dest->current_size = source->current_size;                                                                               \
                                                                                                                                 \
        if (source->table != NULL && source->current_capacity > 0) {                                                             \
            dest->table = (Type *)calloc(source->current_capacity, sizeof(Type));                                                \
            if (!dest->table) {                                                                                                  \
                Name##_init(dest);                                                                                               \
                return;                                                                                                          \
            }                                                                                                                    \
            memcpy(dest->table, source->table, source->current_capacity * sizeof(Type));                                         \
            dest->current_capacity = source->current_capacity;                                                                   \
        }                                                                                                                        \
    }

#define DEFINE_VECTOR(SCOPE, Type, Name)                                                                                         \
                                                                                                                                 \
    typedef struct {                                                                                                             \
        Type *data;                                                                                                              \
        size_t size;                                                                                                             \
        size_t capacity;                                                                                                         \
    } Name;                                                                                                                      \
                                                                                                                                 \
    SCOPE void Name##_init(Name *v) {                                                                                            \
        v->data = NULL;                                                                                                          \
        v->size = 0;                                                                                                             \
        v->capacity = 0;                                                                                                         \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE bool Name##_empty(Name *v) { return v->size == 0; }                                                                    \
                                                                                                                                 \
    SCOPE void Name##_initWithSize(Name *v, size_t initialSize) {                                                                \
        if (initialSize == 0) {                                                                                                  \
            Name##_init(v);                                                                                                      \
        } else {                                                                                                                 \
            v->data = (Type *)calloc(initialSize, sizeof(Type));                                                                 \
            if (!v->data) {                                                                                                      \
                Name##_init(v);                                                                                                  \
                return;                                                                                                          \
            }                                                                                                                    \
            v->size = initialSize;                                                                                               \
            v->capacity = initialSize;                                                                                           \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_free(Name *v) {                                                                                            \
        if (v->data != NULL) {                                                                                                   \
            free(v->data);                                                                                                       \
        }                                                                                                                        \
        Name##_init(v);                                                                                                          \
    }                                                                                                                            \
    SCOPE void Name##_clear(Name *v) { v->size = 0; }                                                                            \
    SCOPE int Name##_reserve(Name *v, size_t new_capacity) {                                                                     \
        if (new_capacity <= v->capacity) {                                                                                       \
            return 0;                                                                                                            \
        }                                                                                                                        \
        Type *new_data = (Type *)realloc(v->data, new_capacity * sizeof(Type));                                                  \
        if (!new_data && new_capacity > 0) {                                                                                     \
            return -1;                                                                                                           \
        }                                                                                                                        \
        v->data = new_data;                                                                                                      \
        v->capacity = new_capacity;                                                                                              \
        return 0;                                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE int Name##_push_back(Name *v, Type value) {                                                                            \
        if (v->size >= v->capacity) {                                                                                            \
            size_t new_capacity;                                                                                                 \
            if (v->capacity == 0) {                                                                                              \
                new_capacity = 8;                                                                                                \
            } else {                                                                                                             \
                new_capacity = v->capacity * 2;                                                                                  \
            }                                                                                                                    \
            if (new_capacity < v->capacity) {                                                                                    \
                return -1;                                                                                                       \
            }                                                                                                                    \
            if (Name##_reserve(v, new_capacity) != 0) {                                                                          \
                return -1;                                                                                                       \
            }                                                                                                                    \
        }                                                                                                                        \
        v->data[v->size++] = value;                                                                                              \
        return 0;                                                                                                                \
    }                                                                                                                            \
    SCOPE void Name##_pop_back(Name *v) { --v->size; }                                                                           \
    SCOPE Type *Name##_get(Name *v, size_t index) {                                                                              \
        if (index >= v->size) {                                                                                                  \
            return NULL;                                                                                                         \
        }                                                                                                                        \
        return &v->data[index];                                                                                                  \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE size_t Name##_size(const Name *v) { return v->size; }                                                                  \
                                                                                                                                 \
    SCOPE void Name##_move(Name *dest, Name *source) {                                                                           \
        if (dest == source) {                                                                                                    \
            return;                                                                                                              \
        }                                                                                                                        \
        dest->data = source->data;                                                                                               \
        dest->size = source->size;                                                                                               \
        dest->capacity = source->capacity;                                                                                       \
        Name##_init(source);                                                                                                     \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_copy(Name *dest, const Name *source) {                                                                     \
        Name##_free(dest);                                                                                                       \
                                                                                                                                 \
        if (source->size == 0) {                                                                                                 \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        size_t capacity_to_allocate = source->capacity;                                                                          \
        if (capacity_to_allocate < source->size) {                                                                               \
            capacity_to_allocate = source->size;                                                                                 \
        }                                                                                                                        \
                                                                                                                                 \
        dest->data = (Type *)malloc(capacity_to_allocate * sizeof(Type));                                                        \
        if (!dest->data) {                                                                                                       \
            return;                                                                                                              \
        }                                                                                                                        \
        memcpy(dest->data, source->data, source->size * sizeof(Type));                                                           \
        dest->size = source->size;                                                                                               \
        dest->capacity = capacity_to_allocate;                                                                                   \
    }


#define DEFINE_VECTOR_WITH_INLINE_STORAGE(SCOPE, Type, Name, InlineCapacity)                                                     \
                                                                                                                                 \
    typedef struct {                                                                                                             \
        Type *data;                                                                                                              \
        size_t size;                                                                                                             \
        size_t capacity;                                                                                                         \
        Type inline_storage[InlineCapacity];                                                                                     \
        bool using_heap;                                                                                                         \
    } Name;                                                                                                                      \
                                                                                                                                 \
    /** @brief Initializes vector to use inline storage */                                                                      \
    SCOPE void Name##_init(Name *v) {                                                                                            \
        v->data = v->inline_storage;                                                                                             \
        v->size = 0;                                                                                                             \
        v->capacity = InlineCapacity;                                                                                            \
        v->using_heap = false;                                                                                                   \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE bool Name##_empty(Name *v) { return v->size == 0; }                                                                    \
                                                                                                                                 \
    /** @brief Initializes with specific size, switching to heap if needed */                                                   \
    SCOPE void Name##_initWithSize(Name *v, size_t initialSize) {                                                                \
        if (initialSize == 0) {                                                                                                  \
            Name##_init(v);                                                                                                      \
        } else if (initialSize <= InlineCapacity) {                                                                              \
            v->data = v->inline_storage;                                                                                         \
            v->capacity = InlineCapacity;                                                                                        \
            v->using_heap = false;                                                                                               \
            memset(v->data, 0, initialSize * sizeof(Type));                                                                     \
            v->size = initialSize;                                                                                               \
        } else {                                                                                                                 \
            v->data = (Type *)calloc(initialSize, sizeof(Type));                                                                 \
            if (!v->data) {                                                                                                      \
                Name##_init(v);                                                                                                  \
                return;                                                                                                          \
            }                                                                                                                    \
            v->size = initialSize;                                                                                               \
            v->capacity = initialSize;                                                                                           \
            v->using_heap = true;                                                                                                \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    /** @brief Frees heap memory and resets to inline storage */                                                                \
    SCOPE void Name##_free(Name *v) {                                                                                            \
        if (v->using_heap && v->data != NULL) {                                                                                  \
            free(v->data);                                                                                                       \
        }                                                                                                                        \
        Name##_init(v);                                                                                                          \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_clear(Name *v) { v->size = 0; }                                                                            \
                                                                                                                                 \
    /** @brief Reserves capacity, migrating to heap if exceeding inline limit */                                                \
    SCOPE int Name##_reserve(Name *v, size_t new_capacity) {                                                                     \
        if (new_capacity <= v->capacity) {                                                                                       \
            return 0;                                                                                                            \
        }                                                                                                                        \
                                                                                                                                 \
        if (new_capacity <= InlineCapacity) {                                                                                    \
            return 0;                                                                                                            \
        }                                                                                                                        \
                                                                                                                                 \
        Type *new_data = (Type *)malloc(new_capacity * sizeof(Type));                                                            \
        if (!new_data) {                                                                                                         \
            return -1;                                                                                                           \
        }                                                                                                                        \
                                                                                                                                 \
        if (v->size > 0) {                                                                                                       \
            memcpy(new_data, v->data, v->size * sizeof(Type));                                                                   \
        }                                                                                                                        \
                                                                                                                                 \
        if (v->using_heap) {                                                                                                     \
            free(v->data);                                                                                                       \
        }                                                                                                                        \
                                                                                                                                 \
        v->data = new_data;                                                                                                      \
        v->capacity = new_capacity;                                                                                              \
        v->using_heap = true;                                                                                                    \
        return 0;                                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    /** @brief Adds element, migrating to heap when inline storage exhausted */                                                 \
    SCOPE int Name##_push_back(Name *v, Type value) {                                                                            \
        if (v->size >= v->capacity) {                                                                                            \
            size_t new_capacity;                                                                                                 \
            if (v->capacity == 0) {                                                                                              \
                new_capacity = 8;                                                                                                \
            } else {                                                                                                             \
                new_capacity = v->capacity * 2;                                                                                  \
            }                                                                                                                    \
            if (new_capacity < v->capacity) {                                                                                    \
                return -1;                                                                                                       \
            }                                                                                                                    \
            if (Name##_reserve(v, new_capacity) != 0) {                                                                          \
                return -1;                                                                                                       \
            }                                                                                                                    \
        }                                                                                                                        \
        v->data[v->size++] = value;                                                                                              \
        return 0;                                                                                                                \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_pop_back(Name *v) { --v->size; }                                                                           \
                                                                                                                                 \
    SCOPE Type *Name##_get(Name *v, size_t index) {                                                                              \
        if (index >= v->size) {                                                                                                  \
            return NULL;                                                                                                         \
        }                                                                                                                        \
        return &v->data[index];                                                                                                  \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE size_t Name##_size(const Name *v) { return v->size; }                                                                  \
                                                                                                                                 \
    /** @brief Transfers ownership, maintaining storage type */                                                                  \
    SCOPE void Name##_move(Name *dest, Name *source) {                                                                           \
        if (dest == source) {                                                                                                    \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        Name##_free(dest);                                                                                                       \
                                                                                                                                 \
        if (source->using_heap) {                                                                                                \
            dest->data = source->data;                                                                                           \
            dest->capacity = source->capacity;                                                                                   \
            dest->using_heap = true;                                                                                             \
        } else {                                                                                                                 \
            dest->data = dest->inline_storage;                                                                                   \
            dest->capacity = InlineCapacity;                                                                                     \
            dest->using_heap = false;                                                                                            \
            if (source->size > 0) {                                                                                              \
                memcpy(dest->data, source->data, source->size * sizeof(Type));                                                   \
            }                                                                                                                    \
        }                                                                                                                        \
                                                                                                                                 \
        dest->size = source->size;                                                                                               \
        Name##_init(source);                                                                                                     \
    }                                                                                                                            \
                                                                                                                                 \
    /** @brief Creates deep copy, preserving inline optimization when possible */                                                \
    SCOPE void Name##_copy(Name *dest, const Name *source) {                                                                     \
        Name##_free(dest);                                                                                                       \
                                                                                                                                 \
        if (source->size == 0) {                                                                                                 \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        if (source->size <= InlineCapacity) {                                                                                    \
            dest->data = dest->inline_storage;                                                                                   \
            dest->capacity = InlineCapacity;                                                                                     \
            dest->using_heap = false;                                                                                            \
        } else {                                                                                                                 \
            size_t capacity_to_allocate = source->capacity;                                                                      \
            if (capacity_to_allocate < source->size) {                                                                           \
                capacity_to_allocate = source->size;                                                                             \
            }                                                                                                                    \
                                                                                                                                 \
            dest->data = (Type *)malloc(capacity_to_allocate * sizeof(Type));                                                    \
            if (!dest->data) {                                                                                                   \
                Name##_init(dest);                                                                                               \
                return;                                                                                                          \
            }                                                                                                                    \
            dest->capacity = capacity_to_allocate;                                                                               \
            dest->using_heap = true;                                                                                             \
        }                                                                                                                        \
                                                                                                                                 \
        memcpy(dest->data, source->data, source->size * sizeof(Type));                                                           \
        dest->size = source->size;                                                                                               \
    }

#define DEFINE_GENERIC_HASH_MAP(SCOPE, Name, KeyType, ValueType, KeyHashFunc, KeyCmpFunc, KeyEmptyVal)                           \
    typedef struct Name##_kv_pair {                                                                                              \
        KeyType key;                                                                                                             \
        ValueType value;                                                                                                         \
    } Name##_kv_pair;                                                                                                            \
    typedef struct Name {                                                                                                        \
        uint64_t current_size;                                                                                                   \
        uint64_t current_capacity;                                                                                               \
        KeyType empty_key_sentinel;                                                                                              \
        Name##_kv_pair *table;                                                                                                   \
    } Name;                                                                                                                      \
    static inline uint64_t Name##_hash_key_internal(KeyType key) { return KeyHashFunc(key); }                                    \
    static inline uint64_t Name##_round_up_to_power_of_2(uint64_t v) {                                                           \
        if (v == 0)                                                                                                              \
            return 0;                                                                                                            \
        v--;                                                                                                                     \
        v |= v >> 1;                                                                                                             \
        v |= v >> 2;                                                                                                             \
        v |= v >> 4;                                                                                                             \
        v |= v >> 8;                                                                                                             \
        v |= v >> 16;                                                                                                            \
        v |= v >> 32;                                                                                                            \
        v++;                                                                                                                     \
        return v;                                                                                                                \
    }                                                                                                                            \
    static void Name##_insert_entry(                                                                                             \
        Name##_kv_pair *table, uint64_t capacity, KeyType key, ValueType value, KeyType empty_key_val_param) {                   \
        assert(!KeyCmpFunc(key, empty_key_val_param) && capacity > 0 && (capacity & (capacity - 1)) == 0);                       \
        uint64_t cap_mask = capacity - 1;                                                                                        \
        uint64_t index = Name##_hash_key_internal(key) & cap_mask;                                                               \
        while (!KeyCmpFunc(table[index].key, empty_key_val_param)) {                                                             \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        table[index].key = key;                                                                                                  \
        table[index].value = value;                                                                                              \
    }                                                                                                                            \
    static Name##_kv_pair *Name##_find_slot(Name *map, KeyType key) {                                                            \
        assert(map->table != NULL && map->current_capacity > 0);                                                                 \
        uint64_t cap_mask = map->current_capacity - 1;                                                                           \
        uint64_t index = Name##_hash_key_internal(key) & cap_mask;                                                               \
        while (!KeyCmpFunc(map->table[index].key, map->empty_key_sentinel) && !KeyCmpFunc(map->table[index].key, key)) {         \
            index = (index + 1) & cap_mask;                                                                                      \
        }                                                                                                                        \
        return &map->table[index];                                                                                               \
    }                                                                                                                            \
    static void Name##_grow(Name *map);                                                                                          \
    SCOPE void Name##_init(Name *map) {                                                                                          \
        map->current_size = 0;                                                                                                   \
        map->current_capacity = 0;                                                                                               \
        map->empty_key_sentinel = (KeyEmptyVal);                                                                                 \
        map->table = NULL;                                                                                                       \
    }                                                                                                                            \
    static void Name##_grow(Name *map) {                                                                                         \
        uint64_t old_capacity = map->current_capacity;                                                                           \
        Name##_kv_pair *old_table = map->table;                                                                                  \
        uint64_t new_capacity;                                                                                                   \
        if (old_capacity == 0) {                                                                                                 \
            new_capacity = (PHM_INITIAL_HEAP_CAPACITY > 0) ? PHM_INITIAL_HEAP_CAPACITY : 8;                                      \
        } else {                                                                                                                 \
            if (old_capacity >= (UINT64_MAX / 2))                                                                                \
                new_capacity = UINT64_MAX;                                                                                       \
            else                                                                                                                 \
                new_capacity = old_capacity * 2;                                                                                 \
        }                                                                                                                        \
        new_capacity = Name##_round_up_to_power_of_2(new_capacity);                                                              \
        if (new_capacity == 0 && old_capacity == 0 && ((PHM_INITIAL_HEAP_CAPACITY > 0) ? PHM_INITIAL_HEAP_CAPACITY : 8) > 0) {   \
            new_capacity = (UINT64_C(1) << 63);                                                                                  \
        }                                                                                                                        \
        if (new_capacity == 0 || (new_capacity <= old_capacity && old_capacity > 0)) {                                           \
            return;                                                                                                              \
        }                                                                                                                        \
        Name##_kv_pair *new_table = (Name##_kv_pair *)malloc(new_capacity * sizeof(Name##_kv_pair));                             \
        if (!new_table)                                                                                                          \
            return;                                                                                                              \
        for (uint64_t i = 0; i < new_capacity; ++i) {                                                                            \
            new_table[i].key = map->empty_key_sentinel;                                                                          \
        }                                                                                                                        \
        if (old_table && map->current_size > 0) {                                                                                \
            uint64_t rehashed_count = 0;                                                                                         \
            for (uint64_t i = 0; i < old_capacity; ++i) {                                                                        \
                if (!KeyCmpFunc(old_table[i].key, map->empty_key_sentinel)) {                                                    \
                    Name##_insert_entry(new_table, new_capacity, old_table[i].key, old_table[i].value, map->empty_key_sentinel); \
                    rehashed_count++;                                                                                            \
                    if (rehashed_count == map->current_size)                                                                     \
                        break;                                                                                                   \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
        if (old_table)                                                                                                           \
            free(old_table);                                                                                                     \
        map->table = new_table;                                                                                                  \
        map->current_capacity = new_capacity;                                                                                    \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE int Name##_put(Name *map, KeyType key, ValueType value) {                                                              \
        assert(!KeyCmpFunc(key, map->empty_key_sentinel));                                                                       \
        if (map->current_capacity == 0 ||                                                                                        \
            (map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM) {                      \
            uint64_t old_cap = map->current_capacity;                                                                            \
            Name##_grow(map);                                                                                                    \
            if (map->current_capacity == old_cap && old_cap > 0) {                                                               \
                if ((map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM)                \
                    return 0;                                                                                                    \
            } else if (map->current_capacity == 0)                                                                               \
                return 0;                                                                                                        \
            else if ((map->current_size + 1) * PHM_LOAD_FACTOR_DEN >= map->current_capacity * PHM_LOAD_FACTOR_NUM)               \
                return 0;                                                                                                        \
        }                                                                                                                        \
        assert(map->current_capacity > 0 && map->table != NULL);                                                                 \
        Name##_kv_pair *slot = Name##_find_slot(map, key);                                                                       \
        if (KeyCmpFunc(slot->key, map->empty_key_sentinel)) {                                                                    \
            slot->key = key;                                                                                                     \
            slot->value = value;                                                                                                 \
            map->current_size++;                                                                                                 \
            return 1;                                                                                                            \
        } else {                                                                                                                 \
            assert(KeyCmpFunc(slot->key, key));                                                                                  \
            slot->value = value;                                                                                                 \
            return 0;                                                                                                            \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE ValueType *Name##_get(Name *map, KeyType key) {                                                                        \
        assert(!KeyCmpFunc(key, map->empty_key_sentinel));                                                                       \
        if (map->current_capacity == 0 || map->table == NULL)                                                                    \
            return NULL;                                                                                                         \
        Name##_kv_pair *slot = Name##_find_slot(map, key);                                                                       \
        return (KeyCmpFunc(slot->key, key)) ? &slot->value : NULL;                                                               \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_for_each(Name *map, void (*callback)(KeyType key, ValueType * value, void *user_data), void *user_data) {  \
        if (map->current_capacity > 0 && map->table != NULL && map->current_size > 0) {                                          \
            uint64_t count = 0;                                                                                                  \
            for (uint64_t i = 0; i < map->current_capacity; ++i) {                                                               \
                if (!KeyCmpFunc(map->table[i].key, map->empty_key_sentinel)) {                                                   \
                    callback(map->table[i].key, &map->table[i].value, user_data);                                                \
                    if (++count == map->current_size)                                                                            \
                        break;                                                                                                   \
                }                                                                                                                \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_free(Name *map) {                                                                                          \
        if (map->table != NULL)                                                                                                  \
            free(map->table);                                                                                                    \
        Name##_init(map);                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_move(Name *dest, Name *source) {                                                                           \
        if (dest == source)                                                                                                      \
            return;                                                                                                              \
        if (dest->table != NULL)                                                                                                 \
            free(dest->table);                                                                                                   \
        *dest = *source;                                                                                                         \
        Name##_init(source);                                                                                                     \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_clear(Name *map) {                                                                                         \
        map->current_size = 0;                                                                                                   \
        if (map->table != NULL && map->current_capacity > 0) {                                                                   \
            for (uint64_t i = 0; i < map->current_capacity; ++i) {                                                               \
                map->table[i].key = map->empty_key_sentinel;                                                                     \
            }                                                                                                                    \
        }                                                                                                                        \
    }                                                                                                                            \
                                                                                                                                 \
    SCOPE void Name##_copy(Name *dest, const Name *source) {                                                                     \
        if (dest == source)                                                                                                      \
            return;                                                                                                              \
        if (dest->table != NULL)                                                                                                 \
            free(dest->table);                                                                                                   \
        Name##_init(dest);                                                                                                       \
        dest->current_size = source->current_size;                                                                               \
        if (source->table != NULL && source->current_capacity > 0) {                                                             \
            dest->table = (Name##_kv_pair *)malloc(source->current_capacity * sizeof(Name##_kv_pair));                           \
            if (!dest->table) {                                                                                                  \
                Name##_init(dest);                                                                                               \
                return;                                                                                                          \
            }                                                                                                                    \
            memcpy(dest->table, source->table, source->current_capacity * sizeof(Name##_kv_pair));                               \
            dest->current_capacity = source->current_capacity;                                                                   \
        }                                                                                                                        \
    }

#endif // HASHMAP_SET_AND_VECTOR_H



// ================================
//  END PTR HASH MAP IMPLEMENTAION
// ================================

typedef struct WgvkDeviceMemoryPool WgvkDeviceMemoryPool;
typedef struct WgvkAllocator WgvkAllocator;

typedef struct wgvkAllocation {
    WgvkDeviceMemoryPool* pool;
    size_t offset;
    size_t size;
    uint32_t chunk_index;
    VkDeviceMemory memory;
} wgvkAllocation;

RGAPI VkResult wgvkAllocator_init(WgvkAllocator* allocator, VkPhysicalDevice physicalDevice, WGPUDevice device, struct VolkDeviceTable* pFunctions);
RGAPI void wgvkAllocator_destroy(WgvkAllocator* allocator);
RGAPI bool wgvkAllocator_alloc(WgvkAllocator* allocator, const VkMemoryRequirements* requirements, VkMemoryPropertyFlags propertyFlags, wgvkAllocation* out_allocation);
RGAPI void wgvkAllocator_free(const wgvkAllocation* allocation);

// =======================================================================
//  Internal Definitions
// =======================================================================

#define ALLOCATOR_GRANULARITY ((size_t)64)
#define BITS_PER_WORD         ((size_t)64)
#define MIN_CHUNK_SIZE        ((size_t)16 * 1024 * 1024)
#define OUT_OF_SPACE          ((size_t)-1)

typedef struct VirtualAllocator {
    uint64_t* level0;
    uint64_t* level1;
    uint64_t* level2;
    size_t size_in_bytes;
    size_t l0_word_count;
    size_t l1_word_count;
    size_t l2_word_count;
    size_t total_blocks;
} VirtualAllocator;

typedef struct WgvkMemoryChunk {
    VkDeviceMemory memory;
    VirtualAllocator allocator;
    void* mapped;
    uint32_t mapCount;
} WgvkMemoryChunk;

struct WgvkDeviceMemoryPool {
    WgvkMemoryChunk* chunks;
    uint32_t chunk_count;
    uint32_t chunk_capacity;
    WGPUDevice device;
    struct VolkDeviceTable* pFunctions;
    VkPhysicalDevice physicalDevice;
    uint32_t memoryTypeIndex;
};

struct WgvkAllocator {
    WgvkDeviceMemoryPool* pools;
    uint32_t pool_count;
    uint32_t pool_capacity;
    WGPUDevice device;
    struct VolkDeviceTable* pFunctions;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
};

typedef struct ImageUsageRecord{
    VkPipelineStageFlags initialStage;
    VkAccessFlags initialAccess;
    VkPipelineStageFlags lastStage;
    VkAccessFlags lastAccess;
    VkImageLayout initialLayout;
    VkImageLayout lastLayout;
    VkImageSubresourceRange initiallyAccessedSubresource;
    VkImageSubresourceRange lastAccessedSubresource;
}ImageUsageRecord;

typedef struct ImageUsageSnap{
    VkImageLayout layout;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
    VkImageSubresourceRange subresource;
}ImageUsageSnap;

typedef struct BufferUsageRecord{
    VkPipelineStageFlags initialStage;
    VkAccessFlags initialAccess;
    VkPipelineStageFlags lastStage;
    VkAccessFlags lastAccess;
    VkBool32 everWrittenTo;
}BufferUsageRecord;

typedef struct BufferUsageSnap{
    VkPipelineStageFlags stage;
    VkAccessFlags access;
}BufferUsageSnap;

typedef enum RCPassCommandType{
    rp_command_type_invalid = 0,
    rp_command_type_draw,
    rp_command_type_draw_indexed,
    rp_command_type_draw_indexed_indirect,
    rp_command_type_draw_indirect,
    rp_command_type_set_blend_constant,
    rp_command_type_set_viewport,
    rp_command_type_set_scissor_rect,
    rp_command_type_set_vertex_buffer,
    rp_command_type_set_index_buffer,
    rp_command_type_set_bind_group,
    rp_command_type_set_render_pipeline,
    rp_command_type_execute_renderbundle,
    cp_command_type_set_compute_pipeline,
    rp_command_type_set_raytracing_pipeline,
    cp_command_type_dispatch_workgroups,
    cp_command_type_dispatch_workgroups_indirect,
    rp_command_type_begin_occlusion_query,
    rp_command_type_end_occlusion_query,
    rp_command_type_insert_debug_marker,
    rp_command_type_multi_draw_indexed_indirect,
    rp_command_type_multi_draw_indirect,
    rp_command_type_enum_count,
    rt_command_type_trace_rays,
    rp_command_type_set_force32 = 0x7fffffff
}RCPassCommandType;

typedef struct RenderPassCommandDraw{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
}RenderPassCommandDraw;

typedef struct RenderPassCommandDrawIndexed{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t baseVertex;
    uint32_t firstInstance;
}RenderPassCommandDrawIndexed;

typedef struct RenderPassCommandSetBindGroup{
    uint32_t groupIndex;
    WGPUBindGroup group;
    VkPipelineBindPoint bindPoint;
    size_t dynamicOffsetCount;
    const uint32_t* dynamicOffsets;
}RenderPassCommandSetBindGroup;
typedef struct RenderPassCommandSetVertexBuffer {
    uint32_t slot;
    WGPUBuffer buffer;
    uint64_t offset;
} RenderPassCommandSetVertexBuffer;

typedef struct RenderPassCommandSetIndexBuffer {
    WGPUBuffer buffer;
    WGPUIndexFormat format;
    uint64_t offset;
    uint64_t size;
} RenderPassCommandSetIndexBuffer;

typedef struct RenderPassCommandSetPipeline {
    WGPURenderPipeline pipeline;
} RenderPassCommandSetPipeline;

typedef struct RenderPassCommandSetRaytracingPipeline{
    WGPURaytracingPipeline pipeline;
}RenderPassCommandSetRaytracingPipeline;

typedef struct RenderPassCommandExecuteRenderbundles{
    WGPURenderBundle renderBundle;
}RenderPassCommandExecuteRenderbundles;

typedef struct ComputePassCommandSetPipeline {
    WGPUComputePipeline pipeline;
} ComputePassCommandSetPipeline;

typedef struct ComputePassCommandDispatchWorkgroups {
    uint32_t x, y, z;
} ComputePassCommandDispatchWorkgroups;

typedef struct ComputePassCommandDispatchWorkgroupsIndirect {
    WGPUBuffer buffer;
    size_t offset;
}ComputePassCommandDispatchWorkgroupsIndirect;

typedef struct RenderPassCommandSetViewport{
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
}RenderPassCommandSetViewport;

typedef struct RenderPassCommandSetScissorRect{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
}RenderPassCommandSetScissorRect;

typedef struct RenderPassCommandDrawIndexedIndirect{
    WGPUBuffer indirectBuffer;
    uint64_t indirectOffset;
}RenderPassCommandDrawIndexedIndirect;

typedef struct RenderPassCommandDrawIndirect{
    WGPUBuffer indirectBuffer;
    uint64_t indirectOffset;
}RenderPassCommandDrawIndirect;

typedef struct RenderPassCommandSetBlendConstant{
    WGPUColor color;
}RenderPassCommandSetBlendConstant;


typedef struct RenderPassCommandMultiDrawIndexedIndirect {
    WGPUBuffer indirectBuffer;
    uint64_t indirectOffset;
    uint32_t maxDrawCount;
    WGPUBuffer drawCountBuffer;
    uint64_t drawCountBufferOffset;
} RenderPassCommandMultiDrawIndexedIndirect;

typedef struct RenderPassCommandBeginOcclusionQuery {
    uint32_t queryIndex;
} RenderPassCommandBeginOcclusionQuery;

typedef struct RenderPassCommandEndOcclusionQuery {
    // This command takes no arguments.
} RenderPassCommandEndOcclusionQuery;

typedef struct RenderPassCommandInsertDebugMarker {
    uint8_t length;
    char text[31];
} RenderPassCommandInsertDebugMarker;

typedef struct RenderPassCommandMultiDrawIndirect {
    WGPUBuffer indirectBuffer;
    uint64_t indirectOffset;
    uint32_t maxDrawCount;
    WGPUBuffer drawCountBuffer;
    uint64_t drawCountBufferOffset;
} RenderPassCommandMultiDrawIndirect;

typedef struct RaytracingPassCommandTraceRays{
    uint32_t rayGenerationOffset;
    uint32_t rayHitOffset;
    uint32_t rayMissOffset;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
}RaytracingPassCommandTraceRays;

typedef struct RenderPassCommandBegin{
    WGPUStringView label;
    size_t colorAttachmentCount;
    WGPURenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
    WGPUBool depthAttachmentPresent;
    WGPURenderPassDepthStencilAttachment depthStencilAttachment;
    WGPUQuerySet occlusionQuerySet;
    WGPUBool timestampWritesPresent;
    WGPUPassTimestampWrites timestampWrites;
}RenderPassCommandBegin;

typedef struct RenderPassCommandGeneric {
    RCPassCommandType type;
    union {
        RenderPassCommandDraw draw;
        RenderPassCommandDrawIndexed drawIndexed;
        RenderPassCommandSetViewport setViewport;
        RenderPassCommandSetScissorRect setScissorRect;
        RenderPassCommandDrawIndexedIndirect drawIndexedIndirect;
        RenderPassCommandDrawIndirect drawIndirect;
        RenderPassCommandSetBlendConstant setBlendConstant;
        RenderPassCommandSetVertexBuffer setVertexBuffer;
        RenderPassCommandSetIndexBuffer setIndexBuffer;
        RenderPassCommandSetBindGroup setBindGroup;
        RenderPassCommandSetPipeline setRenderPipeline;
        RenderPassCommandSetRaytracingPipeline setRaytracingPipeline;
        RenderPassCommandExecuteRenderbundles executeRenderBundles;
        RenderPassCommandMultiDrawIndexedIndirect multiDrawIndexedIndirect;
        RenderPassCommandBeginOcclusionQuery beginOcclusionQuery;
        RenderPassCommandEndOcclusionQuery endOcclusionQuery;
        RenderPassCommandInsertDebugMarker insertDebugMarker;
        RenderPassCommandMultiDrawIndirect multiDrawIndirect;
        ComputePassCommandSetPipeline setComputePipeline;
        ComputePassCommandDispatchWorkgroups dispatchWorkgroups;
        ComputePassCommandDispatchWorkgroupsIndirect dispatchWorkgroupsIndirect;
        RaytracingPassCommandTraceRays traceRays;
    };
}RenderPassCommandGeneric;

#define CONTAINERAPI static inline
DEFINE_PTR_HASH_MAP (CONTAINERAPI, BufferUsageRecordMap, BufferUsageRecord)
//DEFINE_PTR_HASH_MAP (CONTAINERAPI, ImageViewUsageRecordMap, ImageViewUsageRecord)
//DEFINE_PTR_HASH_MAP (CONTAINERAPI, LayoutAssumptions, ImageLayoutPair)
DEFINE_PTR_HASH_MAP (CONTAINERAPI, ImageUsageRecordMap, ImageUsageRecord)
DEFINE_PTR_HASH_SET (CONTAINERAPI, BindGroupUsageSet, WGPUBindGroup)
DEFINE_PTR_HASH_SET (CONTAINERAPI, BindGroupLayoutUsageSet, WGPUBindGroupLayout)
DEFINE_PTR_HASH_SET (CONTAINERAPI, SamplerUsageSet, WGPUSampler)
DEFINE_PTR_HASH_SET (CONTAINERAPI, ImageViewUsageSet, WGPUTextureView)
DEFINE_PTR_HASH_SET (CONTAINERAPI, WGPURenderPassEncoderSet, WGPURenderPassEncoder)
DEFINE_PTR_HASH_SET (CONTAINERAPI, RenderPipelineUsageSet, WGPURenderPipeline)
DEFINE_PTR_HASH_SET (CONTAINERAPI, ComputePipelineUsageSet, WGPUComputePipeline)
DEFINE_PTR_HASH_SET (CONTAINERAPI, RaytracingPipelineUsageSet, WGPURaytracingPipeline)
DEFINE_PTR_HASH_SET (CONTAINERAPI, RenderBundleUsageSet, WGPURenderBundle)
DEFINE_PTR_HASH_SET (CONTAINERAPI, QuerySetUsageSet, WGPUQuerySet)
DEFINE_PTR_HASH_SET (CONTAINERAPI, WGPUComputePassEncoderSet, WGPUComputePassEncoder)
DEFINE_PTR_HASH_SET (CONTAINERAPI, WGPURaytracingPassEncoderSet, WGPURaytracingPassEncoder)

DEFINE_VECTOR (static inline, VkDynamicState, VkDynamicStateVector)
DEFINE_VECTOR (CONTAINERAPI, VkWriteDescriptorSet, VkWriteDescriptorSetVector)
DEFINE_VECTOR (CONTAINERAPI, WGPUFence, WGPUFenceVector)
DEFINE_VECTOR (CONTAINERAPI, VkFence, VkFenceVector)
DEFINE_VECTOR (CONTAINERAPI, VkCommandBuffer, VkCommandBufferVector)
DEFINE_VECTOR (CONTAINERAPI, RenderPassCommandGeneric, RenderPassCommandGenericVector)
DEFINE_VECTOR (CONTAINERAPI, VkSemaphore, VkSemaphoreVector)
DEFINE_VECTOR (CONTAINERAPI, WGPUCommandBuffer, WGPUCommandBufferVector)
DEFINE_VECTOR (CONTAINERAPI, WGPUCommandEncoder, WGPUCommandEncoderVector)
DEFINE_VECTOR (CONTAINERAPI, VkDescriptorBufferInfo, VkDescriptorBufferInfoVector)
DEFINE_VECTOR (CONTAINERAPI, VkDescriptorImageInfo, VkDescriptorImageInfoVector)
DEFINE_VECTOR (CONTAINERAPI, VkWriteDescriptorSetAccelerationStructureKHR, VkWriteDescriptorSetAccelerationStructureKHRVector)
DEFINE_VECTOR (CONTAINERAPI, VkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBindingVector)

DEFINE_PTR_HASH_MAP(CONTAINERAPI, PendingCommandBufferMap, WGPUCommandBufferVector);

typedef struct DescriptorSetAndPool{
    VkDescriptorPool pool;
    VkDescriptorSet set;
}DescriptorSetAndPool;

DEFINE_VECTOR(static inline, VkAttachmentDescription, VkAttachmentDescriptionVector)
DEFINE_VECTOR(static inline, WGPUBuffer, WGPUBufferVector)
DEFINE_VECTOR(static inline, DescriptorSetAndPool, DescriptorSetAndPoolVector)
DEFINE_PTR_HASH_MAP_ERASABLE(static inline, BindGroupCacheMap, DescriptorSetAndPoolVector)


//DEFINE_PTR_HASH_MAP(static inline, BindGroupUsageMap, uint32_t)
//DEFINE_PTR_HASH_MAP(static inline, SamplerUsageMap, uint32_t)

typedef uint32_t refcount_type;

typedef struct ResourceUsage{
    BufferUsageRecordMap referencedBuffers;
    ImageUsageRecordMap referencedTextures;
    ImageViewUsageSet referencedTextureViews;
    BindGroupUsageSet referencedBindGroups;
    BindGroupLayoutUsageSet referencedBindGroupLayouts;
    SamplerUsageSet referencedSamplers;
    RenderPipelineUsageSet referencedRenderPipelines;
    ComputePipelineUsageSet referencedComputePipelines;
    RaytracingPipelineUsageSet referencedRaytracingPipelines;
    RenderBundleUsageSet referencedRenderBundles;
    QuerySetUsageSet referencedQuerySets;
    //LayoutAssumptions entryAndFinalLayouts;
}ResourceUsage;

static inline void ResourceUsage_free(ResourceUsage* ru){
    BufferUsageRecordMap_free(&ru->referencedBuffers);
    ImageUsageRecordMap_free(&ru->referencedTextures);
    ImageViewUsageSet_free(&ru->referencedTextureViews);
    BindGroupUsageSet_free(&ru->referencedBindGroups);
    BindGroupLayoutUsageSet_free(&ru->referencedBindGroupLayouts);
    SamplerUsageSet_free(&ru->referencedSamplers);
    QuerySetUsageSet_free(&ru->referencedQuerySets);
    RenderBundleUsageSet_free(&ru->referencedRenderBundles);
}

static inline void ResourceUsage_move(ResourceUsage* dest, ResourceUsage* source){
    BufferUsageRecordMap_move(&dest->referencedBuffers, &source->referencedBuffers);
    ImageUsageRecordMap_move(&dest->referencedTextures, &source->referencedTextures);
    ImageViewUsageSet_move(&dest->referencedTextureViews, &source->referencedTextureViews);
    BindGroupUsageSet_move(&dest->referencedBindGroups, &source->referencedBindGroups);
    BindGroupLayoutUsageSet_move(&dest->referencedBindGroupLayouts, &source->referencedBindGroupLayouts);
    SamplerUsageSet_move(&dest->referencedSamplers, &source->referencedSamplers);
    QuerySetUsageSet_free(&dest->referencedQuerySets);
    RenderBundleUsageSet_free(&dest->referencedRenderBundles);
    //LayoutAssumptions_move(&dest->entryAndFinalLayouts, &source->entryAndFinalLayouts);
}

static inline void ResourceUsage_init(ResourceUsage* ru){
    BufferUsageRecordMap_init(&ru->referencedBuffers);
    ImageUsageRecordMap_init(&ru->referencedTextures);
    ImageViewUsageSet_init(&ru->referencedTextureViews);
    BindGroupUsageSet_init(&ru->referencedBindGroups);
    BindGroupLayoutUsageSet_init(&ru->referencedBindGroupLayouts);
    SamplerUsageSet_init(&ru->referencedSamplers);
    RenderBundleUsageSet_init(&ru->referencedRenderBundles);
    QuerySetUsageSet_init(&ru->referencedQuerySets);
    //LayoutAssumptions_init(&ru->entryAndFinalLayouts);
}

RGAPI void ru_registerTransition   (ResourceUsage* resourceUsage, WGPUTexture tex, VkImageLayout from, VkImageLayout to);
RGAPI void ru_trackBuffer          (ResourceUsage* resourceUsage, WGPUBuffer buffer, BufferUsageRecord brecord);
RGAPI void ru_trackTexture         (ResourceUsage* resourceUsage, WGPUTexture texture, ImageUsageRecord newRecord);
RGAPI void ru_trackTextureView     (ResourceUsage* resourceUsage, WGPUTextureView view);
RGAPI void ru_trackBindGroup       (ResourceUsage* resourceUsage, WGPUBindGroup bindGroup);
RGAPI void ru_trackBindGroupLayout (ResourceUsage* resourceUsage, WGPUBindGroupLayout bindGroupLayout);
RGAPI void ru_trackSampler         (ResourceUsage* resourceUsage, WGPUSampler sampler);
RGAPI void ru_trackRenderPipeline  (ResourceUsage* resourceUsage, WGPURenderPipeline rpl);
RGAPI void ru_trackComputePipeline (ResourceUsage* resourceUsage, WGPUComputePipeline computePipeline);
RGAPI void ru_trackQuerySet        (ResourceUsage* resourceUsage, WGPUQuerySet computePipeline);
RGAPI void ru_trackRenderBundle    (ResourceUsage* resourceUsage, WGPURenderBundle computePipeline);

RGAPI void ce_trackBuffer(WGPUCommandEncoder encoder, WGPUBuffer buffer, BufferUsageSnap usage);
RGAPI void ce_trackTexture(WGPUCommandEncoder encoder, WGPUTexture texture, ImageUsageSnap usage);
RGAPI void ce_trackTextureView(WGPUCommandEncoder encoder, WGPUTextureView view, ImageUsageSnap usage);

RGAPI Bool32 ru_containsBuffer         (const ResourceUsage* resourceUsage, WGPUBuffer buffer);
RGAPI Bool32 ru_containsTexture        (const ResourceUsage* resourceUsage, WGPUTexture texture);
RGAPI Bool32 ru_containsTextureView    (const ResourceUsage* resourceUsage, WGPUTextureView view);
RGAPI Bool32 ru_containsBindGroup      (const ResourceUsage* resourceUsage, WGPUBindGroup bindGroup);
RGAPI Bool32 ru_containsBindGroupLayout(const ResourceUsage* resourceUsage, WGPUBindGroupLayout bindGroupLayout);
RGAPI Bool32 ru_containsSampler        (const ResourceUsage* resourceUsage, WGPUSampler bindGroup);

RGAPI void releaseAllAndClear(ResourceUsage* resourceUsage);

typedef struct SyncState{
    VkSemaphoreVector semaphores;
    VkSemaphore acquireImageSemaphore;
    bool acquireImageSemaphoreSignalled;
    uint32_t submits;
    //VkFence renderFinishedFence;    
}SyncState;
typedef struct WGPUString{
    char* data;
    size_t length;
}WGPUString;

static inline WGPUString WGPUStringFromView(WGPUStringView view){
    size_t length = view.length == WGPU_STRLEN ? strlen(view.data) : view.length;
    if(length == 0){
        return (WGPUString){0};
    }
    WGPUString ret = {
        .data = (char*)RL_CALLOC(length, 1),
        .length = length
    };
    memcpy(ret.data, view.data, length);
    return ret;
}

static inline void WGPUStringFree(WGPUString string){
    RL_FREE(string.data);
}

typedef struct MappableBufferMemory{
    VkBuffer buffer;
    VmaAllocation allocation;
    VkMemoryPropertyFlags propertyFlags;
    size_t capacity;
}MappableBufferMemory;

#ifdef __cplusplus
constexpr uint32_t max_color_attachments = MAX_COLOR_ATTACHMENTS;
#else
#define max_color_attachments MAX_COLOR_ATTACHMENTS
#endif
typedef struct AttachmentDescriptor{
    VkFormat format;
    uint32_t sampleCount;
    VkAttachmentLoadOp loadop;
    VkAttachmentStoreOp storeop;
#ifdef __cplusplus
    bool operator==(const AttachmentDescriptor& other)const noexcept{
        return format == other.format
        && sampleCount == other.sampleCount
        && loadop == other.loadop
        && storeop == other.storeop;
    }
    bool operator!=(const AttachmentDescriptor& other) const noexcept{
        return !(*this == other);
    }
#endif
}AttachmentDescriptor;
static bool attachmentDescriptorCompare(AttachmentDescriptor a, AttachmentDescriptor b){
    return a.format      == b.format
        && a.sampleCount == b.sampleCount
        && a.loadop      == b.loadop
        && a.storeop     == b.storeop;
}


typedef struct RenderPassLayout{
    uint32_t colorAttachmentCount;
    AttachmentDescriptor colorAttachments [4];
    AttachmentDescriptor colorResolveAttachments [4];
    uint32_t depthAttachmentPresent;
    AttachmentDescriptor depthAttachment;
    //uint32_t colorResolveIndex;
#ifdef __cplusplus
    bool operator==(const RenderPassLayout& other) const noexcept {
        if(colorAttachmentCount != other.colorAttachmentCount)return false;
        for(uint32_t i = 0;i < colorAttachmentCount;i++){
            if(colorAttachments[i] != other.colorAttachments[i]){
                return false;
            }
        }
        if(depthAttachmentPresent != other.depthAttachmentPresent)return false;
        if(depthAttachment != other.depthAttachment)return false;
        //if(colorResolveIndex != other.colorResolveIndex)return false;

        return true;
    }
#endif
}RenderPassLayout;

static bool renderPassLayoutCompare(RenderPassLayout first, RenderPassLayout other){
    if(first.colorAttachmentCount != other.colorAttachmentCount)return false;
    for(uint32_t i = 0;i < first.colorAttachmentCount;i++){
        if(!attachmentDescriptorCompare(first.colorAttachments[i], other.colorAttachments[i])){
            return false;
        }
    }
    return (first.depthAttachmentPresent == other.depthAttachmentPresent) && attachmentDescriptorCompare(first.depthAttachment, other.depthAttachment);
}

typedef struct LayoutedRenderPass{
    RenderPassLayout layout;
    VkAttachmentDescriptionVector allAttachments;
    VkRenderPass renderPass;
}LayoutedRenderPass;
#ifdef __cplusplus
extern "C"{
#endif
LayoutedRenderPass LoadRenderPassFromLayout(WGPUDevice device, RenderPassLayout layout);
RenderPassLayout GetRenderPassLayout(const WGPURenderPassDescriptor* rpdesc);
#ifdef __cplusplus
}
#endif
typedef struct wgpuxorshiftstate{
    uint64_t x64;
    #ifdef __cplusplus
    constexpr void update(uint64_t x) noexcept{
        x64 ^= x * 0x2545F4914F6CDD1D;
        x64 ^= x64 << 13;
        x64 ^= x64 >> 7;
        x64 ^= x64 << 17;
    }
    constexpr void update(uint32_t x, uint32_t y)noexcept{
        x64 ^= ((uint64_t(x) << 32) | uint64_t(y)) * 0x2545F4914F6CDD1D;
        x64 ^= x64 << 13;
        x64 ^= x64 >> 7;
        x64 ^= x64 << 17;
    }
    #endif
}wgpuxorshiftstate;
static inline void xs_update_u32(wgpuxorshiftstate* state, uint32_t x, uint32_t y){
    state->x64 ^= ((((uint64_t)x) << 32) | ((uint64_t)y)) * 0x2545F4914F6CDD1D;
    state->x64 ^= state->x64 << 13;
    state->x64 ^= state->x64 >> 7;
    state->x64 ^= state->x64 << 17;

}

static size_t renderPassLayoutHash(RenderPassLayout layout){
    wgpuxorshiftstate ret = {.x64 = 0x2545F4918F6CDD1D};
    xs_update_u32(&ret, layout.depthAttachmentPresent << 6, layout.colorAttachmentCount);
    for(uint32_t i = 0;i < layout.colorAttachmentCount;i++){
        xs_update_u32(&ret, layout.colorAttachments[i].format, layout.colorAttachments[i].sampleCount);
        xs_update_u32(&ret, layout.colorAttachments[i].loadop, layout.colorAttachments[i].storeop);
    }
    xs_update_u32(&ret, layout.depthAttachmentPresent << 6, layout.depthAttachmentPresent);
    if(layout.depthAttachmentPresent){
        xs_update_u32(&ret, layout.depthAttachment.format, layout.depthAttachment.sampleCount);
        xs_update_u32(&ret, layout.depthAttachment.loadop, layout.depthAttachment.storeop);
    }
    return ret.x64;
}


typedef struct PerframeCache{
    VkCommandPool commandPool;
    VkCommandBufferVector commandBuffers;
    VkCommandBufferVector secondaryCommandBuffers;

    WGPUBufferVector unusedBatchBuffers;
    WGPUBufferVector usedBatchBuffers;
    
    VkCommandBuffer finalTransitionBuffer;
    VkSemaphore finalTransitionSemaphore;
    WGPUFence finalTransitionFence;
    //std::map<uint64_t, small_vector<MappableBufferMemory>> stagingBufferCache;
    //std::unordered_map<WGPUBindGroupLayout, std::vector<std::pair<VkDescriptorPool, VkDescriptorSet>>> bindGroupCache;
    BindGroupCacheMap bindGroupCache;
    VkFenceVector reusableFences;
}PerframeCache;

typedef struct QueueIndices{
    uint32_t graphicsIndex;
    uint32_t computeIndex;
    uint32_t transferIndex;
    uint32_t presentIndex;
}QueueIndices;

typedef struct WGPUSamplerImpl{
    VkSampler sampler;
    refcount_type refCount;
    WGPUDevice device;
}WGPUSamplerImpl;

typedef struct CallbackWithUserdata{
    void(*callback)(void*);
    void* userdata;
    void(*freeUserData)(void*);
}CallbackWithUserdata;

DEFINE_VECTOR(static inline, CallbackWithUserdata, CallbackWithUserdataVector);

typedef enum WGPUFenceState{
    WGPUFenceState_Reset,
    WGPUFenceState_InUse,
    WGPUFenceState_Finished,
    WGPUFenceState_Force32 = 0x7FFFFFFF,
}WGPUFenceState;
typedef struct WGPUFenceImpl{
    VkFence fence;
    WGPUFenceState state;
    WGPUDevice device;
    refcount_type refCount;
    CallbackWithUserdataVector callbacksOnWaitComplete;
}WGPUFenceImpl;

typedef struct PendingCommandBufferListRef{
    WGPUFence fence;
    PendingCommandBufferMap* map;
}PendingCommandBufferListRef;

typedef struct WGPUBindGroupImpl{
    VkDescriptorSet set;
    VkDescriptorPool pool;
    WGPUBindGroupLayout layout;
    refcount_type refCount;
    ResourceUsage resourceUsage;
    WGPUDevice device;
    uint32_t cacheIndex;
    WGPUBindGroupEntry* entries;
    uint32_t entryCount;
}WGPUBindGroupImpl;

typedef struct WGPUBindGroupLayoutImpl{
    VkDescriptorSetLayout layout;
    WGPUDevice device;
    WGPUBindGroupLayoutEntry* entries;
    uint32_t entryCount;

    refcount_type refCount;
}WGPUBindGroupLayoutImpl;

typedef struct WGPUPipelineLayoutImpl{
    VkPipelineLayout layout;
    WGPUDevice device;
    WGPUBindGroupLayout* bindGroupLayouts;
    uint32_t bindGroupLayoutCount;
    refcount_type refCount;
}WGPUPipelineLayoutImpl;
typedef enum AllocationType{
    AllocationTypeUndefined = 0,
    AllocationTypeVMA = 1,
    AllocationTypeBuiltin = 2,
    AllocationTypeJustMemory = 3,
    AllocationTypeForce32 = 0x7fffffff,
}AllocationType;
typedef struct WGPUBufferImpl{
    VkBuffer buffer;
    WGPUDevice device;
    uint32_t cacheIndex;
    WGPUBufferUsage usage;
    WGPUBufferMapState mapState;
    void* mappedRange;
    size_t capacity;
    AllocationType allocationType;
    union{
        VmaAllocation vmaAllocation;
        wgvkAllocation builtinAllocation;
        VkDeviceMemory justMemory;
    };
    VkMemoryPropertyFlags memoryProperties;
    VkDeviceAddress address; //uint64_t, if applicable (BufferUsage_ShaderDeviceAddress)
    refcount_type refCount;
    WGPUFence latestFence;
}WGPUBufferImpl;

typedef struct WGPURayTracingShaderBindingTableImpl{
    uint32_t refCount;
    uint32_t shaderGroupCount;
    uint32_t shaderStageCount;
    VkRayTracingShaderGroupCreateInfoKHR* shaderGroups;
    VkPipelineShaderStageCreateInfo* shaderStages;
    WGPUDevice device;
}WGPURayTracingShaderBindingTableImpl;

typedef struct WGPURayTracingAccelerationContainerImpl{
    VkAccelerationStructureKHR accelerationStructure;
    WGPUDevice device;
    uint32_t geometryCount;
    WGPUBuffer* inputGeometryBuffers;
    VkAccelerationStructureGeometryKHR* geometries;
    VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos;
    uint32_t* primitiveCounts;
    WGPUBuffer accelerationStructureBuffer;
    WGPUBuffer updateScratchBuffer;
    WGPUBuffer buildScratchBuffer;
    WGPURayTracingAccelerationContainerLevel level;
}WGPURayTracingAccelerationContainerImpl;

/*typedef struct WGPUBottomLevelAccelerationStructureImpl {
    WGPUDevice device;
    VkAccelerationStructureKHR accelerationStructure;
    
    WGPUBuffer scratchBuffer;
    WGPUBuffer accelerationStructureBuffer;
} WGPUBottomLevelAccelerationStructureImpl;
typedef WGPUBottomLevelAccelerationStructureImpl *WGPUBottomLevelAccelerationStructure;

typedef struct WGPUTopLevelAccelerationStructureImpl {
    WGPUDevice device;
    VkAccelerationStructureKHR accelerationStructure;
    WGPUBuffer scratchBuffer;
    WGPUBuffer accelerationStructureBuffer;
    WGPUBuffer instancesBuffer;
} WGPUTopLevelAccelerationStructureImpl;*/

static inline uint64_t identity_sdf(uint64_t x){
    return x;
}
static inline int comparison_sdf(uint64_t x, uint64_t y){
    return x == y;
}

typedef struct WGPUFutureImpl{
    void* userdataForFunction;
    void (*functionCalledOnWaitAny)(void*);
    void (*freeUserData)(void*);
}WGPUFutureImpl;

DEFINE_GENERIC_HASH_MAP(CONTAINERAPI, RenderPassCache, RenderPassLayout, LayoutedRenderPass, renderPassLayoutHash, renderPassLayoutCompare, CLITERAL(RenderPassLayout){0});
DEFINE_GENERIC_HASH_MAP(static inline, FutureIDMap, uint64_t, WGPUFutureImpl, identity_sdf, comparison_sdf, 0);


typedef struct WGPUInstanceImpl{
    VkInstance instance;
    refcount_type refCount;
    VkDebugUtilsMessengerEXT debugMessenger;

    uint64_t currentFutureId;
    FutureIDMap g_futureIDMap;
}WGPUInstanceImpl;



typedef struct WGPUAdapterImpl{
    VkPhysicalDevice physicalDevice;
    refcount_type refCount;
    char cachedDeviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    WGPUInstance instance;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
    VkPhysicalDeviceMemoryProperties memProperties;
    QueueIndices queueIndices;
}WGPUAdapterImpl;

#define framesInFlight 2
typedef struct FenceCache{
    WGPUDevice device;
    VkFenceVector cachedFences;
}FenceCache;

typedef struct WGVKCapabilities{
    WGPUBool raytracing;
    WGPUBool shaderDeviceAddress;
    WGPUBool dynamicRendering;
}WGVKCapabilities;

typedef struct WGPUDeviceImpl{
    VkDevice device;
    refcount_type refCount;
    WGPUAdapter adapter;
    WGPUQueue queue;
    size_t submittedFrames;
    WGVKCapabilities capabilities;
    WgvkAllocator builtinAllocator;
    #if USE_VMA_ALLOCATOR == 1
    VmaAllocator allocator;
    #endif

    VmaPool aligned_hostVisiblePool;
    PerframeCache frameCaches[framesInFlight];
    VkCommandPool secondaryCommandPool;
    RenderPassCache renderPassCache;
    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo;
    FenceCache fenceCache;
    wgvk_thread_pool_t thread_pool;
    struct VolkDeviceTable functions;
}WGPUDeviceImpl;

static inline void FenceCache_Init(WGPUDevice device, FenceCache* ptr){
    ptr->device = device;
    VkFenceVector_init(&ptr->cachedFences);
}
static inline VkFence FenceCache_GetFence(FenceCache* ptr){
    if(ptr->cachedFences.size == 0){
        VkFence ret = NULL;
        VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkResult result = ptr->device->functions.vkCreateFence(
            ptr->device->device, 
            &createInfo,
            NULL,
            &ret
        );
        if(result != VK_SUCCESS)abort();
        //rassert(result == VK_SUCCESS, "Could not create fence");
        return ret;
    }
    else{
        VkFence ret = ptr->cachedFences.data[ptr->cachedFences.size - 1];
        VkFenceVector_pop_back(&ptr->cachedFences);
        return ret;
    }
}
static inline void FenceCache_PutFence(FenceCache* ptr, VkFence fence){
    VkFenceVector_push_back(&ptr->cachedFences, fence);
}
static inline void FenceCache_Destroy(FenceCache* ptr){
    for(size_t i = 0;i < ptr->cachedFences.size;i++){
        ptr->device->functions.vkDestroyFence(
            ptr->device->device,
            ptr->cachedFences.data[i],
            NULL
        );
    }
    VkFenceVector_free(&ptr->cachedFences);
}
typedef uint8_t SlimComponentSwizzle;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_IDENTITY = VK_COMPONENT_SWIZZLE_IDENTITY;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_ZERO = VK_COMPONENT_SWIZZLE_ZERO;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_ONE = VK_COMPONENT_SWIZZLE_ONE;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_R = VK_COMPONENT_SWIZZLE_R;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_G = VK_COMPONENT_SWIZZLE_G;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_B = VK_COMPONENT_SWIZZLE_B;
const static SlimComponentSwizzle SLIM_COMPONENT_SWIZZLE_A = VK_COMPONENT_SWIZZLE_A;

typedef struct SlimComponentMapping{
    SlimComponentSwizzle r;
    SlimComponentSwizzle g;
    SlimComponentSwizzle b;
    SlimComponentSwizzle a;
}SlimComponentMapping;

typedef struct SlimViewCreateInfo{
    VkFormat format;
    VkImageSubresourceRange subresourceRange;
    SlimComponentMapping cmap;
    VkImageViewType viewType;
}SlimViewCreateInfo;

static inline bool Compare_FormatAspectAndSR(SlimViewCreateInfo obj, SlimViewCreateInfo obj2){
    return obj.format == obj2.format &&
           obj.subresourceRange.baseMipLevel   == obj2.subresourceRange.baseMipLevel &&
           obj.subresourceRange.levelCount     == obj2.subresourceRange.levelCount &&
           obj.subresourceRange.baseArrayLayer == obj2.subresourceRange.baseArrayLayer &&
           obj.subresourceRange.layerCount     == obj2.subresourceRange.layerCount &&
           obj.subresourceRange.aspectMask     == obj2.subresourceRange.aspectMask &&
           obj.viewType     == obj2.viewType &&
           obj.cmap.r     == obj2.cmap.r &&
           obj.cmap.g     == obj2.cmap.g &&
           obj.cmap.b     == obj2.cmap.b &&
           obj.cmap.a     == obj2.cmap.a;
}

#define FNV_oFFSET_BASIS 14695981039346656037ULL
#define FNV_pRIME 1099511628211ULL

static inline size_t Hash_FormatAspectAndSR(SlimViewCreateInfo obj){
    size_t hash = FNV_oFFSET_BASIS;

    // Hash the 'format' field
    hash ^= (size_t)obj.format;
    hash *= FNV_pRIME;

    // Hash VkImageSubresourceRange members individually
    hash ^= (size_t)obj.subresourceRange.baseMipLevel;
    hash *= FNV_pRIME;

    hash ^= (size_t)obj.subresourceRange.levelCount;
    hash *= FNV_pRIME;

    hash ^= (size_t)obj.subresourceRange.baseArrayLayer;
    hash *= FNV_pRIME;

    hash ^= (size_t)obj.subresourceRange.layerCount;
    hash *= FNV_pRIME;

    hash ^= (size_t)obj.subresourceRange.aspectMask; // Add aspectMask, it's in the comparison but was missing from hash
    hash *= FNV_pRIME;

    hash ^= (size_t)obj.viewType;
    hash *= FNV_pRIME;

    // Hash SlimComponentMapping members portably and efficiently.
    // Combine the four uint8_t into a single uint32_t.
    // This is explicitly defined and consistent across different endian systems.
    uint32_t cmap_combined = (((uint32_t)obj.cmap.r)) |
                             (((uint32_t)obj.cmap.g) << 8) |
                             (((uint32_t)obj.cmap.b) << 16) |
                             (((uint32_t)obj.cmap.a) << 24);
    hash ^= (size_t)cmap_combined;
    hash *= FNV_pRIME;

    return hash;
}

DEFINE_GENERIC_HASH_MAP(static inline, Texture_ViewCache, SlimViewCreateInfo, WGPUTextureView, Hash_FormatAspectAndSR, Compare_FormatAspectAndSR, (SlimViewCreateInfo){VK_FORMAT_UNDEFINED});
typedef struct WGPUTextureImpl{
    VkImage image;
    VkFormat format;
    VkImageUsageFlags usage;
    VkImageLayout layout;
    VkImageType dimension;
    VkDeviceMemory memory;
    WGPUDevice device;
    refcount_type refCount;
    uint32_t width, height, depthOrArrayLayers;
    uint32_t mipLevels;
    uint32_t sampleCount;
    Texture_ViewCache viewCache;
}WGPUTextureImpl;

typedef struct WGPUShaderModuleSingleEntryPoint{
    char epName[16];
    VkShaderModule module;
}WGPUShaderModuleSingleEntryPoint;

typedef struct WGPUShaderModuleImpl{
    refcount_type refCount;
    WGPUDevice device;
    VkShaderModule vulkanModuleMultiEP;
    WGPUShaderModuleSingleEntryPoint modules[16];
    WGPUChainedStruct* source;
}WGPUShaderModuleImpl;

typedef struct WGPURenderPipelineImpl{
    VkPipeline renderPipeline;
    WGPUPipelineLayout layout;
    refcount_type refCount;
    WGPUDevice device;
    VkDynamicStateVector dynamicStates;
}WGPURenderPipelineImpl;

typedef struct WGPUComputePipelineImpl{
    VkPipeline computePipeline;
    WGPUDevice device;
    WGPUPipelineLayout layout;
    refcount_type refCount;
}WGPUComputePipelineImpl;

typedef struct WGPURaytracingPipelineImpl{
    VkPipeline raytracingPipeline;
    uint32_t refCount;
    WGPUPipelineLayout layout;
    WGPUBuffer sbtBuffer;
    VkDeviceSize totalSbtSize;
}WGPURaytracingPipelineImpl;

typedef struct WGPUTextureViewImpl{
    VkImageView view;
    VkFormat format;
    refcount_type refCount;
    WGPUTexture texture;
    VkImageSubresourceRange subresourceRange;
    uint32_t width, height, depthOrArrayLayers;
    uint32_t sampleCount;
}WGPUTextureViewImpl;

typedef struct DefaultDynamicState{
    VkViewport viewport;
    VkRect2D scissorRect;
    uint32_t stencilReference;
    float blendConstants[4];
}DefaultDynamicState;

typedef struct CommandBufferAndSomeState{
    WGPUCommandEncoder cmdEncoder;
    VkCommandBuffer buffer;
    VkPipelineLayout lastLayout;
    WGPUDevice device;
    WGPUBuffer vertexBuffers[8];
    WGPUBuffer indexBuffer;
    WGPUBindGroup graphicsBindGroups[8];
    WGPUBindGroup computeBindGroups[8];
    WGPURaytracingPipeline lastRaytracingPipeline;
    DefaultDynamicState dynamicState;
}CommandBufferAndSomeState;

void recordVkCommand(CommandBufferAndSomeState* destination, const RenderPassCommandGeneric* command, const RenderPassCommandBegin *beginInfo);
void recordVkCommands(WGPUCommandEncoder destination, WGPUDevice device, const RenderPassCommandGenericVector* commands, const RenderPassCommandBegin *beginInfo);

typedef struct WGPURenderPassEncoderImpl{
    VkRenderPass renderPass; //ONLY if !dynamicRendering

    RenderPassCommandBegin beginInfo;
    RenderPassCommandGenericVector bufferedCommands;
    
    WGPUDevice device;
    ResourceUsage resourceUsage;
    refcount_type refCount;

    WGPUPipelineLayout lastLayout;
    VkFramebuffer frameBuffer;
    WGPUCommandEncoder cmdEncoder;
}WGPURenderPassEncoderImpl;

typedef struct WGPUComputePassEncoderImpl{
    RenderPassCommandGenericVector bufferedCommands;
    WGPUDevice device;
    ResourceUsage resourceUsage;
    refcount_type refCount;

    WGPUPipelineLayout lastLayout;
    WGPUCommandEncoder cmdEncoder;
    WGPUBindGroup bindGroups[8];
}WGPUComputePassEncoderImpl;

static inline size_t hashDynamicState(DefaultDynamicState dst){
    float accum = dst.viewport.x;
    accum = accum * 1.79421351f + dst.blendConstants[0];
    accum = accum * 1.79421351f + dst.blendConstants[1];
    accum = accum * 1.79421351f + dst.blendConstants[2];
    accum = accum * 1.79421351f + dst.blendConstants[3];
    accum = accum * 1.19421351f + dst.viewport.y;
    accum = accum * 1.19421351f + dst.viewport.width;
    accum = accum * 1.19421351f + dst.viewport.height;
    accum = accum * 1.19421351f + dst.viewport.minDepth;
    accum = accum * 1.19421351f + dst.viewport.maxDepth;
    size_t ret = dst.scissorRect.extent.width;
    ret = (ret * PHM_HASH_MULTIPLIER) ^ dst.scissorRect.extent.height;
    ret = (ret * PHM_HASH_MULTIPLIER) ^ dst.scissorRect.offset.x;
    ret = (ret * PHM_HASH_MULTIPLIER) ^ dst.scissorRect.offset.y;
    ret = (ret * PHM_HASH_MULTIPLIER) ^ dst.stencilReference;
    return ret ^ ((size_t)accum);
}
static inline size_t cmpDynamicState(DefaultDynamicState a, DefaultDynamicState b){
    return 
    a.viewport.x == b.viewport.x &&
    a.viewport.y == b.viewport.y &&
    a.viewport.width == b.viewport.width &&
    a.viewport.height == b.viewport.height &&
    a.viewport.minDepth == b.viewport.minDepth &&
    a.viewport.maxDepth == b.viewport.maxDepth &&
    a.scissorRect.offset.x == b.scissorRect.offset.x &&
    a.scissorRect.offset.y == b.scissorRect.offset.y &&
    a.scissorRect.extent.width == b.scissorRect.extent.width &&
    a.scissorRect.extent.height == b.scissorRect.extent.height &&
    a.blendConstants[0] == b.blendConstants[0] &&
    a.blendConstants[0] == b.blendConstants[0] &&
    a.blendConstants[0] == b.blendConstants[0] &&
    a.blendConstants[0] == b.blendConstants[0] &&
    a.stencilReference == b.stencilReference;
}
DEFINE_GENERIC_HASH_MAP(static inline, DynamicStateCommandBufferMap, DefaultDynamicState, VkCommandBuffer, hashDynamicState, cmpDynamicState, (DefaultDynamicState){0})

typedef struct WGPURenderBundleImpl{
    RenderPassCommandGenericVector bufferedCommands;
    DynamicStateCommandBufferMap encodedCommandBuffers;
    WGPUDevice device;
    uint32_t refCount;
    
    VkFormat* colorAttachmentFormats;
    uint32_t colorAttachmentCount;
    VkFormat depthFormat;
    VkFormat depthStencilFormat;
}WGPURenderBundleImpl;

typedef struct WGPURenderBundleEncoderImpl{
    RenderPassCommandGenericVector bufferedCommands;
    WGPUDevice device;
    uint32_t refCount;
    uint32_t cacheIndex;
    WGPUBool movedFrom;
    WGPUPipelineLayout lastLayout;

    VkFormat* colorAttachmentFormats;
    uint32_t colorAttachmentCount;
    VkFormat depthStencilFormat;
}WGPURenderBundleEncoderImpl;

void RenderPassEncoder_PushCommand(WGPURenderPassEncoder, const RenderPassCommandGeneric* cmd);
void ComputePassEncoder_PushCommand(WGPUComputePassEncoder, const RenderPassCommandGeneric* cmd);

typedef struct WGPUCommandEncoderImpl{
    VkCommandBuffer buffer;
    uint32_t refCount;
    uint32_t encodedCommandCount;
    WGPURenderPassEncoderSet referencedRPs;
    WGPUComputePassEncoderSet referencedCPs;
    WGPURaytracingPassEncoderSet referencedRTs;

    ResourceUsage resourceUsage;
    WGPUDevice device;
    uint32_t cacheIndex;
    uint32_t movedFrom;
    
    
}WGPUCommandEncoderImpl;
typedef struct WGPUCommandBufferImpl{
    VkCommandBuffer buffer;
    refcount_type refCount;
    WGPURenderPassEncoderSet referencedRPs;
    WGPUComputePassEncoderSet referencedCPs;
    WGPURaytracingPassEncoderSet referencedRTs;
    
    ResourceUsage resourceUsage;
    WGPUString label;
    WGPUDevice device;
    uint32_t cacheIndex;
}WGPUCommandBufferImpl;


typedef struct WGPURaytracingPassEncoderImpl{
    VkCommandBuffer cmdBuffer;
    WGPUDevice device;
    WGPURaytracingPipeline lastPipeline;
    RenderPassCommandGenericVector bufferedCommands;
    ResourceUsage resourceUsage;
    refcount_type refCount;
    WGPUPipelineLayout lastLayout;
    WGPUCommandEncoder cmdEncoder;
    WGPUBindGroup bindGroups[8];
}WGPURaytracingPassEncoderImpl;

typedef enum SurfaceImplType{
    SurfaceImplType_Undefined = 0,
    SurfaceImplType_MetalLayer,
    SurfaceImplType_WindowsHWND,
    SurfaceImplType_XlibWindow,
    SurfaceImplType_WaylandSurface,
    SurfaceImplType_AndroidNativeWindow,
    SurfaceImplType_XCBWindow,
    SurfaceImplType_Force32 = 0x7FFFFFFF,
}SurfaceImplType;

typedef struct WGPUSurfaceImpl{
    
    VkSurfaceKHR surface;
    refcount_type refCount;
    WGPUDevice device;
    SurfaceImplType surfaceType;
    WGPUSurfaceConfiguration lastConfig;
    uint32_t formatCount;
    uint32_t wgpuFormatCount;
    VkSurfaceFormatKHR* formatCache;
    WGPUTextureFormat* wgpuFormatCache;

    uint32_t presentModeCount;
    WGPUPresentMode* presentModeCache;

    VkSwapchainKHR swapchain;
    uint32_t imagecount;
    uint32_t activeImageIndex;
    uint32_t width, height;
    VkFormat swapchainImageFormat;
    VkColorSpaceKHR swapchainColorSpace;
    WGPUTexture* images;
    VkSemaphore* presentSemaphores;
    WGPUSurfaceCapabilities capabilityCache;
}WGPUSurfaceImpl;

typedef struct WGPUQueueImpl{
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;
    VkQueue presentQueue;
    refcount_type refCount;

    SyncState syncState[framesInFlight];
    WGPUDevice device;

    WGPUCommandEncoder presubmitCache;

    PendingCommandBufferMap pendingCommandBuffers[framesInFlight];
}WGPUQueueImpl;



#ifndef WGPU_VALIDATION_ENABLED
#define WGPU_VALIDATION_ENABLED 1
#endif

typedef struct WGPUQuerySetImpl{
    VkQueryPool queryPool;
    WGPUQueryType type;
    refcount_type refCount;
    WGPUDevice device;
}WGPUQuerySetImpl;




#if WGPU_VALIDATION_ENABLED
#include <stdio.h> // For snprintf
#include <string.h> // For strlen, strcmp

// WGPU_VALIDATE_IMPL remains the same
#define WGPU_VALIDATE_IMPL(device_ptr, condition, error_type, message_str, log_level_on_fail) \
    do { \
        if (!(condition)) { \
            const char* resolved_message = (message_str); \
            size_t message_len = strlen(resolved_message); \
            if ((device_ptr) && (device_ptr)->uncapturedErrorCallbackInfo.callback) { \
                (device_ptr)->uncapturedErrorCallbackInfo.callback( \
                    (const WGPUDevice*)(device_ptr), \
                    (error_type), \
                    (WGPUStringView){resolved_message, message_len}, \
                    (device_ptr)->uncapturedErrorCallbackInfo.userdata1, \
                    (device_ptr)->uncapturedErrorCallbackInfo.userdata2 \
                ); \
            } \
            TRACELOG(log_level_on_fail, "Validation Failed: %s", resolved_message); \
            rassert(condition, resolved_message); \
        } \
    } while (0)

#define WGPU_VALIDATE(device_ptr, condition, message_str) WGPU_VALIDATE_IMPL(device_ptr, condition, WGPUErrorType_Validation, message_str, LOG_ERROR)
#define WGPU_VALIDATE_PTR(device_ptr, ptr, ptr_name_str) WGPU_VALIDATE_IMPL(device_ptr, (ptr) != NULL, WGPUErrorType_Validation, ptr_name_str " must not be NULL.", LOG_ERROR)
#define WGPU_VALIDATE_HANDLE(device_ptr, handle, handle_name_str) WGPU_VALIDATE_IMPL(device_ptr, (handle) != VK_NULL_HANDLE, WGPUErrorType_Validation, handle_name_str " is VK_NULL_HANDLE.", LOG_ERROR)

// Specific validation for descriptor structs
#define WGPU_VALIDATE_DESC_PTR(parent_device_ptr, desc_ptr, desc_name_str) \
    do { \
        if (!(desc_ptr)) { \
            const char* msg = desc_name_str " descriptor must not be NULL."; \
            WGPU_VALIDATE_IMPL(parent_device_ptr, false, WGPUErrorType_Validation, msg, LOG_ERROR); \
        } \
    } while (0)


// --- NEW MACROS FOR EQUALITY/INEQUALITY ---

/**
 * @brief Validates that two expressions `a` and `b` are equal.
 * Generates a detailed message on failure, including the stringified expressions and their runtime values.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first expression.
 * @param b The second expression.
 * @param fmt_a The printf-style format specifier for the type of 'a'.
 * @param fmt_b The printf-style format specifier for the type of 'b'.
 * @param context_msg A string providing context for the check (e.g., "sCreateInfo->sType").
 */
#define WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, fmt_a, fmt_b, context_msg) \
    do { \
        /* Evaluate a and b once to avoid side effects if they are complex expressions */ \
        /* Note: This requires C99 or for the types of a_val and b_val to be known, */ \
        /* or rely on type inference if using C++ or newer C standards with typeof. */ \
        /* For maximum C89/C90 compatibility, one might pass a and b directly, accepting multiple evaluations. */ \
        /* Given the context of Vulkan, C99+ is a safe assumption. */ \
        /* We will use __auto_type if available (GCC/Clang extension), otherwise user must be careful. */ \
        /* Or, simply evaluate (a) and (b) directly in the if and snprintf. */ \
        if (!((a) == (b))) { \
            char __wgpu_msg_buffer[512]; \
            snprintf(__wgpu_msg_buffer, sizeof(__wgpu_msg_buffer), \
                     "%s %s: Equality check '%s == %s' failed. LHS (" #a " = " fmt_a ") != RHS (" #b " = " fmt_b ").", \
                     __func__, (context_msg), #a, #b, (a), (b)); \
            WGPU_VALIDATE_IMPL(device_ptr, false, WGPUErrorType_Validation, __wgpu_msg_buffer, LOG_ERROR); \
        } \
    } while (0)


#define WGPU_VALIDATE_NEQ_FORMAT(device_ptr, a, b, fmt_a, fmt_b, context_msg) \
    do { \
        /* Evaluate a and b once to avoid side effects if they are complex expressions */ \
        /* Note: This requires C99 or for the types of a_val and b_val to be known, */ \
        /* or rely on type inference if using C++ or newer C standards with typeof. */ \
        /* For maximum C89/C90 compatibility, one might pass a and b directly, accepting multiple evaluations. */ \
        /* Given the context of Vulkan, C99+ is a safe assumption. */ \
        /* We will use __auto_type if available (GCC/Clang extension), otherwise user must be careful. */ \
        /* Or, simply evaluate (a) and (b) directly in the if and snprintf. */ \
        if (!((a) != (b))) { \
            char __wgpu_msg_buffer[512]; \
            snprintf(__wgpu_msg_buffer, sizeof(__wgpu_msg_buffer), \
                     "%s %s: Non equality check '%s != %s' failed. LHS (" #a " = " fmt_a ") != RHS (" #b " = " fmt_b ").", \
                     __func__, (context_msg), #a, #b, (a), (b)); \
            WGPU_VALIDATE_IMPL(device_ptr, false, WGPUErrorType_Validation, __wgpu_msg_buffer, LOG_ERROR); \
        } \
    } while (0)
/**
 * @brief Validates that two integer expressions `a` and `b` are equal.
 * Provides a default format specifier for integers.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first integer expression.
 * @param b The second integer expression.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_EQ_INT(device_ptr, a, b, context_msg) \
    WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, "%d", "%d", context_msg)

/**
 * @brief Validates that two unsigned integer expressions `a` and `b` are equal.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first unsigned integer expression.
 * @param b The second unsigned integer expression.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_EQ_UINT(device_ptr, a, b, context_msg) \
    WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, "%u", "%u", context_msg)

/**
 * @brief Validates that two pointer expressions `a` and `b` are equal.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first pointer expression.
 * @param b The second pointer expression.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_EQ_PTR(device_ptr, a, b, context_msg) \
    WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, "%p", "%p", context_msg)


#define WGPU_VALIDATE_NEQ_PTR(device_ptr, a, b, context_msg) \
    WGPU_VALIDATE_NEQ_FORMAT(device_ptr, a, b, "%p", "%p", context_msg)
/**
 * @brief Validates that two VkBool32 expressions `a` and `b` are equal.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first VkBool32 expression.
 * @param b The second VkBool32 expression.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_EQ_BOOL32(device_ptr, a, b, context_msg) \
    WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, "%u (BOOL32)", "%u (BOOL32)", context_msg)


/**
 * @brief Validates that two expressions `a` and `b` are NOT equal.
 * Generates a detailed message on failure.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first expression.
 * @param b The second expression.
 * @param fmt_a The printf-style format specifier for the type of 'a'.
 * @param fmt_b The printf-style format specifier for the type of 'b'.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_NE_FORMAT(device_ptr, a, b, fmt_a, fmt_b, context_msg) \
    do { \
        if (!((a) != (b))) { \
            char __wgpu_msg_buffer[512]; \
            snprintf(__wgpu_msg_buffer, sizeof(__wgpu_msg_buffer), \
                     "%s: Inequality check '%s != %s' failed. LHS (" #a " = " fmt_a ") == RHS (" #b " = " fmt_b ").", \
                     (context_msg), #a, #b, (a), (b)); \
            WGPU_VALIDATE_IMPL(device_ptr, false, WGPUErrorType_Validation, __wgpu_msg_buffer, LOG_ERROR); \
        } \
    } while (0)

/**
 * @brief Validates that two C-style strings `a` and `b` are equal using strcmp.
 * Assumes `a` and `b` are non-NULL (use WGPU_VALIDATE_PTR first if needed).
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first C-string.
 * @param b The second C-string.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_STREQ(device_ptr, a, b, context_msg) \
    do { \
        /* Ensure strings are not NULL before strcmp, or ensure this is handled by caller */ \
        /* WGPU_VALIDATE_PTR(device_ptr, (a), #a " for STREQ"); */ \
        /* WGPU_VALIDATE_PTR(device_ptr, (b), #b " for STREQ"); */ \
        if (strcmp((a), (b)) != 0) { \
            char __wgpu_msg_buffer[1024]; /* Potentially longer for strings */ \
            snprintf(__wgpu_msg_buffer, sizeof(__wgpu_msg_buffer), \
                     "%s: String equality check 'strcmp(%s, %s) == 0' failed. LHS (\"%s\") != RHS (\"%s\").", \
                     (context_msg), #a, #b, (a), (b)); \
            WGPU_VALIDATE_IMPL(device_ptr, false, WGPUErrorType_Validation, __wgpu_msg_buffer, LOG_ERROR); \
        } \
    } while (0)


#define WGPU_VALIDATION_ERROR_MESSAGE(message ...) \
    do {  \
        char vmessageBuffer[8192] = {0}; \
        snprintf(vmessageBuffer, 8192, message); \
        TRACELOG(LOG_ERROR, "Validation error: %s"); \
    } while (0)


/**
 * @brief Validates that two C-style strings `a` and `b` are NOT equal using strcmp.
 * Assumes `a` and `b` are non-NULL.
 * @param device_ptr Pointer to the WGPUDevice (can be NULL).
 * @param a The first C-string.
 * @param b The second C-string.
 * @param context_msg A string providing context for the check.
 */
#define WGPU_VALIDATE_STRNEQ(device_ptr, a, b, context_msg) \
    do { \
        if (strcmp((a), (b)) == 0) { \
            char __wgpu_msg_buffer[1024]; \
            snprintf(__wgpu_msg_buffer, sizeof(__wgpu_msg_buffer), \
                     "%s: String inequality check 'strcmp(%s, %s) != 0' failed. LHS (\"%s\") == RHS (\"%s\").", \
                     (context_msg), #a, #b, (a), (b)); \
            WGPU_VALIDATE_IMPL(device_ptr, false, WGPUErrorType_Validation, __wgpu_msg_buffer, LOG_ERROR); \
        } \
    } while (0)

    


#else // WGPU_VALIDATION_ENABLED not defined
// WGPU_VALIDATE_IMPL needs a dummy definition for the other macros to compile to ((void)0)
#define WGPU_VALIDATE_IMPL(device_ptr, condition, error_type, message_str, log_level_on_fail) ((void)0)

#define WGPU_VALIDATE(device_ptr, condition, message_str) ((void)0)
#define WGPU_VALIDATE_PTR(device_ptr, ptr, ptr_name_str) ((void)0)
#define WGPU_VALIDATE_HANDLE(device_ptr, handle, handle_name_str) ((void)0)
#define WGPU_VALIDATE_DESC_PTR(parent_device_ptr, desc_ptr, desc_name_str) ((void)0)

#define WGPU_VALIDATE_EQ_FORMAT(device_ptr, a, b, fmt_a, fmt_b, context_msg) ((void)0)
#define WGPU_VALIDATE_EQ_INT(device_ptr, a, b, context_msg) ((void)0)
#define WGPU_VALIDATE_EQ_UINT(device_ptr, a, b, context_msg) ((void)0)
#define WGPU_VALIDATE_EQ_BOOL32(device_ptr, a, b, context_msg) ((void)0)
#define WGPU_VALIDATE_NE_FORMAT(device_ptr, a, b, fmt_a, fmt_b, context_msg) ((void)0)
#define WGPU_VALIDATE_STREQ(device_ptr, a, b, context_msg) ((void)0)
#define WGPU_VALIDATE_STRNEQ(device_ptr, a, b, context_msg) ((void)0)

#endif // WGPU_VALIDATION_ENABLED

static inline VkQueryType toVulkanQueryType(WGPUQueryType type){
    switch(type){
        case WGPUQueryType_Occlusion:return VK_QUERY_TYPE_OCCLUSION;
        case WGPUQueryType_Timestamp:return VK_QUERY_TYPE_TIMESTAMP;
        default: return VK_QUERY_TYPE_MAX_ENUM;
    }
}
static inline bool isDepthFormat(WGPUTextureFormat format){
    return 
    format == WGPUTextureFormat_Depth16Unorm ||
    format == WGPUTextureFormat_Depth24Plus ||
    format == WGPUTextureFormat_Depth24PlusStencil8 ||
    format == WGPUTextureFormat_Depth32Float ||
    format == WGPUTextureFormat_Depth32FloatStencil8;
}
static inline VkAccelerationStructureTypeKHR toVulkanAccelerationStructureLevel(WGPURayTracingAccelerationContainerLevel level){
    return (level == WGPURayTracingAccelerationContainerLevel_Top) ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
}
static inline bool isDepthStencilFormat(WGPUTextureFormat format){
    return 
    format == WGPUTextureFormat_Depth24PlusStencil8 ||
    format == WGPUTextureFormat_Depth32FloatStencil8;
}
static inline VkImageUsageFlags toVulkanTextureUsage(WGPUTextureUsage usage, WGPUTextureFormat format) {
    VkImageUsageFlags vkUsage = 0;

    if (usage & WGPUTextureUsage_CopySrc) {
        vkUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (usage & WGPUTextureUsage_CopyDst) {
        vkUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (usage & WGPUTextureUsage_TextureBinding) {
        vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage & WGPUTextureUsage_StorageBinding) {
        vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage & WGPUTextureUsage_RenderAttachment) {
        if (isDepthFormat(format)) {
            vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    if (usage & WGPUTextureUsage_TransientAttachment) {
        vkUsage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
    if (usage & WGPUTextureUsage_StorageAttachment) { // Note: Original code mapped this to VK_IMAGE_USAGE_STORAGE_BIT
        vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    // Any WGPUTextureUsage flags not explicitly handled here will be ignored.

    return vkUsage;
}
static inline WGPUCompositeAlphaMode fromVulkanCompositeAlphaMode(VkCompositeAlphaFlagBitsKHR vkFlags){
    if(vkFlags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR){
        return WGPUCompositeAlphaMode_Opaque;
    }
    if(vkFlags & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR){
        return WGPUCompositeAlphaMode_Premultiplied;
    }
    if(vkFlags & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR){
        return WGPUCompositeAlphaMode_Unpremultiplied;
    }
    if(vkFlags & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR){
        return WGPUCompositeAlphaMode_Inherit;
    }
    return WGPUCompositeAlphaMode_Force32;
}
static inline VkCompositeAlphaFlagsKHR toVulkanCompositeAlphaMode(WGPUCompositeAlphaMode wacm){
    switch(wacm){
        case WGPUCompositeAlphaMode_Opaque: return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        case WGPUCompositeAlphaMode_Premultiplied: return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        case WGPUCompositeAlphaMode_Unpremultiplied: return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        case WGPUCompositeAlphaMode_Inherit: return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        case WGPUCompositeAlphaMode_Auto: return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        default: rg_unreachable();
    }
    return 0;
}


static inline VkImageAspectFlags toVulkanAspectMask(WGPUTextureAspect aspect, WGPUTextureFormat format){
    bool depth = isDepthFormat(format);
    bool depthStencil = isDepthStencilFormat(format);
    
    switch(aspect){
        case WGPUTextureAspect_All:{
            if(depthStencil)return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            if(depth)return VK_IMAGE_ASPECT_DEPTH_BIT;
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
        case WGPUTextureAspect_DepthOnly:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
        case WGPUTextureAspect_StencilOnly:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
        case WGPUTextureAspect_Plane0Only:
        return VK_IMAGE_ASPECT_PLANE_0_BIT;
        case WGPUTextureAspect_Plane1Only:
        return VK_IMAGE_ASPECT_PLANE_1_BIT;
        case WGPUTextureAspect_Plane2Only:
        return VK_IMAGE_ASPECT_PLANE_2_BIT;

        default: {
            assert(false && "This aspect is not implemented");
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}

static inline bool isDepthFormatVk(VkFormat format){
    return
    format == VK_FORMAT_D16_UNORM ||
    format == VK_FORMAT_X8_D24_UNORM_PACK32 || // Equivalent for Depth24Plus
    format == VK_FORMAT_D24_UNORM_S8_UINT ||
    format == VK_FORMAT_D32_SFLOAT ||
    format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static inline bool isDepthStencilFormatVk(VkFormat format){
    return
    format == VK_FORMAT_D24_UNORM_S8_UINT ||
    format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static inline VkImageAspectFlags toVulkanAspectMaskVk(WGPUTextureAspect aspect, VkFormat format){
    bool depth = isDepthFormatVk(format);
    bool depthStencil = isDepthStencilFormatVk(format);

    switch(aspect){
        case WGPUTextureAspect_All:{
            if(depthStencil)return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            if(depth)return VK_IMAGE_ASPECT_DEPTH_BIT;
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
        case WGPUTextureAspect_DepthOnly:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
        case WGPUTextureAspect_StencilOnly:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
        case WGPUTextureAspect_Plane0Only:
        return VK_IMAGE_ASPECT_PLANE_0_BIT;
        case WGPUTextureAspect_Plane1Only:
        return VK_IMAGE_ASPECT_PLANE_1_BIT;
        case WGPUTextureAspect_Plane2Only:
        return VK_IMAGE_ASPECT_PLANE_2_BIT;

        default: {
            assert(false && "This aspect is not implemented");
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}
// Inverse conversion: Vulkan usage flags -> WGPUTextureUsage flags
static inline WGPUTextureUsage fromVulkanWGPUTextureUsage(VkImageUsageFlags vkUsage) {
    WGPUTextureUsage usage = 0;
    
    // Map transfer bits
    if (vkUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        usage |= WGPUTextureUsage_CopySrc;
    if (vkUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        usage |= WGPUTextureUsage_CopyDst;
    
    // Map sampling bit
    if (vkUsage & VK_IMAGE_USAGE_SAMPLED_BIT)
        usage |= WGPUTextureUsage_TextureBinding;
    
    // Map storage bit (ambiguous: could originate from either storage flag)
    if (vkUsage & VK_IMAGE_USAGE_STORAGE_BIT)
        usage |= WGPUTextureUsage_StorageBinding | WGPUTextureUsage_StorageAttachment;
    
    // Map render attachment bits (depth/stencil or color, both yield RenderAttachment)
    if (vkUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        usage |= WGPUTextureUsage_RenderAttachment;
    if (vkUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        usage |= WGPUTextureUsage_RenderAttachment;
    
    // Map transient attachment
    if (vkUsage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
        usage |= WGPUTextureUsage_TransientAttachment;
    
    return usage;
}




static inline VkBufferUsageFlags toVulkanBufferUsage(WGPUBufferUsage busg) {
    VkBufferUsageFlags usage = 0;

    // Input: WGPUBufferUsageFlags busg

    if (busg & WGPUBufferUsage_CopySrc) {
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if (busg & WGPUBufferUsage_CopyDst) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    if (busg & WGPUBufferUsage_Vertex) {
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (busg & WGPUBufferUsage_Index) {
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (busg & WGPUBufferUsage_Uniform) {
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (busg & WGPUBufferUsage_Storage) {
        usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (busg & WGPUBufferUsage_Indirect) {
        usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    if (busg & WGPUBufferUsage_MapRead) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    if (busg & WGPUBufferUsage_MapWrite) {
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if (busg & WGPUBufferUsage_ShaderDeviceAddress) {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    if (busg & WGPUBufferUsage_AccelerationStructureInput) {
        usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (busg & WGPUBufferUsage_AccelerationStructureStorage) {
        usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }
    if (busg & WGPUBufferUsage_ShaderBindingTable) {
        usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    }
    return usage;
}

static inline size_t wgpuStrlen(WGPUStringView ws) {
    if (ws.length == WGPU_STRLEN) {
        size_t i = 0;
        while (ws.data[i] != '\0') {
            i++;
        }
        return i;
    } else {
        return ws.length;
    }
}

static inline int wgpuStrcmp(WGPUStringView ws, WGPUStringView ws2) {
    size_t len1 = wgpuStrlen(ws);
    size_t len2 = wgpuStrlen(ws2);
    size_t min_len = (len1 < len2) ? len1 : len2;
    for (size_t i = 0; i < min_len; ++i) {
        if (ws.data[i] != ws2.data[i]) {
            return (unsigned char)ws.data[i] - (unsigned char)ws2.data[i];
        }
    }
    if (len1 != len2) {
        return (len1 > len2) ? 1 : -1;
    }
    return 0;
}

static inline int wgpuStrcmpc(WGPUStringView ws, const char* data) {
    size_t len1 = wgpuStrlen(ws);
    size_t len2 = 0;
    while(data[len2] != '\0'){
        len2++;
    }
    size_t min_len = (len1 < len2) ? len1 : len2;
    for (size_t i = 0; i < min_len; ++i) {
        if (ws.data[i] != data[i]) {
            return (unsigned char)ws.data[i] - (unsigned char)data[i];
        }
    }
    if (len1 != len2) {
        return (len1 > len2) ? 1 : -1;
    }
    return 0;
}

static inline int wgpuStrcmpcn(WGPUStringView ws, const char* data, size_t maxlength) {
    size_t len1 = wgpuStrlen(ws);
    size_t len2 = 0;
    while(len2 < maxlength && data[len2] != '\0'){
        len2++;
    }
    size_t min_len = (len1 < len2) ? len1 : len2;
    for (size_t i = 0; i < min_len; ++i) {
        if (ws.data[i] != data[i]) {
            return (unsigned char)ws.data[i] - (unsigned char)data[i];
        }
    }
    if (len1 != len2) {
        return (len1 > len2) ? 1 : -1;
    }
    return 0;
}


static inline VkColorComponentFlags toVulkanColorWriteMask(WGPUColorWriteMask mask){
    VkColorComponentFlags ret = 0;
    if(mask == WGPUColorWriteMask_All){ // Open to the possibility of WGPUColorWriteMask_All == 0
        return VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    if(mask & WGPUColorWriteMask_Red){
        ret |= VK_COLOR_COMPONENT_R_BIT;
    }
    if(mask & WGPUColorWriteMask_Green){
        ret |= VK_COLOR_COMPONENT_G_BIT;
    }
    if(mask & WGPUColorWriteMask_Blue){
        ret |= VK_COLOR_COMPONENT_B_BIT;
    }
    if(mask & WGPUColorWriteMask_Alpha){
        ret |= VK_COLOR_COMPONENT_A_BIT;
    }
    return ret;
}
static inline VkImageViewType toVulkanTextureViewDimension(WGPUTextureViewDimension dim){
    VkImageViewCreateInfo info;
    switch(dim){
        default:
        case 0:{
            rg_unreachable();
        }
        case WGPUTextureViewDimension_1D:{
            return VK_IMAGE_VIEW_TYPE_1D;
        }
        case WGPUTextureViewDimension_2D:{
            return VK_IMAGE_VIEW_TYPE_2D;
        }
        case WGPUTextureViewDimension_3D:{
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        case WGPUTextureViewDimension_2DArray:{
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

    }
}
static inline VkImageType toVulkanTextureDimension(WGPUTextureDimension dim){
    VkImageViewCreateInfo info;
    switch(dim){
        default:
        case 0:{
            rg_unreachable();
        }
        case WGPUTextureDimension_1D:{
            return VK_IMAGE_TYPE_1D;
        }
        case WGPUTextureDimension_2D:{
            return VK_IMAGE_TYPE_2D;
        }
        case WGPUTextureDimension_3D:{
            return VK_IMAGE_TYPE_3D;
        }
    }
}
static inline WGPUTextureDimension fromVulkanTextureDimension(VkImageType dim){
    VkImageViewCreateInfo info;
    switch(dim){
        default:
            rg_unreachable();
        case VK_IMAGE_TYPE_1D:{
            return WGPUTextureDimension_1D;
        }
        case VK_IMAGE_TYPE_2D:{
            return WGPUTextureDimension_2D;
        }
        case VK_IMAGE_TYPE_3D:{
            return WGPUTextureDimension_3D;
        }
    }
}


// Converts WGPUTextureFormat to VkFormat
static inline VkFormat toVulkanPixelFormat(WGPUTextureFormat format) {
    switch (format) {
        case WGPUTextureFormat_Undefined:            return VK_FORMAT_UNDEFINED;
        case WGPUTextureFormat_R8Unorm:              return VK_FORMAT_R8_UNORM;
        case WGPUTextureFormat_R8Snorm:              return VK_FORMAT_R8_SNORM;
        case WGPUTextureFormat_R8Uint:               return VK_FORMAT_R8_UINT;
        case WGPUTextureFormat_R8Sint:               return VK_FORMAT_R8_SINT;
        case WGPUTextureFormat_R16Uint:              return VK_FORMAT_R16_UINT;
        case WGPUTextureFormat_R16Sint:              return VK_FORMAT_R16_SINT;
        case WGPUTextureFormat_R16Float:             return VK_FORMAT_R16_SFLOAT;
        case WGPUTextureFormat_RG8Unorm:             return VK_FORMAT_R8G8_UNORM;
        case WGPUTextureFormat_RG8Snorm:             return VK_FORMAT_R8G8_SNORM;
        case WGPUTextureFormat_RG8Uint:              return VK_FORMAT_R8G8_UINT;
        case WGPUTextureFormat_RG8Sint:              return VK_FORMAT_R8G8_SINT;
        case WGPUTextureFormat_R32Float:             return VK_FORMAT_R32_SFLOAT;
        case WGPUTextureFormat_R32Uint:              return VK_FORMAT_R32_UINT;
        case WGPUTextureFormat_R32Sint:              return VK_FORMAT_R32_SINT;
        case WGPUTextureFormat_RG16Uint:             return VK_FORMAT_R16G16_UINT;
        case WGPUTextureFormat_RG16Sint:             return VK_FORMAT_R16G16_SINT;
        case WGPUTextureFormat_RG16Float:            return VK_FORMAT_R16G16_SFLOAT;
        case WGPUTextureFormat_RGBA8Unorm:           return VK_FORMAT_R8G8B8A8_UNORM;
        case WGPUTextureFormat_RGBA8UnormSrgb:       return VK_FORMAT_R8G8B8A8_SRGB;
        case WGPUTextureFormat_RGBA8Snorm:           return VK_FORMAT_R8G8B8A8_SNORM;
        case WGPUTextureFormat_RGBA8Uint:            return VK_FORMAT_R8G8B8A8_UINT;
        case WGPUTextureFormat_RGBA8Sint:            return VK_FORMAT_R8G8B8A8_SINT;
        case WGPUTextureFormat_BGRA8Unorm:           return VK_FORMAT_B8G8R8A8_UNORM;
        case WGPUTextureFormat_BGRA8UnormSrgb:       return VK_FORMAT_B8G8R8A8_SRGB;
        case WGPUTextureFormat_RGB10A2Uint:          return VK_FORMAT_A2R10G10B10_UINT_PACK32; // Or A2B10G10R10_UINT_PACK32, check WGPU spec
        case WGPUTextureFormat_RGB10A2Unorm:         return VK_FORMAT_A2R10G10B10_UNORM_PACK32; // Or A2B10G10R10_UNORM_PACK32
        case WGPUTextureFormat_RG11B10Ufloat:        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case WGPUTextureFormat_RGB9E5Ufloat:         return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case WGPUTextureFormat_RG32Float:            return VK_FORMAT_R32G32_SFLOAT;
        case WGPUTextureFormat_RG32Uint:             return VK_FORMAT_R32G32_UINT;
        case WGPUTextureFormat_RG32Sint:             return VK_FORMAT_R32G32_SINT;
        case WGPUTextureFormat_RGBA16Uint:           return VK_FORMAT_R16G16B16A16_UINT;
        case WGPUTextureFormat_RGBA16Sint:           return VK_FORMAT_R16G16B16A16_SINT;
        case WGPUTextureFormat_RGBA16Float:          return VK_FORMAT_R16G16B16A16_SFLOAT;
        case WGPUTextureFormat_RGBA32Float:          return VK_FORMAT_R32G32B32A32_SFLOAT;
        case WGPUTextureFormat_RGBA32Uint:           return VK_FORMAT_R32G32B32A32_UINT;
        case WGPUTextureFormat_RGBA32Sint:           return VK_FORMAT_R32G32B32A32_SINT;
        case WGPUTextureFormat_Stencil8:             return VK_FORMAT_S8_UINT;
        case WGPUTextureFormat_Depth16Unorm:         return VK_FORMAT_D16_UNORM;
        case WGPUTextureFormat_Depth24Plus:          return VK_FORMAT_X8_D24_UNORM_PACK32; // Or D24_UNORM_S8_UINT if stencil is guaranteed
        case WGPUTextureFormat_Depth24PlusStencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
        case WGPUTextureFormat_Depth32Float:         return VK_FORMAT_D32_SFLOAT;
        case WGPUTextureFormat_Depth32FloatStencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case WGPUTextureFormat_BC1RGBAUnorm:         return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case WGPUTextureFormat_BC1RGBAUnormSrgb:     return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case WGPUTextureFormat_BC2RGBAUnorm:         return VK_FORMAT_BC2_UNORM_BLOCK;
        case WGPUTextureFormat_BC2RGBAUnormSrgb:     return VK_FORMAT_BC2_SRGB_BLOCK;
        case WGPUTextureFormat_BC3RGBAUnorm:         return VK_FORMAT_BC3_UNORM_BLOCK;
        case WGPUTextureFormat_BC3RGBAUnormSrgb:     return VK_FORMAT_BC3_SRGB_BLOCK;
        case WGPUTextureFormat_BC4RUnorm:            return VK_FORMAT_BC4_UNORM_BLOCK;
        case WGPUTextureFormat_BC4RSnorm:            return VK_FORMAT_BC4_SNORM_BLOCK;
        case WGPUTextureFormat_BC5RGUnorm:           return VK_FORMAT_BC5_UNORM_BLOCK;
        case WGPUTextureFormat_BC5RGSnorm:           return VK_FORMAT_BC5_SNORM_BLOCK;
        case WGPUTextureFormat_BC6HRGBUfloat:        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case WGPUTextureFormat_BC6HRGBFloat:         return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case WGPUTextureFormat_BC7RGBAUnorm:         return VK_FORMAT_BC7_UNORM_BLOCK;
        case WGPUTextureFormat_BC7RGBAUnormSrgb:     return VK_FORMAT_BC7_SRGB_BLOCK;
        case WGPUTextureFormat_ETC2RGB8Unorm:        return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case WGPUTextureFormat_ETC2RGB8UnormSrgb:    return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case WGPUTextureFormat_ETC2RGB8A1Unorm:      return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:  return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case WGPUTextureFormat_ETC2RGBA8Unorm:       return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case WGPUTextureFormat_ETC2RGBA8UnormSrgb:   return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        case WGPUTextureFormat_EACR11Unorm:          return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        case WGPUTextureFormat_EACR11Snorm:          return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        case WGPUTextureFormat_EACRG11Unorm:         return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        case WGPUTextureFormat_EACRG11Snorm:         return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
        case WGPUTextureFormat_ASTC4x4Unorm:         return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC4x4UnormSrgb:     return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC5x4Unorm:         return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC5x4UnormSrgb:     return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC5x5Unorm:         return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC5x5UnormSrgb:     return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC6x5Unorm:         return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC6x5UnormSrgb:     return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC6x6Unorm:         return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC6x6UnormSrgb:     return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC8x5Unorm:         return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC8x5UnormSrgb:     return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC8x6Unorm:         return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC8x6UnormSrgb:     return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC8x8Unorm:         return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC8x8UnormSrgb:     return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC10x5Unorm:        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC10x5UnormSrgb:    return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC10x6Unorm:        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC10x6UnormSrgb:    return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC10x8Unorm:        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC10x8UnormSrgb:    return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC10x10Unorm:       return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC10x10UnormSrgb:   return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC12x10Unorm:       return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC12x10UnormSrgb:   return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
        case WGPUTextureFormat_ASTC12x12Unorm:       return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        case WGPUTextureFormat_ASTC12x12UnormSrgb:   return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        // WGPUTextureFormat_Force32 is a utility, not a real format.
        default:                                     return VK_FORMAT_UNDEFINED;
    }
}

static inline VkSamplerAddressMode toVulkanAddressMode(WGPUAddressMode mode){
    switch(mode){
        case WGPUAddressMode_ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case WGPUAddressMode_Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case WGPUAddressMode_MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case WGPUAddressMode_Undefined:
        case WGPUAddressMode_Force32:
        rg_unreachable();
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; 
    }
}

static inline WGPUTextureFormat fromVulkanPixelFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED: return WGPUTextureFormat_Undefined;
        case VK_FORMAT_R8_UNORM: return WGPUTextureFormat_R8Unorm;
        case VK_FORMAT_R8_SNORM: return WGPUTextureFormat_R8Snorm;
        case VK_FORMAT_R8_UINT: return WGPUTextureFormat_R8Uint;
        case VK_FORMAT_R8_SINT: return WGPUTextureFormat_R8Sint;
        case VK_FORMAT_R16_UINT: return WGPUTextureFormat_R16Uint;
        case VK_FORMAT_R16_SINT: return WGPUTextureFormat_R16Sint;
        case VK_FORMAT_R16_SFLOAT: return WGPUTextureFormat_R16Float;
        case VK_FORMAT_R8G8_UNORM: return WGPUTextureFormat_RG8Unorm;
        case VK_FORMAT_R8G8_SNORM: return WGPUTextureFormat_RG8Snorm;
        case VK_FORMAT_R8G8_UINT: return WGPUTextureFormat_RG8Uint;
        case VK_FORMAT_R8G8_SINT: return WGPUTextureFormat_RG8Sint;
        case VK_FORMAT_R32_SFLOAT: return WGPUTextureFormat_R32Float;
        case VK_FORMAT_R32_UINT: return WGPUTextureFormat_R32Uint;
        case VK_FORMAT_R32_SINT: return WGPUTextureFormat_R32Sint;
        case VK_FORMAT_R16G16_UINT: return WGPUTextureFormat_RG16Uint;
        case VK_FORMAT_R16G16_SINT: return WGPUTextureFormat_RG16Sint;
        case VK_FORMAT_R16G16_SFLOAT: return WGPUTextureFormat_RG16Float;
        case VK_FORMAT_R8G8B8A8_UNORM: return WGPUTextureFormat_RGBA8Unorm;
        case VK_FORMAT_R8G8B8A8_SRGB: return WGPUTextureFormat_RGBA8UnormSrgb;
        case VK_FORMAT_R8G8B8A8_SNORM: return WGPUTextureFormat_RGBA8Snorm;
        case VK_FORMAT_R8G8B8A8_UINT: return WGPUTextureFormat_RGBA8Uint;
        case VK_FORMAT_R8G8B8A8_SINT: return WGPUTextureFormat_RGBA8Sint;
        case VK_FORMAT_B8G8R8A8_UNORM: return WGPUTextureFormat_BGRA8Unorm;
        case VK_FORMAT_B8G8R8A8_SRGB: return WGPUTextureFormat_BGRA8UnormSrgb;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return WGPUTextureFormat_RGB10A2Uint;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return WGPUTextureFormat_RGB10A2Unorm;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return WGPUTextureFormat_RG11B10Ufloat;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return WGPUTextureFormat_RGB9E5Ufloat;
        case VK_FORMAT_R32G32_SFLOAT: return WGPUTextureFormat_RG32Float;
        case VK_FORMAT_R32G32_UINT: return WGPUTextureFormat_RG32Uint;
        case VK_FORMAT_R32G32_SINT: return WGPUTextureFormat_RG32Sint;
        case VK_FORMAT_R16G16B16A16_UINT: return WGPUTextureFormat_RGBA16Uint;
        case VK_FORMAT_R16G16B16A16_SINT: return WGPUTextureFormat_RGBA16Sint;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return WGPUTextureFormat_RGBA16Float;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return WGPUTextureFormat_RGBA32Float;
        case VK_FORMAT_R32G32B32A32_UINT: return WGPUTextureFormat_RGBA32Uint;
        case VK_FORMAT_R32G32B32A32_SINT: return WGPUTextureFormat_RGBA32Sint;
        case VK_FORMAT_S8_UINT: return WGPUTextureFormat_Stencil8;
        case VK_FORMAT_D16_UNORM: return WGPUTextureFormat_Depth16Unorm;
        case VK_FORMAT_X8_D24_UNORM_PACK32: return WGPUTextureFormat_Depth24Plus;
        case VK_FORMAT_D24_UNORM_S8_UINT: return WGPUTextureFormat_Depth24PlusStencil8;
        case VK_FORMAT_D32_SFLOAT: return WGPUTextureFormat_Depth32Float;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return WGPUTextureFormat_Depth32FloatStencil8;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return WGPUTextureFormat_BC1RGBAUnorm;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return WGPUTextureFormat_BC1RGBAUnormSrgb;
        case VK_FORMAT_BC2_UNORM_BLOCK: return WGPUTextureFormat_BC2RGBAUnorm;
        case VK_FORMAT_BC2_SRGB_BLOCK: return WGPUTextureFormat_BC2RGBAUnormSrgb;
        case VK_FORMAT_BC3_UNORM_BLOCK: return WGPUTextureFormat_BC3RGBAUnorm;
        case VK_FORMAT_BC3_SRGB_BLOCK: return WGPUTextureFormat_BC3RGBAUnormSrgb;
        case VK_FORMAT_BC4_UNORM_BLOCK: return WGPUTextureFormat_BC4RUnorm;
        case VK_FORMAT_BC4_SNORM_BLOCK: return WGPUTextureFormat_BC4RSnorm;
        case VK_FORMAT_BC5_UNORM_BLOCK: return WGPUTextureFormat_BC5RGUnorm;
        case VK_FORMAT_BC5_SNORM_BLOCK: return WGPUTextureFormat_BC5RGSnorm;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return WGPUTextureFormat_BC6HRGBUfloat;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return WGPUTextureFormat_BC6HRGBFloat;
        case VK_FORMAT_BC7_UNORM_BLOCK: return WGPUTextureFormat_BC7RGBAUnorm;
        case VK_FORMAT_BC7_SRGB_BLOCK: return WGPUTextureFormat_BC7RGBAUnormSrgb;
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return WGPUTextureFormat_ETC2RGB8Unorm;
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return WGPUTextureFormat_ETC2RGB8UnormSrgb;
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return WGPUTextureFormat_ETC2RGB8A1Unorm;
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return WGPUTextureFormat_ETC2RGB8A1UnormSrgb;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return WGPUTextureFormat_ETC2RGBA8Unorm;
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return WGPUTextureFormat_ETC2RGBA8UnormSrgb;
        case VK_FORMAT_EAC_R11_UNORM_BLOCK: return WGPUTextureFormat_EACR11Unorm;
        case VK_FORMAT_EAC_R11_SNORM_BLOCK: return WGPUTextureFormat_EACR11Snorm;
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return WGPUTextureFormat_EACRG11Unorm;
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return WGPUTextureFormat_EACRG11Snorm;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return WGPUTextureFormat_ASTC4x4Unorm;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return WGPUTextureFormat_ASTC4x4UnormSrgb;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return WGPUTextureFormat_ASTC5x4Unorm;
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return WGPUTextureFormat_ASTC5x4UnormSrgb;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return WGPUTextureFormat_ASTC5x5Unorm;
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return WGPUTextureFormat_ASTC5x5UnormSrgb;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return WGPUTextureFormat_ASTC6x5Unorm;
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return WGPUTextureFormat_ASTC6x5UnormSrgb;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return WGPUTextureFormat_ASTC6x6Unorm;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return WGPUTextureFormat_ASTC6x6UnormSrgb;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return WGPUTextureFormat_ASTC8x5Unorm;
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return WGPUTextureFormat_ASTC8x5UnormSrgb;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return WGPUTextureFormat_ASTC8x6Unorm;
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return WGPUTextureFormat_ASTC8x6UnormSrgb;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return WGPUTextureFormat_ASTC8x8Unorm;
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return WGPUTextureFormat_ASTC8x8UnormSrgb;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return WGPUTextureFormat_ASTC10x5Unorm;
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return WGPUTextureFormat_ASTC10x5UnormSrgb;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return WGPUTextureFormat_ASTC10x6Unorm;
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return WGPUTextureFormat_ASTC10x6UnormSrgb;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return WGPUTextureFormat_ASTC10x8Unorm;
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return WGPUTextureFormat_ASTC10x8UnormSrgb;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return WGPUTextureFormat_ASTC10x10Unorm;
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return WGPUTextureFormat_ASTC10x10UnormSrgb;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return WGPUTextureFormat_ASTC12x10Unorm;
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return WGPUTextureFormat_ASTC12x10UnormSrgb;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return WGPUTextureFormat_ASTC12x12Unorm;
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return WGPUTextureFormat_ASTC12x12UnormSrgb;
        default:                                     return WGPUTextureFormat_Undefined;
    }
}


static inline VkAttachmentStoreOp toVulkanStoreOperation(WGPUStoreOp lop) {
    switch (lop) {
    case WGPUStoreOp_Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case WGPUStoreOp_Discard:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case WGPUStoreOp_Undefined:
    
        return VK_ATTACHMENT_STORE_OP_DONT_CARE; // Example fallback
    default:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE; // Default fallback
    }
}
static inline VkAttachmentLoadOp toVulkanLoadOperation(WGPULoadOp lop) {
    switch (lop) {
    case WGPULoadOp_Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case WGPULoadOp_Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case WGPULoadOp_ExpandResolveTexture:
        // Vulkan does not have a direct equivalent; choose appropriate op or handle separately
        return VK_ATTACHMENT_LOAD_OP_LOAD; // Example fallback
    default:
        return VK_ATTACHMENT_LOAD_OP_LOAD; // Default fallback
    }
}
static inline WGPUPresentMode fromVulkanPresentMode(VkPresentModeKHR mode){
    switch(mode){
        case VK_PRESENT_MODE_FIFO_KHR:
            return WGPUPresentMode_Fifo;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            return WGPUPresentMode_FifoRelaxed;
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return WGPUPresentMode_Immediate;
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return WGPUPresentMode_Mailbox;
        default:
            return (WGPUPresentMode)~0;
    }
}

static inline VkPresentModeKHR toVulkanPresentMode(WGPUPresentMode mode){
    switch(mode){
        case WGPUPresentMode_Fifo:
            return VK_PRESENT_MODE_FIFO_KHR;
        case WGPUPresentMode_FifoRelaxed:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case WGPUPresentMode_Immediate:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case WGPUPresentMode_Mailbox:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        default:
            rg_trap();
    }
    return (VkPresentModeKHR)~0;
}

static inline VkCompareOp toVulkanCompareFunction(WGPUCompareFunction cf) {
    switch (cf) {
    case WGPUCompareFunction_Never:
        return VK_COMPARE_OP_NEVER;
    case WGPUCompareFunction_Less:
        return VK_COMPARE_OP_LESS;
    case WGPUCompareFunction_Equal:
        return VK_COMPARE_OP_EQUAL;
    case WGPUCompareFunction_LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case WGPUCompareFunction_Greater:
        return VK_COMPARE_OP_GREATER;
    case WGPUCompareFunction_NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case WGPUCompareFunction_GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case WGPUCompareFunction_Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        rg_unreachable();
        return VK_COMPARE_OP_ALWAYS; // Default fallback
    }
}
static inline VkSamplerMipmapMode toVulkanMipmapFilter(WGPUMipmapFilterMode mmfilter){
    switch(mmfilter){
        case WGPUMipmapFilterMode_Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case WGPUMipmapFilterMode_Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        default:
            rg_unreachable();
            return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}
static inline VkFilter toVulkanFilterMode(WGPUFilterMode filterMode){
    switch(filterMode){
        case WGPUFilterMode_Linear:
            return VK_FILTER_LINEAR;
        case WGPUFilterMode_Nearest:
            return VK_FILTER_NEAREST;
        default:
            rg_unreachable();
            return VK_FILTER_LINEAR;
    }
}

static inline VkStencilOp toVulkanStencilOperation(WGPUStencilOperation op){
    switch(op){
        case WGPUStencilOperation_Undefined: return VK_STENCIL_OP_ZERO;
        case WGPUStencilOperation_Keep: return VK_STENCIL_OP_KEEP;
        case WGPUStencilOperation_Zero: return VK_STENCIL_OP_ZERO;
        case WGPUStencilOperation_Replace: return VK_STENCIL_OP_REPLACE;
        case WGPUStencilOperation_Invert: return VK_STENCIL_OP_INVERT;
        case WGPUStencilOperation_IncrementClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case WGPUStencilOperation_DecrementClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case WGPUStencilOperation_IncrementWrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case WGPUStencilOperation_DecrementWrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default: rg_unreachable();
    }
}
static inline VkCullModeFlags toVulkanCullMode(WGPUCullMode cm){
    switch(cm){
        case WGPUCullMode_Back: return VK_CULL_MODE_BACK_BIT;
        case WGPUCullMode_Front: return VK_CULL_MODE_FRONT_BIT;
        case WGPUCullMode_None: return VK_CULL_MODE_NONE;
        default: rg_unreachable();
    }
}

//static inline VkPrimitiveTopology toVulkanPrimitive(PrimitiveType type){
//    switch(type){
//        case RL_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
//        case RL_TRIANGLES: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//        case RL_LINES: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
//        case RL_POINTS: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
//        case RL_QUADS:
//            //rassert(false, "Quads are not a primitive type");
//        default:
//            rg_unreachable();
//    }
//}
static inline VkPrimitiveTopology toVulkanPrimitiveTopology(WGPUPrimitiveTopology type){
    switch(type){
        case WGPUPrimitiveTopology_PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case WGPUPrimitiveTopology_LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case WGPUPrimitiveTopology_LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case WGPUPrimitiveTopology_TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case WGPUPrimitiveTopology_TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case WGPUPrimitiveTopology_Undefined:
        case WGPUPrimitiveTopology_Force32:
        rg_unreachable();
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

static inline VkFrontFace toVulkanFrontFace(WGPUFrontFace ff) {
    switch (ff) {
    case WGPUFrontFace_CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case WGPUFrontFace_CW:
        return VK_FRONT_FACE_CLOCKWISE;
    default:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE; // Default fallback
    }
}

static inline VkFormat toVulkanVertexFormat(WGPUVertexFormat vf) {
    switch (vf) {
    case WGPUVertexFormat_Uint8:
        return VK_FORMAT_R8_UINT;
    case WGPUVertexFormat_Uint8x2:
        return VK_FORMAT_R8G8_UINT;
    case WGPUVertexFormat_Uint8x4:
        return VK_FORMAT_R8G8B8A8_UINT;
    case WGPUVertexFormat_Sint8:
        return VK_FORMAT_R8_SINT;
    case WGPUVertexFormat_Sint8x2:
        return VK_FORMAT_R8G8_SINT;
    case WGPUVertexFormat_Sint8x4:
        return VK_FORMAT_R8G8B8A8_SINT;
    case WGPUVertexFormat_Unorm8:
        return VK_FORMAT_R8_UNORM;
    case WGPUVertexFormat_Unorm8x2:
        return VK_FORMAT_R8G8_UNORM;
    case WGPUVertexFormat_Unorm8x4:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case WGPUVertexFormat_Snorm8:
        return VK_FORMAT_R8_SNORM;
    case WGPUVertexFormat_Snorm8x2:
        return VK_FORMAT_R8G8_SNORM;
    case WGPUVertexFormat_Snorm8x4:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case WGPUVertexFormat_Uint16:
        return VK_FORMAT_R16_UINT;
    case WGPUVertexFormat_Uint16x2:
        return VK_FORMAT_R16G16_UINT;
    case WGPUVertexFormat_Uint16x4:
        return VK_FORMAT_R16G16B16A16_UINT;
    case WGPUVertexFormat_Sint16:
        return VK_FORMAT_R16_SINT;
    case WGPUVertexFormat_Sint16x2:
        return VK_FORMAT_R16G16_SINT;
    case WGPUVertexFormat_Sint16x4:
        return VK_FORMAT_R16G16B16A16_SINT;
    case WGPUVertexFormat_Unorm16:
        return VK_FORMAT_R16_UNORM;
    case WGPUVertexFormat_Unorm16x2:
        return VK_FORMAT_R16G16_UNORM;
    case WGPUVertexFormat_Unorm16x4:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case WGPUVertexFormat_Snorm16:
        return VK_FORMAT_R16_SNORM;
    case WGPUVertexFormat_Snorm16x2:
        return VK_FORMAT_R16G16_SNORM;
    case WGPUVertexFormat_Snorm16x4:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case WGPUVertexFormat_Float16:
        return VK_FORMAT_R16_SFLOAT;
    case WGPUVertexFormat_Float16x2:
        return VK_FORMAT_R16G16_SFLOAT;
    case WGPUVertexFormat_Float16x4:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case WGPUVertexFormat_Float32:
        return VK_FORMAT_R32_SFLOAT;
    case WGPUVertexFormat_Float32x2:
        return VK_FORMAT_R32G32_SFLOAT;
    case WGPUVertexFormat_Float32x3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case WGPUVertexFormat_Float32x4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case WGPUVertexFormat_Uint32:
        return VK_FORMAT_R32_UINT;
    case WGPUVertexFormat_Uint32x2:
        return VK_FORMAT_R32G32_UINT;
    case WGPUVertexFormat_Uint32x3:
        return VK_FORMAT_R32G32B32_UINT;
    case WGPUVertexFormat_Uint32x4:
        return VK_FORMAT_R32G32B32A32_UINT;
    case WGPUVertexFormat_Sint32:
        return VK_FORMAT_R32_SINT;
    case WGPUVertexFormat_Sint32x2:
        return VK_FORMAT_R32G32_SINT;
    case WGPUVertexFormat_Sint32x3:
        return VK_FORMAT_R32G32B32_SINT;
    case WGPUVertexFormat_Sint32x4:
        return VK_FORMAT_R32G32B32A32_SINT;
    case WGPUVertexFormat_Unorm10_10_10_2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case WGPUVertexFormat_Unorm8x4BGRA:
        return VK_FORMAT_B8G8R8A8_UNORM;
    default:
        return VK_FORMAT_UNDEFINED; // Default fallback
    }
}
//static inline VkDescriptorType toVulkanResourceType(uniform_type type) {
//    switch (type) {
//        case storage_texture2d:       [[fallthrough]];
//        case storage_texture2d_array: [[fallthrough]];
//        case storage_texture3d: 
//            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//        
//        case storage_buffer:
//            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//        case uniform_buffer:
//            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//
//        
//        case texture2d:       [[fallthrough]];
//        case texture2d_array: [[fallthrough]];
//        case texture3d:
//            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
//
//
//        case texture_sampler:
//            return VK_DESCRIPTOR_TYPE_SAMPLER;
//        case acceleration_structure:
//            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//        case combined_image_sampler:
//            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//        case uniform_type_undefined: [[fallthrough]];
//        case uniform_type_force32:   [[fallthrough]];
//        case uniform_type_enumcount: [[fallthrough]];
//        default:
//            rg_unreachable();
//    }
//    rg_unreachable();
//}

static inline VkVertexInputRate toVulkanVertexStepMode(WGPUVertexStepMode vsm) {
    switch (vsm) {
    case WGPUVertexStepMode_Vertex:
        return VK_VERTEX_INPUT_RATE_VERTEX;
    case WGPUVertexStepMode_Instance:
        return VK_VERTEX_INPUT_RATE_INSTANCE;
    
    case WGPUVertexStepMode_Undefined: //fallthrough
    default:
        return VK_VERTEX_INPUT_RATE_MAX_ENUM; // Default fallback
    }
}
static inline VkIndexType toVulkanIndexFormat(WGPUIndexFormat ifmt) {
    switch (ifmt) {
        case WGPUIndexFormat_Uint16:
            return VK_INDEX_TYPE_UINT16;
        case WGPUIndexFormat_Uint32:
            return VK_INDEX_TYPE_UINT32;
        default:
            return VK_INDEX_TYPE_UINT16; // Default fallback
    }
}

static inline VkShaderStageFlags toVulkanShaderStage(WGPUShaderStageEnum stage) {
    switch (stage){
        case WGPUShaderStageEnum_Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
        case WGPUShaderStageEnum_TessControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case WGPUShaderStageEnum_TessEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case WGPUShaderStageEnum_Geometry:       return VK_SHADER_STAGE_GEOMETRY_BIT;
        case WGPUShaderStageEnum_Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
        case WGPUShaderStageEnum_Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
        case WGPUShaderStageEnum_RayGen:         return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case WGPUShaderStageEnum_Miss:           return VK_SHADER_STAGE_MISS_BIT_KHR;
        case WGPUShaderStageEnum_ClosestHit:     return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case WGPUShaderStageEnum_AnyHit:         return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case WGPUShaderStageEnum_Intersect:      return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case WGPUShaderStageEnum_Callable:       return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case WGPUShaderStageEnum_Task:           return VK_SHADER_STAGE_TASK_BIT_EXT;
        case WGPUShaderStageEnum_Mesh:           return VK_SHADER_STAGE_MESH_BIT_EXT;
        default: rg_unreachable();
    }
}


static inline VkShaderStageFlags toVulkanShaderStageBits(WGPUShaderStage stage) {
    VkShaderStageFlags ret = 0;
    if(stage & WGPUShaderStage_Vertex){ret |= VK_SHADER_STAGE_VERTEX_BIT;}
    if(stage & WGPUShaderStage_TessControl){ret |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;}
    if(stage & WGPUShaderStage_TessEvaluation){ret |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;}
    if(stage & WGPUShaderStage_Geometry){ret |= VK_SHADER_STAGE_GEOMETRY_BIT;}
    if(stage & WGPUShaderStage_Fragment){ret |= VK_SHADER_STAGE_FRAGMENT_BIT;}
    if(stage & WGPUShaderStage_Compute){ret |= VK_SHADER_STAGE_COMPUTE_BIT;}
    if(stage & WGPUShaderStage_RayGen){ret |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;}
    if(stage & WGPUShaderStage_Miss){ret |= VK_SHADER_STAGE_MISS_BIT_KHR;}
    if(stage & WGPUShaderStage_ClosestHit){ret |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;}
    if(stage & WGPUShaderStage_AnyHit){ret |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;}
    if(stage & WGPUShaderStage_Intersect){ret |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;}
    if(stage & WGPUShaderStage_Callable){ret |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;}
    if(stage & WGPUShaderStage_Task){ret |= VK_SHADER_STAGE_TASK_BIT_EXT;}
    if(stage & WGPUShaderStage_Mesh){ret |= VK_SHADER_STAGE_MESH_BIT_EXT;}
    return ret;
}

static inline VkPipelineStageFlags toVulkanPipelineStageBits(WGPUShaderStage stage) {
    VkPipelineStageFlags ret = 0;
    if(stage & WGPUShaderStage_Vertex){
        ret |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_TessControl){
        ret |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_TessEvaluation){
        ret |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_Geometry){
        ret |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_Fragment){
        ret |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_Compute){
        ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if(stage & WGPUShaderStage_RayGen){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_Miss){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_ClosestHit){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_AnyHit){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_Intersect){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_Callable){
        ret |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    if(stage & WGPUShaderStage_Task){
        ret |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
    }
    if(stage & WGPUShaderStage_Mesh){
        ret |= VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
    }
    return ret;
}

static inline VkBlendFactor toVulkanBlendFactor(WGPUBlendFactor bf) {
    switch (bf) {
    case WGPUBlendFactor_Zero:
        return VK_BLEND_FACTOR_ZERO;
    case WGPUBlendFactor_One:
        return VK_BLEND_FACTOR_ONE;
    case WGPUBlendFactor_Src:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case WGPUBlendFactor_OneMinusSrc:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case WGPUBlendFactor_SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case WGPUBlendFactor_OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case WGPUBlendFactor_Dst:
        return VK_BLEND_FACTOR_DST_COLOR;
    case WGPUBlendFactor_OneMinusDst:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case WGPUBlendFactor_DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case WGPUBlendFactor_OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case WGPUBlendFactor_SrcAlphaSaturated:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case WGPUBlendFactor_Constant:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case WGPUBlendFactor_OneMinusConstant:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case WGPUBlendFactor_Src1:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case WGPUBlendFactor_OneMinusSrc1:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case WGPUBlendFactor_Src1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case WGPUBlendFactor_OneMinusSrc1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE; // Default fallback
    }
}

static inline VkBlendOp toVulkanBlendOperation(WGPUBlendOperation bo) {
    switch (bo) {
    case WGPUBlendOperation_Add:
        return VK_BLEND_OP_ADD;
    case WGPUBlendOperation_Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case WGPUBlendOperation_ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case WGPUBlendOperation_Min:
        return VK_BLEND_OP_MIN;
    case WGPUBlendOperation_Max:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_ADD; // Default fallback
    }
}

//static inline WGPUTextureFormat toWGPUPixelFormat(PixelFormat format) {
//    switch (format) {
//        case RGBA8:
//            return WGPUTextureFormat_RGBA8Unorm;
//        case RGBA8_Srgb:
//            return WGPUTextureFormat_RGBA8UnormSrgb;
//        case BGRA8:
//            return WGPUTextureFormat_BGRA8Unorm;
//        case BGRA8_Srgb:
//            return WGPUTextureFormat_BGRA8UnormSrgb;
//        case RGBA16F:
//            return WGPUTextureFormat_RGBA16Float;
//        case RGBA32F:
//            return WGPUTextureFormat_RGBA32Float;
//        case Depth24:
//            return WGPUTextureFormat_Depth24Plus;
//        case Depth32:
//            return WGPUTextureFormat_Depth32Float;
//        case GRAYSCALE:
//            assert(0 && "GRAYSCALE format not supported in Vulkan.");
//        case RGB8:
//            assert(0 && "RGB8 format not supported in Vulkan.");
//        default:
//            rg_unreachable();
//    }
//    return WGPUTextureFormat_Undefined;
//}

static inline VkDescriptorType extractVkDescriptorType(const WGPUBindGroupLayoutEntry* entry){
    if(entry->buffer.type == WGPUBufferBindingType_Storage){
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    if(entry->buffer.type == WGPUBufferBindingType_ReadOnlyStorage){
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    if(entry->buffer.type == WGPUBufferBindingType_Uniform){
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    if(entry->storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed){
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    if(entry->texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    if(entry->sampler.type != WGPUSamplerBindingType_BindingNotUsed){
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    if(entry->accelerationStructure){
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }
    rg_trap();
}

static inline VkAccessFlags extractVkAccessFlags(const WGPUBindGroupLayoutEntry* entry){
    if(entry->buffer.type == WGPUBufferBindingType_Storage){
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if(entry->buffer.type == WGPUBufferBindingType_ReadOnlyStorage){
        return VK_ACCESS_SHADER_READ_BIT;
    }
    if(entry->buffer.type == WGPUBufferBindingType_Uniform){
        return VK_ACCESS_SHADER_READ_BIT;
    }
    if(entry->storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed){
        return VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    }
    if(entry->texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
        return VK_ACCESS_SHADER_READ_BIT;
    }
    if(entry->sampler.type != WGPUSamplerBindingType_BindingNotUsed){
        return 0;
    }
    rg_trap();
}







#endif