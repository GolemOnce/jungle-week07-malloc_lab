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

static char *heap_listp;
 
// static void *find_fit(size_t asize){
//     void *bp = heap_listp; // 프롤로그 건너뛰기 > NEXT_BLKP(heap_listp);
//     while (GET_SIZE(HDRP(bp)) > 0) { //에필로그 전까지
//         if (GET_SIZE(HDRP(bp)) > asize || !GET_ALLOC((HDRP(bp))))
//         return bp;
//         bp += GET_SIZE(HDRP(bp)); // bp = NEXT_BLKP(bp);
//     }
//     return NULL;
// }

static void *find_fit(size_t asize) {
    void *bp;
    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize){
    size_t originsize = GET_SIZE(HDRP(bp));

    if (originsize - asize >= 2 * DSIZE){ //2* DSIZE가 최소 블록 단위... (3 * WSIZE로 했었음. 더블워드라 4*WSIZE(2*DSIZE)였어야하는데 잘못 생각)
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //bp = NEXT_BLKP(bp);
        PUT(HDRP(NEXT_BLKP(bp)), PACK(originsize-asize, 0)); //NEXT_BLKP(bp) 대신 bp
        PUT(FTRP(NEXT_BLKP(bp)), PACK(originsize-asize, 0)); //NEXT_BLKP(bp) 대신 bp
    }
    else{
        PUT(HDRP(bp), PACK(originsize, 1));
        PUT(FTRP(bp), PACK(originsize, 1));
    }
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //case1 이전, 다음 블록 모두 할당
    if (prev_alloc && next_alloc) return bp;
    //case2 이전 블록 할당, 다음 블록 가용
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    //case3 이전 블록 가용, 다음 블록 할당
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); // FTRP의 GET_SIZE가 갱신되면서 풋터 위치 조정
        bp = PREV_BLKP(bp);
    }
    //case4 이전, 다음 블록 모두 가용
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

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

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
    size_t asize; //조정된 블록 사이즈
    size_t extendsize; //확장할 힙의 크기
    char *bp;

    if (size == 0) return NULL; //잘못된 요청 무시

    if (size <= DSIZE) asize = 2 * DSIZE; //오버헤드, 정렬 등을 반영한 조정된 블록 크기
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL){
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

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize; 

    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //     return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //     copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;
    if (ptr == NULL) return mm_malloc(size);   // 표준 동작
    if (size == 0) { mm_free(ptr); return NULL; }

    size_t oldsize = GET_SIZE(HDRP(ptr)) - DSIZE;   // 기존 페이로드 크기
    size_t copySize = (size < oldsize) ? size : oldsize;

    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}