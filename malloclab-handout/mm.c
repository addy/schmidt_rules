/*
The mem_init function models the virtual memory available to the heap as a
large, double-word aligned array of bytes. The bytes between mem_heap and mem_
brk represent allocated virtual memory. The bytes following mem_brk represent
unallocated virtual memory. The allocator requests additional heap memory by
calling the mem_sbrk function, which has the same interface as the system’s sbrk
function, as well as the same semantics, except that it rejects requests to shrink
the heap.░░░░░
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
* NOTE TO STUDENTS: Before you do anything else, please
* provide your team information in the following struct.
********************************************************/
team_t team = {
    /* Team name */
    "Jerry and the Sunshine Psychopaths",
    /* First member's full name */
    "Addison Shaw",
    /* First member's email address */
    "addisons@ksu.edu",
    /* Second member's full name (leave blank if none) */
    "David Maas",
    /* Second member's email address (leave blank if none) */
    "djmaas@ksu.edu"
};

/* Basic constants and macros */
#define WORD_SIZE 4 /* Word and header/footer size (bytes) */
#define DOUBLE_WORD_SIZE 8 /* Double word size (bytes) */
#define CHUNK_SIZE (1<<12) /* Initial useable heap size (4096 bytes) */
#define HEAP_BASE_OFFSET (2 * WORD_SIZE) /* The offset off of mem_heap_lo where the real heap starts */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET_AS_WORD_POINTER(p) (*(unsigned int *)(p))
#define PUT_IN_WORD_POINTER(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET_AS_WORD_POINTER(p) & ~0x7)
#define IS_ALLOCATED(p) (GET_AS_WORD_POINTER(p) & 0x1)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void* getHeaderPointer(char* blockPointer);
static void* getFooterPointer(char* blockPointer);
static void* getNextBlockPointer(char* blockPointer);
static void* getPreviousBlockPointer(char* blockPointer);
static void* extend_heap(size_t words);
static void* place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);

/*Converted function - return the header of the pointer*/
static void* getHeaderPointer(char* blockPointer)
{
    void* ret = blockPointer - WORD_SIZE;
    return ret;
}

/*Converted function - return the footer of the pointer*/
static void* getFooterPointer(char* blockPointer)
{
    void* ret = blockPointer + GET_SIZE(getHeaderPointer(blockPointer)) - DOUBLE_WORD_SIZE;
    return ret;
}

/*Converted function - return the next block pointer*/
static void* getNextBlockPointer(char* blockPointer)
{
    return blockPointer + GET_SIZE(blockPointer - WORD_SIZE);
}

/*Converted function - return the previous block pointer*/
static void* getPreviousBlockPointer(char* blockPointer)
{
    return blockPointer - GET_SIZE(blockPointer - DOUBLE_WORD_SIZE);
}

/*Adds onto the current heap size by the necessary word size*/
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    /* Ensure we extend by an even number of words */
    if ((words % 2) != 0)
    {
        words++;
    }

    size = words * WORD_SIZE;

    bp = mem_sbrk(size);

    if ((int)bp == -1)
    {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(size, 0)); /* Free block header */
    PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(size, 0)); /* Free block footer */
    PUT_IN_WORD_POINTER(getHeaderPointer(getNextBlockPointer(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    //return coalesce(bp);
    return bp;
}

/*Searches for free blocks to increase the available size*/
static void* coalesce(void *bp)
{
    size_t prev_alloc = IS_ALLOCATED(getFooterPointer(getPreviousBlockPointer(bp)));
    size_t next_alloc = IS_ALLOCATED(getHeaderPointer(getNextBlockPointer(bp)));
    size_t size = GET_SIZE(getHeaderPointer(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(getHeaderPointer(getNextBlockPointer(bp)));
        PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(size, 0));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(getHeaderPointer(getPreviousBlockPointer(bp)));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(size, 0));
        PUT_IN_WORD_POINTER(getHeaderPointer(getPreviousBlockPointer(bp)), PACK(size, 0));
        bp = getPreviousBlockPointer(bp);
    }
    else { /* Case 4 */
        size += GET_SIZE(getHeaderPointer(getPreviousBlockPointer(bp))) +
        GET_SIZE(getFooterPointer(getNextBlockPointer(bp)));
        PUT_IN_WORD_POINTER(getHeaderPointer(getPreviousBlockPointer(bp)), PACK(size, 0));
        PUT_IN_WORD_POINTER(getFooterPointer(getNextBlockPointer(bp)), PACK(size, 0));
        bp = getPreviousBlockPointer(bp);
    }

    return bp;
}

/*searches for a valid placement and returns the pointer to its position*/
static void* find_fit(size_t adjustedSize)
{
    void* bp =  mem_heap_lo() + HEAP_BASE_OFFSET;

    /* first fit search */
    while (GET_SIZE(getHeaderPointer(bp)) > 0)
    {
        if (!IS_ALLOCATED(getHeaderPointer(bp)) && (adjustedSize <= GET_SIZE(getHeaderPointer(bp))))
        {
            return bp;
        }

        bp = getNextBlockPointer(bp);
    }

    return NULL; /* no fit */
}

/*responsible for placing the payload into the heap*/
static void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(getHeaderPointer(bp));
    if ((csize - asize) >= (2*DOUBLE_WORD_SIZE))
    {
        PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(asize, 1));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(asize, 1));
        bp = getNextBlockPointer(bp);
        PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(csize-asize, 0));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(csize-asize, 0));

        return bp;
    }
    else
    {
        PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(csize, 1));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(csize, 1));

        return NULL;
    }
}

