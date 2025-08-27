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
    "hoho",
    /* First member's full name */
    "son",
    /* First member's email address */
    "shh3350@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//macro
#define WSIZE 4 //워드사이즈4
#define DSIZE 8 //워드더블사이즈8
#define CHUNKSIZE (1<<12) //힙 확장

#define MAX(x, y) ((x)>(y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) //크기와 할당된 비트 워드에 pack

//주소 p의 워드 읽기, 쓰기
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

//주소 p의 크기, 할당 필드 읽기
#define GET_SIZE(p) ((GET(p) & ~0x7))
#define GET_ALLOC(p) ((GET(p) & 0x1))

//bp(블록 포인터)의 헤더, 풋터의 compute 주소
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

//bp(블록 포인터)의 이전, 다음 블록의 compute 주소
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//명시 리스트 포인터
#define PTRSIZE (sizeof(void*))
#define PRED(bp) (*(void **)(bp))
#define SUCC(bp) (*(void **)((char *)(bp) + PTRSIZE))
#define MIN_FREEBLK  ( (2*WSIZE + 2*PTRSIZE) < (2*DSIZE) ? (2*DSIZE) : (2*WSIZE + 2*PTRSIZE) )

static char *heap_listp;
static void *fit_bp;
static void *head_bp = NULL; // 명시적 가용 리스트의 머리

static void *find_fit(size_t asize) {
    void *bp;
    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }
    return NULL;
}

static void *find_fit_exp(size_t asize) {
    void *bp = head_bp;
    for (; bp; bp = SUCC(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }
    return NULL;
}

static void insert_front(void *bp) {
    /* free 블록 메타( pred/succ )는 payload에 기록 */
    PRED(bp) = NULL;
    SUCC(bp) = head_bp;
    if (head_bp) PRED(head_bp) = bp;
    head_bp = bp;
}

static void remove_free(void *bp) {
    void *prev = PRED(bp);
    void *next = SUCC(bp);
    if (prev) SUCC(prev) = next; else head_bp = next;
    if (next) PRED(next) = prev;
    PRED(bp) = SUCC(bp) = NULL;
}

static void place(void *bp, size_t asize){
    size_t originsize = GET_SIZE(HDRP(bp));

    remove_free(bp);

    if (originsize - asize >= MIN_FREEBLK){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        void *new_bp = NEXT_BLKP(bp);
        PUT(HDRP(new_bp), PACK(originsize - asize, 0));
        PUT(FTRP(new_bp), PACK(originsize - asize, 0));

        insert_front(new_bp);
    } else {
        PUT(HDRP(bp), PACK(originsize, 1));
        PUT(FTRP(bp), PACK(originsize, 1));
    }
}


static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size       = GET_SIZE(HDRP(bp));

    /* 양 옆이 free면 먼저 리스트에서 제거 후 합친다 */
    if (!prev_alloc) {
        void *prev = PREV_BLKP(bp);
        remove_free(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(size, 0));
        PUT(FTRP(bp),   PACK(size, 0));
        bp = prev;
    }
    if (!next_alloc) {
        void *next = NEXT_BLKP(bp);
        remove_free(next);
        size += GET_SIZE(HDRP(next));
        PUT(HDRP(bp),   PACK(size, 0));
        PUT(FTRP(next), PACK(size, 0));
    }

    /* 최종 병합 블록을 free-list 머리에 삽입 */
    insert_front(bp);
    return bp;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    bp = mem_sbrk(size);
    if (bp == (void *)-1) return NULL;

    PUT(HDRP(bp), PACK(size, 0)); //새로 만든 블록의 헤더
    PUT(FTRP(bp), PACK(size, 0)); //새로 만든 블록의 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //헤더(에필로그)
    
    return coalesce(bp);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;
    PUT(heap_listp, 0);                          //정렬 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //프롤로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //프로롤그 풋터
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     //에필로그 헤더
    heap_listp += (2*WSIZE);
    head_bp = NULL;
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{

    size_t asize; //조정된 블록 사이즈
    size_t extendsize; //확장할 힙의 크기
    char *bp;

    if (size == 0) return NULL; //잘못된 요청 무시

    if (size <= DSIZE) asize = 2 * DSIZE; //오버헤드, 정렬 등을 반영한 조정된 블록 크기
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if (asize < MIN_FREEBLK) asize = MIN_FREEBLK;

    if ((bp = find_fit_exp(asize)) != NULL){
        place(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if (bp == NULL) return;
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }

    size_t old_block   = GET_SIZE(HDRP(ptr));
    size_t old_payload = old_block - DSIZE; // 헤더+풋터 제외

    void *newptr = mm_malloc(size);
    if (!newptr) return NULL;

    size_t new_payload = GET_SIZE(HDRP(newptr)) - DSIZE; // 새 블록의 실제 페이로드
    size_t copySize = old_payload;
    if (copySize > size)        copySize = size;         // 요청 크기 초과 금지
    if (copySize > new_payload) copySize = new_payload;  // 새 블록 수용량 초과 금지

    memmove(newptr, ptr, copySize); // 겹침 안전
    mm_free(ptr);
    return newptr;
}
