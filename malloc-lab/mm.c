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
// #define PACK(size, prev_alloc, alloc) ((size) | ((prev_alloc)*2) | (alloc))

//주소 p의 워드 읽기, 쓰기
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

//주소 p의 크기, 할당 필드 읽기
#define GET_SIZE(p) ((GET(p) & ~0x7))
#define GET_ALLOC(p) ((GET(p) & 0x1))
// #define GET_PREVALLOC(p) ((GET(p) & 0x2))

//bp(블록 포인터)의 헤더, 풋터의 compute 주소
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

//bp(블록 포인터)의 이전, 다음 블록의 compute 주소
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//명시 리스트 포인터
#define PRED(bp) (*(void**)(bp))
#define SUCC(bp) (*(void**)((char*)bp + WSIZE))

static char *heap_listp;
static void *fit_bp;
static void *head_bp = NULL;

/*
static void implicit_place_repoint(void* bp){
    if (PRED(bp) && SUCC(bp)){
        PRED(SUCC(bp)) = PRED(bp);
        SUCC(PRED(bp)) = SUCC(bp);
    }
    else if (PRED(bp) && !SUCC(bp)){
        SUCC(PRED(bp)) = NULL;
    }
    else if (!PRED(bp) && SUCC(bp)){
        PRED(SUCC(bp)) = NULL;
        head_bp = SUCC(bp);
    }
    else
        head_bp = NULL;
}

static void implicit_place_split_repoint(void* bp, void* new_bp){
    if (PRED(bp) && SUCC(bp)){
        PRED(new_bp) = PRED(bp);
        SUCC(new_bp) = SUCC(bp);
        PRED(SUCC(new_bp)) = new_bp;
        SUCC(PRED(new_bp)) = new_bp;
    }
    else if (PRED(bp) && !SUCC(bp)){
        SUCC(PRED(bp)) = new_bp;
        PRED(new_bp) = PRED(bp);
        SUCC(new_bp) = NULL;
    }
    else if (!PRED(bp) && SUCC(bp)){
        PRED(SUCC(bp)) = new_bp;
        SUCC(new_bp) = SUCC(bp);
        PRED(new_bp) = NULL;
        head_bp = new_bp;
    }
    else{
        SUCC(new_bp) = NULL;
        PRED(new_bp) = NULL;
        head_bp = new_bp;
    }
}

static void implicit_free_repoint(void* bp, void* new_bp, void* pred_bp, void* succ_bp){
    // PRED, SUCC 포인터 조정
    if (pred_bp && succ_bp){
        SUCC(pred_bp) = succ_bp;
        PRED(succ_bp) = pred_bp;
    }
    else if (pred_bp && !succ_bp){
        SUCC(pred_bp) = NULL;
    }
    else if (!pred_bp && succ_bp){
        PRED(succ_bp) = NULL;
    }

    // 헤드, bp 포인터 조정
    if (head_bp){
        SUCC(new_bp) = head_bp;
        PRED(head_bp) = bp;
    }
    else{
        SUCC(bp) = NULL;
    }
    PRED(bp) = NULL;
    head_bp = bp;
} // 실패작
*/ 

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
} // 묵시

static void *find_fit_ex(size_t asize) {
    void *bp;
    for (bp = head_bp; GET_SIZE(HDRP(bp)) > 0; bp = SUCC(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }
    return NULL;
} // 명시

static void *find_next_fit(size_t asize) {
    void *bp = fit_bp;
    for (; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            fit_bp = bp;
            return bp;
        }
    }
    for (bp = NEXT_BLKP(heap_listp); bp != fit_bp; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            fit_bp = bp;
            return bp;
        }
    }
    return NULL;
} // 묵시

static void *find_best_fit(size_t asize) {
    void *bp;
    size_t best_asize = (size_t)-1;
    void *best_bp = NULL;
    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp))) continue;
        size_t csize = GET_SIZE(HDRP(bp));
        if (csize < asize) continue;
        if (csize == asize) return bp;
        if (csize > asize) {
            if (best_asize > csize){
                best_asize = csize;
                best_bp = bp;
                continue;
            }
        }
    }
    return best_bp;
} // 묵시

/* 헬퍼 함수 3개
static void detach(void *bp){
    void *prev = PRED(bp), *next = SUCC(bp);
    if (prev) SUCC(prev) = next; else head_bp = next;
    if (next) PRED(next) = prev;
    PRED(bp) = SUCC(bp) = NULL;
}

// 2) free-list의 old 자리에 new를 '교체' (split 시 사용)
static void replace_node(void *old_bp, void *new_bp){
    void *prev = PRED(old_bp), *next = SUCC(old_bp);
    PRED(new_bp) = prev;  SUCC(new_bp) = next;
    if (prev) SUCC(prev) = new_bp; else head_bp = new_bp;
    if (next) PRED(next) = new_bp;
    PRED(old_bp) = SUCC(old_bp) = NULL;
}

// 3) free-list 머리에 삽입 (free/coalesce 결과 삽입에 사용)
static void insert_front(void *bp){
    PRED(bp) = NULL;
    SUCC(bp) = head_bp;
    if (head_bp) PRED(head_bp) = bp;
    head_bp = bp;
}

*/
static void place(void *bp, size_t asize){
    size_t originsize = GET_SIZE(HDRP(bp));

    // if (originsize - asize >= 2 * DSIZE){ // n * DSIZE가 최소 블록 단위... (3 * WSIZE로 했었음. 더블워드라 4*WSIZE(2*DSIZE)였어야하는데 잘못 생각)
    if (originsize - asize >= (2 * DSIZE)) {
        void *new_bp;
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(originsize-asize, 0));
        PUT(FTRP(bp), PACK(originsize-asize, 0));        
    }
    else{
        PUT(HDRP(bp), PACK(originsize, 1));
        PUT(FTRP(bp), PACK(originsize, 1));
    }
    fit_bp = NEXT_BLKP(bp); // next_fit 재사용 편향성 줄여줌 (필수X)
} // 묵시

