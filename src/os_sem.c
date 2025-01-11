/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module : Semaphore API lib            		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 *   Version:   1.1                    			*
 ************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V3.0  | Hoangdx     |Dec,13,2022	  |First release
// ============================================================================
#include <stdint.h>
#include <stdio.h>
#include "../emOS_InternalAPI.h"

#if configOS_ENABLE_SEM

#ifndef OS_SEM_DEFAULT_MAX_COUNT
#define OS_SEM_DEFAULT_MAX_COUNT	65535
#endif
volatile OS_Sem_t	*SemList = NULL;
extern TCB_t volatile *OS_TaskList,*CurrentTCB,*NextTCB;
extern volatile		uint32_t		SchedulerState;
static uint32_t last_wait_seq = 0;
/*=================================================================*/
OS_Sem_t * OS_SemNew(uint16_t InitCount){
	uint8_t retry = 1;
	OS_Sem_t *result=NULL;
	//Alloc memory
	while(retry --){
		result = OS_MemMalloc(sizeof (OS_Sem_t));
		if(result != NULL) break;
		OS_ENTER_CRITICAL();
		OS_CleanData_ns();
		OS_EXIT_CRITICAL();
	}
	if(result == NULL){
		OS_PRINTF("\r\n[OS_SemNew] Out of memory");
		return	NULL;
	}
	//Init parameter
	OS_ENTER_CRITICAL();
	result->count 			= InitCount;
	result->max_count 		= OS_SEM_DEFAULT_MAX_COUNT;
	result->next			= (OS_Sem_t *)SemList;
	SemList					= result;
	OS_SystemCheck(1);
	OS_EXIT_CRITICAL();
	return result;
} 
/*=================================================================*/
void OS_SemDelete(OS_Sem_t* Sem){
	OS_Sem_t * curSem,*preSem=NULL;
	TCB_t 	*curTask;
	if(Sem == NULL) return;
	//Check if MUTEX is registered or not
	OS_ENTER_CRITICAL();
	curSem = (OS_Sem_t*)SemList;
	while(curSem != NULL){
		if(curSem ==Sem) break;
		preSem = curSem;
		curSem = curSem->next;
	}
	if(curSem == NULL){
		OS_EXIT_CRITICAL();
		OS_PRINTF("\r\n[OS_SemDelete] SEMAPHORE not found. Abort");
		return;
	}
	//Release all task waiting for current SEMAPHORE
	curTask = (TCB_t*) OS_TaskList;
	while(curTask != NULL){
		if(curTask->state == OS_TASK_WAIT_SEM && (OS_Sem_t*)curTask->wait_param == Sem){
			curTask->flag = 0;
			curTask->state = OS_TASK_STATE_READY;
		}
		curTask = curTask->next;
	}
	//Remove SEMAPHORE from the list
	if(SemList == Sem) SemList = SemList->next ;
	else {
		preSem->next = curSem->next;
		OS_MemFree(Sem);
	}
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/*=================================================================*/
uint32_t OS_SemPend(OS_Sem_t * Sem, uint32_t Timeout){
	extern TCB_t 	*SystemTaskHandle;
	uint8_t result;
	OS_ENTER_CRITICAL();
	OS_SystemCheck(1);
	if(CurrentTCB == SystemTaskHandle){
		OS_PRINTF("\r\n[OS] Bug. SystemTask could not stop %s:%d",__FILE__,__LINE__);
		OS_EXIT_CRITICAL();
		return 0;
	}
	if(Sem->count){
		result= Sem->count--;
		OS_EXIT_CRITICAL();
		return	result;
	}
	else if(Timeout == 0){	//Return immediate
		OS_EXIT_CRITICAL();
		return	0;
	}
	else{
		//		OS_PRINTF("\r\n[OS_SemPend] Task wait %s",CurrentTCB->name);
		CurrentTCB->state = OS_TASK_WAIT_SEM;
		CurrentTCB->wait_param  = Sem;
		CurrentTCB->wait_seq 	= last_wait_seq++;
		CurrentTCB->timer 		= Timeout;
		OS_SelectNextTaskToRun_ns();
		OS_SystemCheck();
		OS_EXIT_CRITICAL();
		CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
		OS_ENTER_CRITICAL();
		result = CurrentTCB->flag;
		OS_EXIT_CRITICAL();
	}
	return	result;
}
/*=================================================================*/
static inline void OS_SemCheckTaskWaiting_ns(OS_Sem_t * Sem){
	if(Sem == NULL) return ;
	while(Sem->count){
		TCB_t*  WakeUpTask = NULL;
		for(TCB_t * curTask = (TCB_t *) OS_TaskList; curTask != NULL; curTask = curTask->next ){
			if(curTask->state == OS_TASK_WAIT_SEM && curTask->wait_param == Sem){
				if(WakeUpTask == NULL || (WakeUpTask->wait_seq > curTask->wait_seq)){
					WakeUpTask = curTask;
				}
			}
		}
		if(WakeUpTask != NULL) {
			WakeUpTask->state = OS_TASK_STATE_READY;
			WakeUpTask->timer = 0;
			WakeUpTask->flag = 1;
			Sem->count--;
			if(WakeUpTask->priority > CurrentTCB->priority) OS_SelectNextTaskToRun_ns();
		}
		else break;
	}
}
/*=================================================================*/
uint32_t OS_SemPost(OS_Sem_t *Sem){
	uint32_t count =0;
	if(Sem == NULL) return 0;
	OS_ENTER_CRITICAL();
	OS_SystemCheck();
	if(Sem->count >= Sem->max_count){
		OS_PRINTF("\r\n[OS_SemPost] Semaphore full. Could not post any more\n");
	}
	else Sem->count++;
	OS_SemCheckTaskWaiting_ns(Sem);
	count =   Sem->count;
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(CurrentTCB != NextTCB && SchedulerState);
	return count;
}
//--------------------------------------------------------------------
void OS_SemSet(OS_Sem_t * Sem, uint16_t Count){
	if(Sem == NULL) return ;
	OS_ENTER_CRITICAL();
	OS_SystemCheck();
	Sem->count = Count;
	if(Sem->count > Sem->max_count){
		OS_PRINTF("\r\n[OS_SemSet] Limit sem count to %u",Sem->max_count );
		Sem->count = Sem->max_count;
	}
	OS_SemCheckTaskWaiting_ns(Sem);
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(CurrentTCB != NextTCB && SchedulerState);
}   
/*=================================================================*/
void OS_CheckSemList_ns(){
}
#endif
