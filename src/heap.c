/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module : Heap management unit      		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 *   Version:   1.1                    			*
 ************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V1.1  | Hoangdx     |Jan,04,2021	  | Fix Cpp check warning
//		V1.1  | Hoangdx     |Apr,25,2021	  | Fix Change OS_ENTER_CRITICAL and OS_EXIT_CRITICAL from void to uint32_t
// ============================================================================
#include <stdio.h>
#include <stdint.h>
#include "../emOS_InternalAPI.h"


//-------------------------------------------------------------------------------------
#ifndef	CPU_BYTE_ALIGNMENT
#define	CPU_BYTE_ALIGNMENT	sizeof(_ssize_t)
#endif

#ifndef	HEAP_MIN_SIZE
#define	HEAP_MIN_SIZE	4
#endif

#ifndef	configOS_HEAP_SIZE
#define	HEAP_SIZE		(1024*64)
#else
#define HEAP_SIZE	configOS_HEAP_SIZE
#endif

#ifndef	MEM_MEM_ALIGN
#define	MEM_MEM_ALIGN(x)	(((uint32_t)(x) +CPU_BYTE_ALIGNMENT -1)&~((uint32_t)(CPU_BYTE_ALIGNMENT) -1))
#endif
//-------------------------------------------------------------------------------------

#define	HEAP_MSG			OS_PRINTF
#define	HEAP_ERR			OS_PRINTF

/********* Heap format *******************************************************
| A_HEAP_LINK | Data Space | A_HEAP_LINK | Data Space | ..... | NULL |
| Next | Size | ...........| Next | Size | ...........| ..... | NULL |

   + Next : Next Block in linked list
   + Size : Size of Data Space
 *****************************************************************************/
typedef struct A_HEAP_LINK{
	struct   A_HEAP_LINK   *next;
	uint32_t   size;
} heap_link_t;

#define   HEAP_STRUCT_SIZE_ALIGNED      MEM_MEM_ALIGN(sizeof(heap_link_t))
#define   HEAP_SIZE_ALIGNED                  MEM_MEM_ALIGN(HEAP_SIZE)
#define   HEAP_MIN_SIZE_ALIGNED            MEM_MEM_ALIGN(HEAP_MIN_SIZE)
#define   HEAP_USED_MASK

unsigned   char   ARRAY_INDEX(ram_heap,HEAP_SIZE_ALIGNED   +   HEAP_STRUCT_SIZE_ALIGNED   + sizeof(uint32_t)-1);
heap_link_t   *heap_free_list;

volatile   unsigned char heap_init_state =0;

/*********************************************************************************************************
 *          Function: OS_HeapInit
 *
 * Description : This function is called by RTOS to init OS heap memory.   Your
 *                      application MUST NOT call this function.
 *
 * Arguments    : none
 *
 * Returns       : none
 *
 * Note(s)      : This function is INTERNAL to RTOS and your application should not call it.
 *********************************************************************************************************/
void      OS_HeapInit(void){
	OS_ENTER_CRITICAL();
	if(heap_init_state ==0){
		//01.Init free Link list
		heap_free_list =(heap_link_t   *) MEM_MEM_ALIGN(ram_heap);
		heap_free_list->next   =   NULL;
		heap_free_list->size   =   HEAP_SIZE_ALIGNED;
		//02.Change heap_Init State
		heap_init_state = 1;
		OS_EXIT_CRITICAL();
//		HEAP_MSG("\r\nHeap initialization done");
	}
	else{
		HEAP_ERR("\r\nHeap initialization error. Call init when heap_init_state != 0");
		HEAP_ERR("\r\nHeap initialization error. Internal error. Please Stop");
	}
}
/*********************************************************************************************************
 *          Function: OS_MemMalloc
 *
 * Description : Malloc a memory block with size from OS Heap
 *
 * Arguments    : size : Size of block need to malloc
 *
 * Returns       : OK      : Pointer point to first element in memory block.
 *                      FALSE : NULL
 *
 * Note(s)       :   Address always aligned to match CPU architect
 *********************************************************************************************************/
void *   OS_MemMalloc(uint32_t   size){
	heap_link_t   * current_block,*pre_block,*temp;
	void *result;

	if(heap_init_state ==0){
		OS_ENTER_CRITICAL();
		OS_HeapInit();
		OS_EXIT_CRITICAL();
	}
	//01.Check parameter
	if(size ==0) return NULL;
	size   =   (uint32_t)MEM_MEM_ALIGN(size);
	result = NULL;
	//02.Prevent concurrent access by OS_CRITICAL Section
	OS_ENTER_CRITICAL();
	//   OS_MutexTake(heap_key_ptr,OS_NO_TIMEOUT);
	//03.Find freeBlock has enough space
	current_block        = heap_free_list;
	pre_block = NULL;
	while(current_block != NULL){
		if(current_block->size >= size)   break;
		pre_block = current_block;
		current_block = current_block->next;
	}
	//04.Check   block
	if(current_block == NULL)   goto exit_;
	//05.Need Slip block in two ?
	if (current_block->size > size + HEAP_STRUCT_SIZE_ALIGNED + HEAP_MIN_SIZE_ALIGNED){
		temp   =   (heap_link_t*)((uint32_t)current_block + HEAP_STRUCT_SIZE_ALIGNED + size);
		temp->next = current_block->next;
		current_block->next = temp;
		temp->size = current_block->size - size - HEAP_STRUCT_SIZE_ALIGNED;
		current_block->size   =   size;
	}
	//06.Remove Block from free list
	if(pre_block != NULL){
		pre_block->next = current_block->next;
	}
	else   {
		heap_free_list   = current_block->next;
	}
	result =   (void*)((uint32_t)current_block + HEAP_STRUCT_SIZE_ALIGNED);
	exit_:
	OS_EXIT_CRITICAL();
	//   OS_MutexRelease(heap_key_ptr);
	// OS_PRINTF("\r\n\n maloc %u byte  %u",OS_MemSize(result),heap_free_left());
	return   result;
}
/*********************************************************************************************************
 *          Function: OS_MemFree
 *
 * Description : Push a memory block back to OS Heap
 *
 * Arguments    : p Address of block to be push back
 *
 * Returns       : 0             : MemFree Error
 *                      BlockSize : MemFree OK
 *
 * Note(s)       :
 *********************************************************************************************************/
