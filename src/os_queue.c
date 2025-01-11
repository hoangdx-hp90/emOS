/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module : Data Queue                  		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 *   Version:   1.1                    			*
 ************************************************/
// ============================================================================
// Revision History:
// ============================================================================
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V3.0  | Hoangdx     |Dec,13,2022	  |First release
// ============================================================================
#include <stdint.h>
#include <stdio.h>
#include "../emOS_InternalAPI.h"

#if configOS_ENABLE_QUEUE
volatile OS_Queue_t   *QueueList = NULL;
extern TCB_t volatile *OS_TaskList,*CurrentTCB,*NextTCB;
extern volatile		uint32_t		SchedulerState;
static uint32_t last_wait_seq = 0;
/*=================================================================*/
OS_Queue_t * OS_QueueNew(uint16_t NumElement,uint16_t ElementSize){
	uint8_t retry = 1;
	OS_Queue_t *result=NULL;
	//02. No memory. Try clear deleted pending data and alloc again
	//Alloc memory
	while(retry --){
		result = OS_MemMalloc(sizeof (OS_Queue_t));
		if(result != NULL) break;
		OS_ENTER_CRITICAL();
		OS_CleanData_ns();
		OS_EXIT_CRITICAL();
	}
	if(result == NULL){
		OS_PRINTF("\r\n[OS_QueueNew] Out of memory");
		return	NULL;
	}
	result->buf = OS_MemMalloc(ElementSize*NumElement);
	if(result->buf == NULL){
		OS_PRINTF("\r\n[OS_QueueNew] Out of memory");
		OS_MemFree(result);
		return   NULL;
	}
	// init parameter
	result->next 			= NULL;
	result->wr_index        =0;
	result->rd_index       	=0;
	result->element_size   	= ElementSize;
	result->buf_size		= ElementSize*NumElement;
	result->byteAvailable	= 0;
	//Insert to Queue List
	OS_ENTER_CRITICAL();
	result->next   = (OS_Queue_t *) QueueList;
	QueueList      =   result;
	OS_EXIT_CRITICAL();
	OS_SystemCheck();
	return result;
}
/*=================================================================*/
uint32_t OS_QueuePend(OS_Queue_t * Mbox, void* RecBuf, uint32_t Timeout){ 
	extern TaskHandle 	*SystemTaskHandle;
	uint32_t result;
	if(Mbox == NULL || RecBuf == NULL){
		OS_PRINTF("\r\n[OS_QueuePend] Invalid parameter value");
		return 0;
	}
	OS_ENTER_CRITICAL();
	if(CurrentTCB == SystemTaskHandle){
		OS_PRINTF("\r\n[OS] Bug. SystemTask could not stop %s:%d",__FILE__,__LINE__);
		OS_EXIT_CRITICAL();
		return 0;
	}
	if(Mbox->byteAvailable >= Mbox->element_size){      //Queue has any data ????
		uint32_t i;
		//Copy data to queue
		if(Mbox->rd_index >  Mbox->buf_size- Mbox->element_size ){
			OS_PRINTF("\r\n[OS_QueuePend] Queue bug %s %d", __FILE__, __LINE__);
			OS_EXIT_CRITICAL();
			return 0;
		}
		for(i=0;i< Mbox->element_size;i++){
			((uint8_t*)RecBuf)[i] = Mbox->buf[Mbox->rd_index+i];
		}
		if (Mbox->rd_index >= Mbox->buf_size) Mbox->rd_index = 0;
		Mbox->byteAvailable -= Mbox->element_size;
		OS_EXIT_CRITICAL();
		return   Mbox->element_size;
	}
	else{
		if(Timeout == 0){   //Return immediate
			OS_EXIT_CRITICAL();
			return   0;
		}
		else{
			CurrentTCB->state = OS_TASK_WAIT_QUEUE;
			CurrentTCB->wait_param = Mbox;
			CurrentTCB->rx_buf = RecBuf;
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
	}
	return	result;
}
/*=================================================================*/
uint32_t OS_QueuePost(OS_Queue_t * Mbox, void * msg){
	uint32_t count =0;

	if(Mbox == NULL || msg == NULL){
		OS_PRINTF("\r\n[OS_QueuePend] Invalid parameter value");
		return 0;
	}
	OS_ENTER_CRITICAL();

	if(Mbox->byteAvailable + Mbox->element_size > Mbox->buf_size){
		OS_DEBUG(2,("[OS_QueuePost] Queue full. Could not post any more"));
		OS_EXIT_CRITICAL();
		return 0;
	}
	else {
		uint32_t i;
		// Copy data to queue
		if(Mbox->wr_index >  Mbox->buf_size- Mbox->element_size ){
			OS_PRINTF("\r\n[OS_QueuePend] Queue bug %s %d", __FILE__, __LINE__);
			OS_EXIT_CRITICAL();
			return 0;
		}
		for(i=0;i< Mbox->element_size;i++){
			Mbox->buf[Mbox->wr_index+i] = ((uint8_t*)msg)[i];
		}
		if (Mbox->wr_index >= Mbox->buf_size) Mbox->wr_index = 0;
		Mbox->byteAvailable += Mbox->element_size;
	}

	//Wake-up task waiting for QUEUE
	while(Mbox->byteAvailable){
      TCB_t 	*curTask,*WakeUpTask;
		curTask = (TCB_t*)OS_TaskList;
		WakeUpTask = NULL;
		if(Mbox->byteAvailable <Mbox->element_size ){
			OS_PRINTF("\r\n[OS_QueuePend] Queue bug %s %d", __FILE__, __LINE__);
			OS_EXIT_CRITICAL();
			return 0;
		}
		while(curTask != NULL){
			if(curTask->state == OS_TASK_WAIT_QUEUE && curTask->wait_param == Mbox && curTask->rx_buf != NULL){
				if(WakeUpTask == NULL || (WakeUpTask->wait_seq > curTask->wait_seq)){
					WakeUpTask = curTask;
				}
			}
			curTask = curTask->next;
		}
		if(WakeUpTask != NULL) {
			uint32_t i;
			WakeUpTask->state = OS_TASK_STATE_READY;
			WakeUpTask->timer = 0;
			WakeUpTask->flag = Mbox->element_size;
			if(Mbox->rd_index >  Mbox->buf_size- Mbox->element_size ){
				OS_PRINTF("\r\n[OS_QueuePend] Queue bug %s %d", __FILE__, __LINE__);
				OS_EXIT_CRITICAL();
				return 0;
			}
			for(i=0;i<Mbox->element_size;i++) ((uint8_t*)WakeUpTask->rx_buf)[i] = Mbox->buf[Mbox->rd_index +i];
			if (Mbox->rd_index >= Mbox->buf_size) Mbox->rd_index = 0;
			Mbox->byteAvailable -= Mbox->element_size;
			OS_SelectNextTaskToRun_ns();
		}
		else break;
	}
	count =   Mbox->byteAvailable;
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(CurrentTCB != NextTCB && SchedulerState);
	return count;
}
/*=================================================================*/
void OS_QueueDelete(OS_Queue_t *Mbox){
	OS_Queue_t * curMbox,*preMbox=NULL;
	TCB_t 	*curTask;
	if(Mbox == NULL) return;
	//Check if MUTEX is registered or not
	OS_ENTER_CRITICAL();
	curMbox = (OS_Queue_t*)QueueList;
	while(curMbox != NULL){
		if(curMbox ==Mbox) break;
		preMbox = curMbox;
		curMbox = curMbox->next;
	}
	if(curMbox == NULL){
		OS_EXIT_CRITICAL();
		OS_PRINTF("\r\n[OS_SemDelete] SEMAPHORE not found. Abort");
		return;
	}
	//Release all task waiting for current QUEUE
	curTask = (TCB_t*) OS_TaskList;
	while(curTask != NULL){
		if(curTask->state == OS_TASK_WAIT_QUEUE && (OS_Queue_t*)curTask->wait_param == Mbox){
			curTask->flag = 0;
			curTask->state = OS_TASK_STATE_READY;
		}
		curTask = curTask->next;
	}
	//Remove QUEUE from the list
	if(QueueList == Mbox) QueueList = QueueList->next ;
	else {
		preMbox->next = curMbox->next;
		OS_MemFree(curMbox->buf);
		OS_MemFree(Mbox);
	}
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/*=================================================================*/
void OS_CheckQueueList_ns(){
} 
#endif
