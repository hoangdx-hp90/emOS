/*
 * emOS_InternalAPI.h
 *
 *  Created on: Feb 12, 2022
 *      Author: work
 */

#ifndef EMOS_EMOS_INTERNALAPI_H_
#define EMOS_EMOS_INTERNALAPI_H_
#include "emOS.h"
void	 __CPU_FUNC_ATTRIBUTE__		heap_init(void);
uint32_t __CPU_FUNC_ATTRIBUTE__		heap_free_left(void);
uint32_t __CPU_FUNC_ATTRIBUTE__		heap_total(void);


int32_t __CPU_FUNC_ATTRIBUTE__		OS_ENTER_CRITICAL(void);
int32_t __CPU_FUNC_ATTRIBUTE__		OS_EXIT_CRITICAL(void);

void __CPU_FUNC_ATTRIBUTE__ CPU_REQUEST_CONTEXT_SWITCH(uint32_t State);
void __CPU_FUNC_ATTRIBUTE__ OS_SelectNextTaskToRun_ns();
void __CPU_FUNC_ATTRIBUTE__ OS_CleanData_ns(void);
void __CPU_FUNC_ATTRIBUTE__ OS_InsertList_ns(TCB_t ** list, TCB_t *tcb);
void __CPU_FUNC_ATTRIBUTE__ OS_InsertEndList_ns(TCB_t ** list, TCB_t *tcb);
void __CPU_FUNC_ATTRIBUTE__ OS_RemoveFromList_ns( TCB_t ** list,TCB_t * tcb);

#if configOS_ENABLE_SEM
void __CPU_FUNC_ATTRIBUTE__ OS_CheckSemList_ns();
#endif
#if configOS_ENABLE_MUTEX
void __CPU_FUNC_ATTRIBUTE__ OS_CheckMutexList_ns();
OS_Mutex_t 	__CPU_FUNC_ATTRIBUTE__ *OS_MutexNew2(OS_Mutex_t * static_mutex);
#endif
#if configOS_RUN_SYSTEM_CHECK
void OS_SystemCheck(uint32_t check_cur_task);
#else
#define OS_SystemCheck(...)
#endif


#if configOS_ENABLE_QUEUE
OS_Queue_t * __CPU_FUNC_ATTRIBUTE__ 	OS_QueueNew(uint16_t NumElement,uint16_t ElementSize);
uint32_t     __CPU_FUNC_ATTRIBUTE__  	OS_QueuePend(OS_Queue_t * Mbox, void* RecBuf, uint32_t Timeout);
uint32_t     __CPU_FUNC_ATTRIBUTE__		OS_QueuePost(OS_Queue_t * Mbox, void * msg);
void 		 __CPU_FUNC_ATTRIBUTE__		OS_QueueDelete(OS_Queue_t *Mbox);
void		 __CPU_FUNC_ATTRIBUTE__ 	OS_CheckQueueList_ns();
#endif

#endif /* EMOS_EMOS_INTERNALAPI_H_ */