uint32_t      OS_MemFree(void*   p){
	uint32_t block_size;
	heap_link_t   * current_block,*pre_block,*temp;
	//OS_PRINTF("\r\n\n Free %u byte",OS_MemSize(p));
	if(heap_init_state ==0) {
		HEAP_ERR("\r\n[heap_free] Heap is not configured. User programing error");
		return 0;
	}
	if(p== NULL) return 0 ;
	current_block = (heap_link_t*)((uint32_t)p - HEAP_STRUCT_SIZE_ALIGNED);
	OS_ENTER_CRITICAL();
	//   OS_MutexTake(heap_key_ptr,OS_NO_TIMEOUT);
	temp = heap_free_list;
	block_size = current_block->size;
	pre_block   = NULL;
	//01.Find position to push back
	while((temp !=NULL) && (temp <current_block)){
		pre_block = temp;
		temp = temp->next;
	}
	//02.Insert to freelist
	if(pre_block == NULL){
		current_block->next = heap_free_list;
		heap_free_list         = current_block;
	}
	else {
		current_block->next =pre_block->next;
		pre_block->next = current_block;
	}
	//03. Can combine with pre_block ?
	if(pre_block != NULL){
		if(((uint32_t)pre_block) + HEAP_STRUCT_SIZE_ALIGNED + pre_block->size == (uint32_t) current_block){
			pre_block->size += HEAP_STRUCT_SIZE_ALIGNED + current_block->size;
			pre_block->next = current_block->next;
			current_block = pre_block;
			//OS_PRINTF("\r\n\nHeap defragment 1  %u",block_size);
		}
	}
	//04.Can combine with next_block ?
	if(current_block->next != NULL){
		if(((uint32_t)current_block) + HEAP_STRUCT_SIZE_ALIGNED + current_block->size == (uint32_t) current_block->next){
			current_block->size += current_block->next->size + HEAP_STRUCT_SIZE_ALIGNED;
			current_block->next = current_block->next->next;
			// OS_PRINTF("\r\n\nHeap defragment 2  %u",block_size);
		}
	}

	OS_EXIT_CRITICAL();
	//   OS_MutexRelease(heap_key_ptr);
	//OS_PRINTF("\r\n\n Free left %u",heap_free_left());
	return block_size;
}
//==================================================================
uint32_t      OS_MemSize(void*   p){
	heap_link_t   * current_block;
	if (p == NULL) return 0;
	current_block = (heap_link_t*)((uint32_t)p - HEAP_STRUCT_SIZE_ALIGNED);
	return    current_block->size;
}
//==================================================================
void OS_Heapstatus(void){
	heap_link_t* mem;
	uint32_t i;

	OS_ENTER_CRITICAL();
	mem = heap_free_list;
	i=1;
	OS_PRINTF("\r\n+--------+--------------+------------+------------+");
	OS_PRINTF("\r\n|  index |   Address    | Next block |    size    |");
	OS_PRINTF("\r\n+--------+--------------+------------+------------+");
	while(mem != NULL){
		OS_PRINTF("\r\n|  %3u   |   %8x   |  %8x  |  %8u  |",(unsigned int)i,(unsigned int)(mem),(unsigned int)(mem->next),(unsigned int)(mem->size));
		i++;
		mem = mem->next;
	}
	OS_PRINTF("\r\n+--------+--------------+------------+------------+");
	OS_EXIT_CRITICAL();
}
//==================================================================
uint32_t   heap_free_left(){
	uint32_t total;
	heap_link_t* mem;

	OS_ENTER_CRITICAL();
	mem = heap_free_list;
	total =0;
	while(mem != NULL){
		total += mem->size;
		mem = mem->next;
	}
	OS_EXIT_CRITICAL();
	return total;
}
//==================================================================
uint32_t heap_total(void){
	return HEAP_SIZE;
}
//==================================================================
uint32_t heap_smallest_block(void){
	uint32_t smallest;
	heap_link_t* mem;

	OS_ENTER_CRITICAL();
	mem = heap_free_list;
	smallest   =0xffffffff;
	while(mem != NULL){
		if(mem->size <smallest) smallest = mem->size;
		mem = mem->next;
	}
	OS_EXIT_CRITICAL();
	return smallest;
} 
//==================================================================
void* malloc(size_t size){
	return OS_MemMalloc(size);
}
//==================================================================
void* calloc(size_t num, size_t size){
	void* r= OS_MemMalloc(num*size);
	if(r != NULL) {
		uint32_t i;
		for(i=0;i<num*size;i++) *((uint8_t*)r +i) = 0;
	}
	return r;
}
//==================================================================
void free(void* ptr){
	OS_MemFree(ptr);
}