static void place_imp(void *bp, size_t asize){
    size_t originsize = GET_SIZE(HDRP(bp));

    // if (originsize - asize >= 2 * DSIZE){ // n * DSIZE가 최소 블록 단위... (3 * WSIZE로 했었음. 더블워드라 4*WSIZE(2*DSIZE)였어야하는데 잘못 생각)
    if (originsize - asize >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *new_bp = NEXT_BLKP(bp);
        PUT(HDRP(new_bp), PACK(originsize-asize, 0));
        PUT(FTRP(new_bp), PACK(originsize-asize, 0));
        // implicit_place_split_repoint(bp, new_bp); // PRED, SUCC 포인터 갱신
        replace_node(bp, new_bp);
    }
    else{
        PUT(HDRP(bp), PACK(originsize, 1));
        PUT(FTRP(bp), PACK(originsize, 1));
        // implicit_place_repoint(bp); // PRED, SUCC 포인터 갱신
        detach(bp);
    }
    fit_bp = NEXT_BLKP(bp); // next_fit 재사용 편향성 줄여줌 (필수X)
} // 명시

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 프로로그에 풋터가 있다고 가정
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc){                     // 이전 free: 먼저 리스트에서 제거
        void *prev = PREV_BLKP(bp);
        detach(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(size, 0));
        PUT(FTRP(bp),   PACK(size, 0));
        bp = prev;
    }
    if (!next_alloc){                     // 다음 free: 먼저 리스트에서 제거
        void *next = NEXT_BLKP(bp);
        detach(next);
        size += GET_SIZE(HDRP(next));
        PUT(HDRP(bp),   PACK(size, 0));
        PUT(FTRP(next), PACK(size, 0));
    }

    // 병합 결과 블록을 free-list 머리에 삽입
    insert_front(bp);

    // (넥스트핏이면) 로버 보정
    fit_bp = NEXT_BLKP(bp);
    return bp;
}

// static void *coalesce(void *bp){
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
//     size_t size = GET_SIZE(HDRP(bp));

//     //case1 이전, 다음 블록 모두 할당
//     if (prev_alloc && next_alloc){
//         implicit_free_repoint(bp, bp, NULL, NULL);
//         return bp;
//     } 

//     //case2 이전 블록 할당, 다음 블록 가용
//     else if (prev_alloc && !next_alloc){
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));

//         //명시
//         void* new_bp = NEXT_BLKP(bp);
//         implicit_free_repoint(bp,bp,PRED(new_bp),SUCC(new_bp));
//     }
//     //case3 이전 블록 가용, 다음 블록 할당
//     else if (!prev_alloc && next_alloc){
//         size += GET_SIZE(HDRP(PREV_BLKP(bp)));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0)); // FTRP의 GET_SIZE가 갱신되면서 풋터 위치 조정

//         //명시
//         void* new_bp = PREV_BLKP(bp); // prev_bp
//         implicit_free_repoint(bp,new_bp,PRED(new_bp),SUCC(new_bp));
       
//         bp = PREV_BLKP(bp);
//     }
//     //case4 이전, 다음 블록 모두 가용
//     else{
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

//         //명시
//         void *prev_bp = PREV_BLKP(bp); // new_bp
//         void *next_bp = NEXT_BLKP(bp);
//         implicit_free_repoint(bp,prev_bp,PRED(prev_bp),SUCC(next_bp));
        
//         bp = PREV_BLKP(bp);
//     }
//     //next-fit
//     fit_bp = NEXT_BLKP(bp);
//     return bp;
// }

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0)); // 새로 만든 블록의 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 새로 만든 블록의 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그(새로 만든 블록 뒤)
    
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
    heap_listp += (2*WSIZE); // 힙 영역 시작
    fit_bp = NEXT_BLKP(heap_listp); // next_fit 포인터
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

    if ((bp = find_fit_ex(asize)) != NULL){
        place_imp(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place_imp(bp, asize);
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
    // if (ptr == NULL) return mm_malloc(size);   // 표준 동작
    // if (size == 0) { mm_free(ptr); return NULL; }

    // size_t oldsize = GET_SIZE(HDRP(ptr)) - DSIZE;   // 기존 페이로드 크기
    // size_t copySize = (size < oldsize) ? size : oldsize;

    // void *newptr = mm_malloc(size);
    // if (newptr == NULL) return NULL;

    // memcpy(newptr, ptr, copySize);
    // mm_free(ptr);
    // return newptr;
}