/* 
* mm_init - initialize the malloc package.
*/
int mm_init(void)
{
    mem_init();
    void* base = mem_heap_lo();

    /* Create the initial empty heap */
    if ((base = mem_sbrk(4*WORD_SIZE)) == (void *)-1)
    {
        return -1;
    }

    PUT_IN_WORD_POINTER(base, 0); /* Alignment padding */
    PUT_IN_WORD_POINTER(base + (1*WORD_SIZE), PACK(DOUBLE_WORD_SIZE, 1)); /* Prologue header */
    PUT_IN_WORD_POINTER(base + (2*WORD_SIZE), PACK(DOUBLE_WORD_SIZE, 1)); /* Prologue footer */
    PUT_IN_WORD_POINTER(base + (3*WORD_SIZE), PACK(0, 1)); /* Epilogue header */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNK_SIZE / WORD_SIZE) == NULL)
    {
        return -1;
    }

    return 0;
}


/* 
* mm_malloc - Allocate a block by incrementing the brk pointer.
*     Always allocate a block whose size is a multiple of the alignment.
*/
void *mm_malloc(size_t size)
{
    size_t adjustedSize; /* Adjusted block size */
    size_t extendSize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
    {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DOUBLE_WORD_SIZE)
    {
        adjustedSize = 2*DOUBLE_WORD_SIZE;
    }
    else
    {
        adjustedSize = DOUBLE_WORD_SIZE * ((size + (DOUBLE_WORD_SIZE) + (DOUBLE_WORD_SIZE-1)) / DOUBLE_WORD_SIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(adjustedSize)) != NULL)
    {
        place(bp, adjustedSize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendSize = MAX(adjustedSize,CHUNK_SIZE);
    if ((bp = extend_heap(extendSize / WORD_SIZE)) == NULL)
    {
        return NULL;
    }
    
    place(bp, adjustedSize);
    return bp;
}

/*
* mm_free - Freeing a block does nothing.
*/
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(getHeaderPointer(ptr));
    PUT_IN_WORD_POINTER(getHeaderPointer(ptr), PACK(size, 0));
    PUT_IN_WORD_POINTER(getFooterPointer(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
* mm_realloc - Implemented simply in terms of mm_malloc and mm_free
*/
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
    {
        return NULL;
    }
    copySize = GET_SIZE(getHeaderPointer(oldptr));
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*Checks consistency of heap
    -Checks invariants
    -Prints error messages
    -Returns non-zero value if heap is consistent
*/
static int mm_check(void)
{
    void* ptr;
    size_t* heap_top =  mem_heap_hi();
    size_t* heap_bot =  mem_heap_lo();
    size_t* heap_size = mem_heapsize();

    assert(heap_bot < (heap_top + heap_size));
    
    for(ptr = heap_top; GET_SIZE(getHeaderPointer(ptr)) > 0; ptr = getNextBlockPointer(ptr)) 
    {      
        if ((int)ptr > (int)heap_top)
        {
            printf("ERROR: Top of heap exceeded by pointer\n top: %p,\n pointer: %p\n", heap_top, ptr);
        }
        if ((int)ptr > (int)heap_bot || (int)ptr < (int)heap_top)
        {        
            printf("Error: pointer %p out of heap bounds\n", ptr);
        }
        if (IS_ALLOCATED(getHeaderPointer(ptr)) == 0 && IS_ALLOCATED(getHeaderPointer(getNextBlockPointer(ptr))) == 0)
        {
            printf("Error: Empty stacked blocks %p and %p not coalesced\n", ptr, getNextBlockPointer(ptr));
        }
        if ((size_t)ptr % 8)
        {
            printf("Error: %p misaligned our headers and payload\n", ptr);
        }
    }

    int i;
    for (i = 0; i < 100; i++) 
    {
        ptr = (char*)GET_AS_WORD_POINTER(heap_bot + (i * WSIZE));
        while (ptr != NULL) 
        {
            assert(!IS_ALLOCATED(ptr));
            ptr = (char*)GET_AS_WORD_POINTER(ptr);
        }
    }

    return 0;
}