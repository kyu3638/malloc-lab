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
    "RED - TEAM 2",
    /* First member's full name */
    "SEUNGKYU JEON",
    /* First member's email address */
    "kyu3638@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 8             /* Word and header/footer size (bytes) */
#define DSIZE 16            /* Double word size (bytes) */
#define MINIMUM 32          /* Initial Prologue block size, header, footer, PREC, SUCC */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* Free List 상에서의 이전, 이후 블록의 포인터를 리턴한다. */
#define PREC_FREEP(bp) (*(void **)(bp))         // 이전 블록의 bp
#define SUCC_FREEP(bp) (*(void **)(bp + WSIZE)) // 이후 블록의 bp

static char *heap_listp = NULL;
static char *free_listp = NULL;

static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
void removeBlock(void *bp);
void putFreeBlock(void *bp);

/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                              /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(MINIMUM, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), NULL);             /* Predecessor */
    PUT(heap_listp + (3 * WSIZE), NULL);             /* Successor */
    PUT(heap_listp + (4 * WSIZE), PACK(MINIMUM, 1)); /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));       /* Epilogue header */
    free_listp = heap_listp + (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reps. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place th block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{
    /* Explicit - First fit */
    void *bp;
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCC_FREEP(bp))
    {
        if (asize <= GET_SIZE(HDRP(bp)))
            return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    removeBlock(bp);
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        // free list 첫번째에 분할된 블럭을 연결
        putFreeBlock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void removeBlock(void *bp)
{
    /* Free list의 첫번째 블록 없앨 때 */
    if (bp == free_listp)
    {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    /* Free list 안에서 없앨 때 */
    else
    {
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}

/*
    putFreeBlock(bp) : 새로 반환되거나 생성된 가용 블록을 free list의 첫 부분에 넣는다.
*/
void putFreeBlock(void *bp)
{
    PREC_FREEP(bp) = NULL;
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1 직전, 직후 블록이 모두 할당 -> 해당 블록만 free list에 넣어주면 된다 */
    if (prev_alloc && next_alloc)
    {
        putFreeBlock(bp);
        return bp;
    }
    /* Case 2 직전 블록 할당, 직후 블록 가용 */
    else if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* Case 3 직전 블록 가용, 직후 블록 할당 */
    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* Case 4 */
    else
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // 연결된 새 가용 블록을 free list에 추가한다.
    putFreeBlock(bp);

    return bp;
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
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
