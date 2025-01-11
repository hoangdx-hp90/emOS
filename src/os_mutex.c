/************************************************
 *	RTOS for Cortext M3-M4 CPU 					*
 *	Module : Mutex and Recursive mutex        	*
 *	Author : Hoangdx1@viettel.com.vn	      	*
 *	Version:	1.1      			 			*
 *************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V3.0  | Hoangdx     |Dec,13,2022	  |First release
// ============================================================================
#include <stdint.h>
#include <stdio.h>
#include "../emOS_InternalAPI.h"
#if configOS_ENABLE_MUTEX

#ifndef configRECURSIVE_MAX_LEVEL
#define configRECURSIVE_MAX_LEVEL      65535     // 0. Non-Recursive mutex
#endif                                           // Other : Maximum nesting level

//3.request sequence
volatile OS_Mutex_t	*MutexList = NULL;
extern TCB_t volatile *OS_TaskList,*CurrentTCB,*NextTCB;
extern volatile		uint32_t		SchedulerState;
static uint32_t last_wait_seq = 0;
/*=================================================================*/
OS_Mutex_t * OS_MutexNew(){
	uint8_t retry = 1;
	OS_Mutex_t *result=NULL;
	//Alloc memory
	while(retry --){
		result = OS_MemMalloc(sizeof (OS_Mutex_t));
		if(result != NULL) break;
		OS_ENTER_CRITICAL();
		OS_CleanData_ns();
		OS_EXIT_CRITICAL();
	}
	if(result == NULL){
		OS_PRINTF("\r\n[OS_MutexNew] Out of memory");
		return	NULL;
	}
	//Init parameter
	OS_ENTER_CRITICAL();
	result->RecursiveLevel = 0;
	result->Holder    = NULL;
	result->next	=	(OS_Mutex_t *)MutexList;
	MutexList		=	result;
	OS_SystemCheck(1);
	OS_EXIT_CRITICAL();
	return result;
}
/*=================================================================*/
OS_Mutex_t * OS_MutexNew2(OS_Mutex_t * static_mutex){
	if(static_mutex == NULL) return NULL;
	//Insert to List
	OS_ENTER_CRITICAL();
	static_mutex->RecursiveLevel = 0;
	static_mutex->Holder    = NULL;
	static_mutex->next		=	(OS_Mutex_t *)MutexList;
	MutexList				=	static_mutex;
	OS_EXIT_CRITICAL();
	return static_mutex;
}
/*=================================================================*/
void OS_MutexDelete(OS_Mutex_t *m){
	OS_Mutex_t * curMutex,*preMutex=NULL;
	TCB_t 	*curTask;
	if(m == NULL) return;
	//Check if MUTEX is registered or not
	OS_ENTER_CRITICAL();
	curMutex = (OS_Mutex_t*)MutexList;
	while(curMutex != NULL){
		if(curMutex ==m) break;
		preMutex = curMutex;
		curMutex = curMutex->next;
	}
	if(curMutex == NULL){
		OS_EXIT_CRITICAL();
		OS_PRINTF("\r\n[OS_MutexDelete] MUTEX not found. Abort");
		return;
	}
	//Release all task waiting for current MUTEX
	curTask = (TCB_t*) OS_TaskList;
	while(curTask != NULL){
		if(curTask->state == OS_TASK_WAIT_MUTEX && (OS_Mutex_t*)curTask->wait_param == m){
			curTask->flag = 0;
			curTask->state = OS_TASK_STATE_READY;
		}
		curTask = curTask->next;
	}
	//Remove Mutex from the list
	if(MutexList == m) MutexList = MutexList->next ;
	else {
		preMutex->next = curMutex->next;
		OS_MemFree(m);
	}
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/*=================================================================*/
uint8_t OS_MutexTake(OS_Mutex_t *m,uint32_t Timeout){ 
	extern TCB_t 	*SystemTaskHandle;
	uint8_t result;
	OS_ENTER_CRITICAL();
	OS_SystemCheck(1);
	if(CurrentTCB == SystemTaskHandle){
		OS_PRINTF("\r\n[OS] Bug. SystemTask could not stop %s:%d",__FILE__,__LINE__);
		OS_EXIT_CRITICAL();
		return 0;
	}
	//	OS_PRINTF("\r\n[OS_MutexTake] Level %u. Task request %s",m->RecursiveLevel, CurrentTCB->name);
	if(m->RecursiveLevel==0 || ( m->Holder == CurrentTCB  && m->RecursiveLevel < configRECURSIVE_MAX_LEVEL )){
		m->RecursiveLevel++;
		result = m->RecursiveLevel;
		m->Holder = (TCB_t*)CurrentTCB;
		m->lock_time = OS_GetCurrentTick();
		OS_SystemCheck(1);
		OS_EXIT_CRITICAL();
		return	result;
	}
	else	 if(Timeout == 0){	//Return immediate
		OS_EXIT_CRITICAL();
		return	0;
	}
	else{
		//		OS_PRINTF("\r\n[OS_MutexTake] Task wait %s",CurrentTCB->name);
		CurrentTCB->state = OS_TASK_WAIT_MUTEX;
		CurrentTCB->wait_param = m;
		CurrentTCB->wait_seq = last_wait_seq++;
		CurrentTCB->timer = Timeout;
		OS_SelectNextTaskToRun_ns();
		OS_SystemCheck();
		OS_EXIT_CRITICAL();
		CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
		OS_ENTER_CRITICAL();
		result = CurrentTCB->flag;
		OS_EXIT_CRITICAL();
	}
	if(result ==0) OS_PRINTF("\r\n[OS] Mutex timeout. Task %s",CurrentTCB->name);
	return	result;
}
/*=================================================================*/
uint8_t OS_MutexRelease(OS_Mutex_t *m){
	uint32_t	result = 0;
	TCB_t 	*curTask,*WakeUpTask;
	if(m == NULL) return 0;
	OS_ENTER_CRITICAL();
	//Check level
	//	OS_PRINTF("\r\n[OS_MutexRelease] Level %u. Task release %s",m->RecursiveLevel, CurrentTCB->name);
	if(m->RecursiveLevel >1){
		m->RecursiveLevel--;
		result = m->RecursiveLevel;
		OS_EXIT_CRITICAL();
		return result;
	}
	else if(m->RecursiveLevel ==0){
		OS_PRINTF("\r\n[OS_MutexRelease] Mutex using error");
		if(m->Holder != NULL) OS_PRINTF(". Task cause error %s. Task release %s", m->Holder->name, CurrentTCB->name);
		else OS_PRINTF(". Task release %s", CurrentTCB->name);
	}
	if(((OS_GetCurrentTick()-m->lock_time)&0x7fffffff) > configOS_TICK_PER_SECOND){
		OS_PRINTF("\r\n[OS_MutexRelease] WARNING Task hold mutex for long time %u",(OS_GetCurrentTick()-m->lock_time)&0x7fffffff);
		if(m->Holder != NULL) OS_PRINTF(". Task cause warning %s. Task release %s", m->Holder->name, CurrentTCB->name);
		else OS_PRINTF(". Task release %s",  CurrentTCB->name);
	}
	m->RecursiveLevel =0;
	m->Holder = NULL;
	//Wake-up task waiting for MUTEX
	WakeUpTask = NULL;
	for(curTask = (TCB_t *) OS_TaskList; curTask != NULL; curTask = curTask->next ){
		if(curTask->state == OS_TASK_WAIT_MUTEX && curTask->wait_param == m){
			if(WakeUpTask == NULL || (WakeUpTask->wait_seq > curTask->wait_seq)){
				WakeUpTask = curTask;
			}
		}
	}
	if(WakeUpTask != NULL) {
		WakeUpTask->state = OS_TASK_STATE_READY;
		WakeUpTask->timer = 0;
		WakeUpTask->flag = 1;
		m->RecursiveLevel =1;
		m->Holder = WakeUpTask;
		m->lock_time = OS_GetCurrentTick();
		//		OS_PRINTF("\r\n[OS_MutexRelease] Task receipt %s",WakeUpTask->name);
		OS_SelectNextTaskToRun_ns();
	}
	//Context switch ?
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(CurrentTCB != NextTCB && SchedulerState);
	return result;
}
/*=================================================================*/
void OS_CheckMutexList_ns(){
}
#endif
