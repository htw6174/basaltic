#ifndef HTW_CORE_H_INCLUDED
#define HTW_CORE_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* linux kernal style typedefs */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/* Debug utilities */
#define HTW_STOPWATCH(x) { clock_t start = clock(); \
                    x; \
                    clock_t end = clock(); \
                    float seconds = (float)(end - start) / CLOCKS_PER_SEC; \
                    printf("%s finished in %.3f seconds / %li ticks\n", #x, seconds, end - start); }

static inline void htw_printArray(FILE* dest, void* data, u32 size, u32 count, u32 valuesPerLine, char* format) {
    for (int i = 0; i < count; i++) {
        void* p = (char*)data + (i * size);
        fprintf(dest, format, *(unsigned int*)p);
        // end of line
        if (i % valuesPerLine == (valuesPerLine - 1)) fprintf(dest, "\n");
    }
    // if last line isn't filled, add a newline
    if (count % valuesPerLine != 0) fprintf(dest, "\n");
}

// FIXME: same as printArray right now
static inline void htw_printVectorArray(FILE* dest, void* data, u32 dim, u32 count, u32 vecsPerLine, char* format) {
    for (int i = 0; i < count; i++) {
        void* p = (char*)data + (i * sizeof(float));
        fprintf(dest, format, *(unsigned int*)p);
        // end of line
        if (i % vecsPerLine == (vecsPerLine - 1)) fprintf(dest, "\n");
    }
    // if last line isn't filled, add a newline
    if (count % vecsPerLine != 0) fprintf(dest, "\n");
}

/* String and char utilities */
// Corresponds to the index of 0 in the standard character set
#define INT_CHAR_OFFSET 48
#define charToInt(c) (int)c - INT_CHAR_OFFSET
#define intToChar(i) (char)i + INT_CHAR_OFFSET

static inline int htw_strToInt(const char *str) {
    int sum = 0;
    int i = 0;
    char c;
    while((c = str[i++]) != '\0') {
        sum = (sum * 10) + charToInt(c);
    }
    return sum;
}

/* Math utilities */

#define PI 3.141592f
#define DEG_TO_RAD 0.017453f

inline int min_int(int a, int b) {
    return a < b ? a : b;
}

inline int max_int(int a, int b) {
    return a > b ? a : b;
}

// returns the smallest multiple of alignment which is >= value
int htw_align(int value, int alignment);

// returns the smallest power of 2 which is >= value
unsigned int htw_nextPow(unsigned int value);

float lerp(float a, float b, float progress);

int lerp_int(int a, int b, double prog);

double inverseLerp_int(int a, int b, int val);

int remap_int(int val, int oldMin, int oldMax, int newMin, int newMax);

/* Simple file handling utilities */
static const int HTW_FILE_LOAD_MAX_LENGTH = 1024*1024;

// Simply free the result when you're done with it
static char* htw_load(const char* path);

static char* htw_load(const char* path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    char* contents = calloc(HTW_FILE_LOAD_MAX_LENGTH, sizeof(char));
    size_t i = 0;
    char c;
    while((c = getc(fp)) != EOF) {
        contents[i++] = c;
        if (i == HTW_FILE_LOAD_MAX_LENGTH) {
            contents[i - 1] = 0;
            fprintf(stderr, "Reached max readable length for file %s\n", path);
            break;
        }
    }

    //printf("Loaded %lu bytes from %s\n", i, path);

    fclose(fp);
    return contents;
}

// TODO
/** Generic object pools
 * Allows for frequent reuse of objects without reallocation of memory. Creating a pool allocates enough space for n objects once. The pool keeps track of which pool items are in use/not in use. Requesting a new object from the pool returns the pointer to an unused item, and marks that item as in use. Telling the pool to destroy an object marks it as unused. Destroying the pool frees all items from memory.
 */
typedef struct {
    int capacity;
    int itemSize;
    void *poolItems;
} Pool;

Pool *createPool(int itemSize, int capacity);

/**
 * @brief Free all memory used by Pool [pool]
 *
 * @param pool pool to destroy
 * @return >= 0: number of pool objects still in use. < 0: error
 */
int destroyPool(Pool *pool);
void *getNewPoolItem(Pool *pool);
int destroyPoolItem(Pool *pool, void *item);

#endif // HTW_CORE_H_INCLUDED
