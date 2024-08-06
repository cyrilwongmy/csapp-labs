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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* $begin mallocmacros */
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* Basic constants and macros */
#define WSIZE                                                                  \
  4 /* Word and header/footer size (bytes) */ // line:vm:mm:beginconst
#define DSIZE 8                               /* Double word size (bytes) */
#define CHUNKSIZE                                                              \
  (1 << 12) /* Extend heap by this amount (bytes) */ // line:vm:mm:endconst

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define CLASS_SIZE 20

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // line:vm:mm:get
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // line:vm:mm:put

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)                      // line:vm:mm:hdrp
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // line:vm:mm:ftrp

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1) // line:vm:mm:getalloc

/* block ptr bp which points to the first bytes of the payload
Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)                                                          \
  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // line:vm:mm:nextblkp
#define PREV_BLKP(bp)                                                          \
  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // line:vm:mm:prevblkp
/* $end mallocmacros */

/* Give block ptr bp, return its previous block in the linkedlist */
#define GET_PREV(bp) (*(unsigned int **)(bp))
#define GET_NEXT(bp) (*((unsigned int **)((unsigned int *)(bp) + 1)))

#define SET_PREV(bp, prev) (*((void **)(bp)) = prev)
#define SET_NEXT(bp, next) (*((void **)(bp) + 1) = next)

#define GET_LIST_BY_INDEX(index) (unsigned int **)(size_class_listp + index)

/* Given block ptr bp,
    compute the next block in the size class linkedlist
 */
#define NEXT_BLK_IN_LIST(bp) ((char *)bp +)

/* Private global variables */
static char *heap_listp = 0; /* Pointer to second block of prologue */
static unsigned int *size_class_listp = 0; /* Pointer to size class list */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void delete(void *bp); /* Delete bp block from the linked list */
static void insert(void *bp,
                   size_t size); /* Insert the bp block into the linked list */
static unsigned int find_list(size_t size); /* Find the index of the list */
static void *find_in_list(size_t size,
                          unsigned int *list); /* Find the index of the list */

