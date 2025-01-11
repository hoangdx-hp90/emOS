/*
 * emOS_InternalAPI.h
 *
 *  Created on: Feb 12, 2022
 *      Author: work
 */

#ifndef EMOS_EMOS_INTERNALAPI_H_
#define EMOS_EMOS_INTERNALAPI_H_
#include "emOS.h"
void			heap_init(void);
uint32_t		heap_free_left(void);
uint32_t		heap_total(void);


int32_t 		OS_ENTER_CRITICAL(void);
int32_t 		OS_EXIT_CRITICAL(void);

void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State);
void OS_SelectNextTaskToRun_ns();
void OS_CleanData_ns(void);
void OS_InsertList_ns(TCB_t ** list, TCB_t *tcb);
void OS_InsertEndList_ns(TCB_t ** list, TCB_t *tcb);
void OS_RemoveFromList_ns( TCB_t ** list,TCB_t * tcb);

#if configOS_ENABLE_SEM
void OS_CheckSemList_ns();
#endif
#if configOS_ENABLE_MUTEX
void 		OS_CheckMutexList_ns();
OS_Mutex_t 		*OS_MutexNew2(OS_Mutex_t * static_mutex);
#endif
#if configOS_RUN_SYSTEM_CHECK
void OS_SystemCheck(uint32_t check_cur_task);
#else
#define OS_SystemCheck(...)
#endif


#if configOS_ENABLE_QUEUE
OS_Queue_t * 	OS_QueueNew(uint16_t NumElement,uint16_t ElementSize);
uint32_t       	OS_QueuePend(OS_Queue_t * Mbox, void* RecBuf, uint32_t Timeout);
uint32_t    	OS_QueuePost(OS_Queue_t * Mbox, void * msg);
void 			OS_QueueDelete(OS_Queue_t *Mbox);
void			OS_CheckQueueList_ns();
#endif

#endif /* EMOS_EMOS_INTERNALAPI_H_ */
