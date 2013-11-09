/*
* mm-naive.c - The fastest, least memory-efficient malloc package.
* 
* In this naive approach, a block is allocated by simply incrementing
* the brk pointer.  A block is pure payload. There are no headers or
* footers.  Blocks are never coalesced or reused. Realloc is
* implemented directly using mm_malloc and mm_free.
*
* NOTE TO STUDENTS: Replace this header comment with your own header
* comment that gives a high level description of your solution.
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

static void* getHeaderPointer(char* blockPointer)
{
    void* ret = blockPointer - WORD_SIZE;
    if(ret < mem_heap_lo() || ret > mem_heap_hi())
    {
        printf("!!!!!!! WARNING: getHeaderPointer returning pointer outside heap!\n");
        printf("0x%X < 0x%X || 0x%X > 0x%X",
            (unsigned int)ret, (unsigned int)mem_heap_lo(), (unsigned int)ret, (unsigned int)mem_heap_hi()
        );
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    return ret;
}

static void* getFooterPointer(char* blockPointer)
{
    void* ret = blockPointer + GET_SIZE(getHeaderPointer(blockPointer)) - DOUBLE_WORD_SIZE;
    if(ret < mem_heap_lo() || ret > mem_heap_hi())
    {
        printf("!!!!!!! WARNING: getHeaderPointer returning pointer outside heap!\n");
        printf("0x%X < 0x%X || 0x%X > 0x%X",
            (unsigned int)ret, (unsigned int)mem_heap_lo(), (unsigned int)ret, (unsigned int)mem_heap_hi()
            );
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    return ret;
}

static void* getNextBlockPointer(char* blockPointer)
{
    return blockPointer + GET_SIZE(blockPointer - WORD_SIZE);
}

static void* getPreviousBlockPointer(char* blockPointer)
{
    return blockPointer - GET_SIZE(blockPointer - DOUBLE_WORD_SIZE);
}

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

    printf("---- extend_heap(%d) allocating %d bytes\n", words, size);

    bp = mem_sbrk(size);
    printf("bp = mem_sbrk(%d) = 0x%X\n", size, (unsigned int)bp);

    if ((int)bp == -1)
    {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(size, 0)); /* Free block header */
    PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(size, 0)); /* Free block footer */
    PUT_IN_WORD_POINTER(getHeaderPointer(getNextBlockPointer(bp)), PACK(0, 1)); /* New epilogue header */

    printf("New block:\nbp = 0x%X\nheader = 0x%X\nfooter = 0x%X\nsize(hdr/ftr) = %d/%d\n",
        (unsigned int)bp,
        (unsigned int)getHeaderPointer(bp),
        (unsigned int)getFooterPointer(bp),
        GET_SIZE(getHeaderPointer(bp)),
        GET_SIZE(getFooterPointer(bp)));

    /* Coalesce if the previous block was free */
    //return coalesce(bp);
    return bp;
}

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

static void *find_fit(size_t adjustedSize)
{
    printf("---- find_fit(%d)\n", adjustedSize);
    void* bp =  mem_heap_lo() + HEAP_BASE_OFFSET;

    /* first fit search */
    while (GET_SIZE(getHeaderPointer(bp)) > 0)
    {
        if (!IS_ALLOCATED(getHeaderPointer(bp)) && (adjustedSize <= GET_SIZE(getHeaderPointer(bp))))
        {
            printf("---- found fit %d && %d <= %d\n", IS_ALLOCATED(getHeaderPointer(bp)), adjustedSize, GET_SIZE(getHeaderPointer(bp)));
            fflush(stdout);
            return bp;
        }

        bp = getNextBlockPointer(bp);
    }
    printf("---- find_fit(%d) failed to find fit\n", adjustedSize);
    fflush(stdout);
    return NULL; /* no fit */
}

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
        printf("no errors in place\n");
        fflush(stdout);
        return bp;
    }
    else
    {
        PUT_IN_WORD_POINTER(getHeaderPointer(bp), PACK(csize, 1));
        PUT_IN_WORD_POINTER(getFooterPointer(bp), PACK(csize, 1));
        printf("need to split.\n");
        fflush(stdout);
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
    printf("----- mm_malloc(%d)\n", size);

    size_t adjustedSize; /* Adjusted block size */
    size_t extendSize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
    {
        printf("----- mm_malloc: Returning NULL (No size allocation)\n");
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
        printf("----- mm_malloc: Returning %X (Found fit)\n", (unsigned int)bp);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendSize = MAX(adjustedSize,CHUNK_SIZE);
    if ((bp = extend_heap(extendSize / WORD_SIZE)) == NULL)
    {
        printf("----- mm_malloc: Returning NULL (No Fit + Couldn't extend heap :( )\n");
        return NULL;
    }
    
    place(bp, adjustedSize);

    printf("----- mm_malloc: Returning %X (Heap Extended)\n", (unsigned int)bp);
    return bp;
}

/*
* mm_free - Freeing a block does nothing.
*/
void mm_free(void *ptr)
{
    printf("----- mm_free(%X)\n", (unsigned int)ptr);
    size_t size = GET_SIZE(getHeaderPointer(ptr));
    PUT_IN_WORD_POINTER(getHeaderPointer(ptr), PACK(size, 0));
    PUT_IN_WORD_POINTER(getFooterPointer(ptr), PACK(size, 0));
    coalesce(ptr);
    printf("----- mm_free: Returning.\n");
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}