static void printfList() {
  printf("[printfList] is called\n");
  for (int i = 0; i < CLASS_SIZE; ++i) {
    unsigned int **list_ptr = GET_LIST_BY_INDEX(i);
    printf("[printfList] list %d: head = %p\n", i, *list_ptr);
    // go through the list
    if (*list_ptr != NULL) {
      void *bp = *list_ptr;
      printf("[printfList] list %d traversal started: \n", i);
      // printf("[printfList] block address: %p, size: %u, free: %d\n", bp,
      //        GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
      for (; bp != NULL; bp = GET_NEXT(bp)) {
        printf("[printfList] block address: %p\n", bp);
        // printf("[printfList] block address: %p, size: %u, free: %d\n", bp,
        //        GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
        // printf("[printfList] block address: %p, size: %u\n", bp,
        //        GET_SIZE(HDRP(bp)));
      }
      printf("[printfList] list %d traversal end\n", i);
    }
  }
  printf("[printfList] end\n");
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  printf("\nmm_init is called\n");
  if ((heap_listp = mem_sbrk((CLASS_SIZE + 4) * WSIZE)) == (void *)-1) {
    return -1;
  }
  printf("address of heap_listp: %p\n", heap_listp);

  size_class_listp = (unsigned int *)heap_listp;
  printf("address of size_class_listp: %p\n", size_class_listp);
  for (int i = 0; i < CLASS_SIZE; ++i) {
    PUT(heap_listp + i * WSIZE, 0);
  }

  heap_listp += CLASS_SIZE * WSIZE;

  PUT(heap_listp, 0);                          /* Alignment padding */
  PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1)); /* Prologue header */
  printf("[mm_init] prologue header address: %p\n", heap_listp + 1 * WSIZE);
  PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1)); /* Prologue footer */
  printf("[mm_init] prologue footer address: %p\n", heap_listp + 2 * WSIZE);
  PUT(heap_listp + 3 * WSIZE, PACK(0, 1));     /* Epilogue header */
  printf("[mm_init] epilogue header address: %p\n", heap_listp + 3 * WSIZE);
  heap_listp += DSIZE;
  printf("address of heap_listp after init: %p\n", heap_listp);

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
    return -1;
  }
  printfList();
  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
  printf("\n[mm_malloc] is called with size: %lu\n", size);
  printfList();
  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;

  /* $end mmmalloc */
  if (heap_listp == 0) {
    mm_init();
  }
  /* $begin mmmalloc */
  /* Ignore spurious requests */
  if (size == 0)
    return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE) // line:vm:mm:sizeadjust1
    // 1st DSIZE for data, 2nd DSIZE for prev, next ptrs, 3rd DSIZE for hdr and
    // ftr
    asize = 3 * DSIZE; // line:vm:mm:sizeadjust2
  else
    // ajdust block size to be aligned with DSIZE
    asize = DSIZE * ((size + (3 * DSIZE) + (DSIZE - 1)) /
                     DSIZE); // line:vm:mm:sizeadjust3

  printf("[mm_malloc] adjusted block size: asize: %lu\n", asize);
  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL) { // line:vm:mm:findfitcall
    place(bp, asize);                   // line:vm:mm:findfitplace
    printf("[mm_malloc] find fit in the free list\n");
    printfList();
    printf("[mm_malloc] end with block bp %p\n", bp);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize, CHUNKSIZE); // line:vm:mm:growheap1
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
    printf("[mm_malloc] extend heap failed\n");
    return NULL; // line:vm:mm:growheap2
  }
  printf("[mm_malloc] extend heap to %lu bytes\n", extendsize);
  place(bp, asize); // line:vm:mm:growheap3
  printfList();
  printf("[mm_malloc] end with block bp %p\n", bp);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  /* $end mmfree */
  printf("\n[mm_free] is called with block %p\n", bp);
  printfList();
  if (bp == 0) {
    printf("[mm_free] block is NULL\n");
    return;
  }

  /* $begin mmfree */
  size_t size = GET_SIZE(HDRP(bp));
  /* $end mmfree */
  if (heap_listp == 0) {
    printf("[mm_free] heap_listp is NULL\n");
    mm_init();
  }
  /* $begin mmfree */

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
  printfList();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0) {
    mm_free(ptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if (ptr == NULL) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if (!newptr) {
    return 0;
  }

  /* Copy the old data. */
  oldsize = GET_SIZE(HDRP(ptr));
  if (size < oldsize)
    oldsize = size;
  memcpy(newptr, ptr, oldsize);

  /* Free the old block. */
  mm_free(ptr);

  return newptr;
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
  /* $end mmfirstfit */

  /* $begin mmfirstfit */
  /* First-fit search */
  void *bp;

  unsigned int index = find_list(asize);
  // search through all lists
  // search until index >= 20, no fit return NULL
  for (; index < 20; ++index) {
    printf("[find_fit] search in list %d\n", index);
    // search within the list
    bp = find_in_list(asize, *GET_LIST_BY_INDEX(index));
    if (bp) {
      printf("[find_fit] find fit in list returns %p\n", bp);
      return bp;
    }
    printf("[find_fit] find_in_list returns %p\n", bp);
  }

  return NULL; /* No fit */
}
/* $end mmfirstfit */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) {
  printf("[extend_heap] is called with words: %lu\n", words);
  char *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words + 1) * WSIZE
                     : words * WSIZE; // line:vm:mm:beginextend
  if ((long)(bp = mem_sbrk(size)) == -1) {
    printf("[extend_heap] mem_sbrk failed\n");
    return NULL; // line:vm:mm:endextend
  }

  /* Initialize free block header/footer and the epilogue header */
  /* Free block header */ // line:vm:mm:freeblockhdr
  PUT(HDRP(bp), PACK(size, 0));
  /* Free block footer */ // line:vm:mm:freeblockftr
  PUT(FTRP(bp), PACK(size, 0));
  /* New epilogue header */ // line:vm:mm:newepihdr
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
  SET_NEXT(bp, NULL);
  SET_PREV(bp, NULL);
  printf("[extend_heap] new block %p with size %zu\n", bp, size);

  /* Coalesce if the previous block was free */
  return coalesce(bp); // line:vm:mm:returnblock
}
/* $end mmextendheap */

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp) {
  printf("[coalesce] is called with block %p\n", bp);
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  printf("[coalesce] prev_alloc: %zu, prev_size: %u, prev_addr: %p\n",
         prev_alloc, GET_SIZE(FTRP(PREV_BLKP(bp))), PREV_BLKP(bp));
  printf("[coalesce] next_alloc: %zu, next_size: %u, next_addr: %p\n",
         next_alloc, GET_SIZE(HDRP(NEXT_BLKP(bp))), NEXT_BLKP(bp));

  if (prev_alloc && next_alloc) { /* Case 1 */
    printf("[coalesce] case 1\n");
  }
  else if (prev_alloc && !next_alloc) { /* Case 2 */
    printf("[coalesce] case 2\n");
    delete (NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  else if (!prev_alloc && next_alloc) { /* Case 3 */
    printf("[coalesce] case 3\n");
    delete (PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  else { /* Case 4 */
    printf("[coalesce] case 4\n");
    delete (NEXT_BLKP(bp));
    delete (PREV_BLKP(bp));
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  insert(bp, size);
  return bp;
}
/* $end mmfree */

static void delete(void *bp) {
  printf("[delete] start to delete block %p\n", bp);
  // get its prev and next block's address
  void *prev = GET_PREV(bp);
  void *next = GET_NEXT(bp);
  printf("[delete] prev address is %p\n", prev);
  printf("[delete] next address is %p\n", next);

  // handle when prev and next is NULL and also maintain the ptr in size class
  // list
  if (prev == NULL && next == NULL) {
    printf("[delete] prev and next are both NULL\n");
    // only node in the list is bp
    unsigned int index = find_list(GET_SIZE(HDRP(bp)));
    printf("[delete] delete block %p with size %u from list %d\n", bp, GET_SIZE(HDRP(bp)), index);
    unsigned int **list_ptr = GET_LIST_BY_INDEX(index);
    *list_ptr = NULL;
  } else if (prev == NULL && next != NULL) {
    printf("[delete] prev is NULL\n");
    unsigned int index = find_list(GET_SIZE(HDRP(bp)));
    unsigned int **list_ptr = GET_LIST_BY_INDEX(index);
    *list_ptr = (unsigned int *)(next);
    SET_PREV(next, NULL);
  } else if (prev != NULL && next == NULL) {
    printf("[delete] next is NULL\n");
    SET_NEXT(prev, NULL);
  } else {
    printf("[delete] prev and next are both not NULL\n");
    SET_NEXT(prev, next);
    SET_PREV(next, prev);
  }

  SET_PREV(bp, NULL);
  SET_NEXT(bp, NULL);
}

static void insert(void *bp, size_t size) {
  unsigned int index = find_list(size);

  unsigned int **list_ptr = GET_LIST_BY_INDEX(index);
  printf("[insert] begin to insert block %p with size %zu into list %d\n", bp, size,
         index);

  if (*list_ptr == NULL) {
    *list_ptr = bp;
    SET_NEXT(bp, NULL);
    SET_PREV(bp, NULL);
    printf("[insert] insert block %p with size %zu into empty list %d\n", bp,
           size, index);
    printf("[insert] after update block %p, next: %p, prev: %p\n", bp, GET_NEXT(bp), GET_PREV(bp));
  } else {
    // insert into the head of the list
    unsigned int *head = *list_ptr;
    *list_ptr = bp;
    SET_NEXT(bp, head);
    SET_PREV(head, bp);
    printf("[insert] insert block %p with size %zu into list %d\n", bp, size,
           index);
  }
}

/* Find the index of the list */
static unsigned int find_list(size_t size) {
  // Find the size class of asize belongs to
  printf("[find_list] start to find list for size %zu\n", size);
  int index = -1;
  for (; index < 20 && size > 0; ++index) {
    size = size >> 1;
  }

  printf("[find_list] find list index: %d\n", index);
  assert(index > -1);
  return index;
}

/* Find the index of the list */
static void *find_in_list(size_t size, unsigned int *list) {
  printf("[find_in_list] start to find in list %p for size %zu\n", list, size);
  if (list == 0) {
    printf("[find_in_list] list %p is empty return imm\n", list);
    return NULL;
  }

  for (; list != NULL; list = GET_NEXT(list)) {
    printf("[find_in_list] block address: %p, size: %u\n", list,
           GET_SIZE(HDRP(list)));
    if (GET_SIZE(HDRP(list)) >= size) {
      return list;
    }
  }
  return NULL;
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size (16
 * bytes)
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
  printf("[place] is called with block %p, requested size %lu\n", bp, asize);
  size_t csize = GET_SIZE(HDRP(bp));
  printf("[place] block csize %lu\n", csize);

  if ((csize - asize) >= (2 * DSIZE)) {
    delete(bp);
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    insert(bp, csize - asize);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
    printf("[place] split block %p with other block size %lu\n", bp, csize - asize);
  } else {
    delete(bp);
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
    printf("[place] no split\n");
  }
}
/* $end mmplace */