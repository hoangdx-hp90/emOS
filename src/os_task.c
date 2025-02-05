/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module :  Task Scheduler           		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 ************************************************/
/* ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V3.0  | Hoangdx     |Dec,13,2022	  |First release
// ============================================================================
	2024-02-20: Fixed round robin priority arbiter sequence over flow
============================================================================*/
#include <stdint.h>
#include <string.h>
#include "../emOS_InternalAPI.h"
#if configPORT_REDIRECT_STDOUT_TO_UDP_PORT >0
#include "net.h"
#endif
//====== External function =================================
extern	void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param);
extern 	void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State); 
extern uint64_t CPU_GET_TIMER_VAL(void);
extern uint32_t CPU_GET_TIMER_PERIOD();
void		 OS_Err_Handler(uint32_t err_code, uint32_t err_addr);
#ifndef	configOS_RUNTIME_UPDATE_TICK
#define	configOS_RUNTIME_UPDATE_TICK		5000
#endif

#ifndef	configOS_SUPPORT_RUNTIME_STATUS
#define	configOS_SUPPORT_RUNTIME_STATUS	1
#endif

#ifndef	configOS_CAL_CPU_HEAP_STATUS
#define	configOS_CAL_CPU_HEAP_STATUS	1
#endif

#ifndef configOS_MAX_TICK_CALLBACK_FCN
#define configOS_MAX_TICK_CALLBACK_FCN	8
#endif

#if configOS_MAX_TICK_CALLBACK_FCN < 1
#undef configOS_MAX_TICK_CALLBACK_FCN
#define configOS_MAX_TICK_CALLBACK_FCN	8
#endif
//====== Internal variable =================================
TCB_t volatile *OS_TaskList 	= NULL;
TCB_t volatile *CurrentTCB	=	NULL;		// Current running task.
TCB_t volatile *NextTCB		=	NULL;		// Current running task.


TCB_t 	*SystemTaskHandle =NULL;
volatile		uint32_t		SchedulerState = 0;
static volatile	uint32_t		OS_Tick,OS_TimeSecond;
static volatile	uint32_t		SystemTask_lastRunTime =0;
static volatile	uint16_t     	CPU_Usage;
volatile 		uint32_t		NeedSwitchContext 	=0;
volatile		uint32_t		OS_INTERRUPT_CONTEXT=0;
#if configOS_CAL_CPU_HEAP_STATUS >=1
uint32_t volatile heap_remaining;
#endif


static void (*tick_callback_list[configOS_MAX_TICK_CALLBACK_FCN])(void) = {NULL};

const char* task_state_string[] ={
		"READY",
		"SUSPEND",
		"SLEEP",
		"WAIT_MUTEX",
		"WAIT_SEM",
		"WAIT_QUEUE",
		"DELETED",
		"UNKNOWN"
};

//================================================================================
// CPU DEFAULT FUNCTION
//================================================================================
void __attribute((weak))		CPU_PERFORM_OTHER_CONFIG(void){}
void __attribute((weak))		CPU_TICK_CALLBACK(void){}
void __attribute((weak))  		CPU_IDLE_CALLBACK(){}
void __attribute((weak))  		CPU_OS_TICK_CALBACK(){}
/********************************************************************************
 *   Function    : check_exits
 *   Description :
 *   Input       :
 *   Output      :
 *   Return      : NULL If TCB is not exits in current list
 ********************************************************************************/
//static inline TCB_t* check_exits_ns(TCB_t*list ,const TCB_t*new_item){
//	while(list!=NULL){
//		if(list == new_item){
//			OS_PRINTF("\r\n[OS_Task] Could not reinsert exited task to list at %s_%d",__FILE__,__LINE__);
//			return list;
//		}
//		list=list->next;
//	}
//	return list;
//}
//********************************************************************************/
void OS_InsertList_ns(TCB_t ** list, TCB_t *tcb){
	if(list == NULL || tcb == NULL) return ;
	tcb->next = *list;
	*list = tcb;
}
//********************************************************************************/
void OS_InsertEndList_ns(TCB_t ** list, TCB_t *tcb){
	TCB_t * cur,*pre;
	if(list == NULL  || tcb == NULL){
		OS_PRINTF("\r\n[OS_InsertEndList] Could not process NULL list");
		return;
	}
	cur = *list; pre	= NULL;
	tcb->next 	= NULL;
	//01.Find end position
	while(cur!= NULL){
		pre = cur; cur = cur->next;
	}
	//02.Insert
	if(pre == NULL)  *list = tcb;
	else pre->next = tcb;
}
//********************************************************************************/
void OS_RemoveFromList_ns( TCB_t ** list,TCB_t * tcb){
	TCB_t * cur,*pre;
	if(list == NULL  || tcb == NULL) return ;
	cur = *list;  pre	= NULL;
	while(cur != NULL){
		if(cur == tcb){
			if( pre == NULL) *list = cur->next;
			else pre->next = cur->next;
			tcb->next = NULL;
			return ;
		}
		pre	   = cur; cur = cur->next;
	}
}
//********************************************************************************/
void OS_CleanData_ns(void){
	TCB_t * cur= (TCB_t*)OS_TaskList,*pre=NULL;
	while(cur != NULL){
		if(cur->state == OS_TASK_STATE_DELETED){
			TCB_t * tmp = cur;
			//			h_printf("\r\n Clean task %s %d",cur->name,pre);
			if(pre == NULL){
				OS_TaskList = cur->next;
				cur = cur->next;
				OS_MemFree(tmp);
			}
			else{
				pre->next = cur->next;
				cur = pre->next;
				OS_MemFree(tmp);
			}
		}
		else{
			pre = cur;
			cur = cur->next;
		}
	}
}
//********************************************************************************/
void OS_SelectNextTaskToRun_ns(){
	TCB_t * sel_task = NULL;
	for(TCB_t * cur = (TCB_t*)OS_TaskList;cur != NULL; cur = cur->next ){
		if(cur->state == OS_TASK_STATE_READY){
			if(sel_task == NULL || cur->priority > sel_task->priority ){ //Task with higher priority => select current task
				sel_task 				= cur;
			}
			else if(cur->priority == sel_task->priority){//Task with same priority => select task which not run for longest time
				if(cur->sub_priority > sel_task->sub_priority){
					sel_task = cur;
				}
			}
		}
	}
	//Check run quota
	if(sel_task != NULL && CurrentTCB != NULL && sel_task->priority == CurrentTCB->priority &&  CurrentTCB->run_remain  && CurrentTCB->state == OS_TASK_STATE_READY){
		sel_task = (TCB_t*)CurrentTCB;
	}
	//Check if need wakeup system task
	if(((OS_Tick - SystemTask_lastRunTime)&0x7fffffff) >= configOS_RUNTIME_UPDATE_TICK){
		sel_task = SystemTaskHandle;
	}
	//sel_task->wait_priority = 0;
	NextTCB = sel_task;
	//	OS_PRINTF("\r\n %s -> %s at %u" , CurrentTCB->name,NextTCB->name, OS_Tick);
}
/********************************************************************************/
static inline void  OS_UpdateTaskRunTime(TCB_t * tcb){
	if(tcb != NULL) {
		uint32_t cur_time = CPU_GET_TIMER_VAL();
		uint32_t cur_run_time = cur_time - tcb->start_time;
		tcb->runtime += cur_run_time;
		tcb->start_time = cur_time;
	}
}
//********************************************************************************/
static void OS_SystemTask(void* param){
	uint32_t LastTime =0;

	for(;;){
		//		OS_PRINTF("\r\n[emOS] System task run");
		OS_ENTER_CRITICAL();
		SystemTask_lastRunTime =OS_Tick;//Reset SystemTask sleep counter.
		OS_CleanData_ns();
#if configOS_ENABLE_SEM
		OS_CheckSemList_ns();
#endif
#if configOS_ENABLE_MUTEX
		OS_CheckMutexList_ns();
#endif
#if configOS_ENABLE_QUEUE
		OS_CheckQueueList_ns();
#endif
#if configOS_SUPPORT_RUNTIME_STATUS
		//04.Calculate OS Task usage.
		if( OS_Tick - LastTime >= (configOS_RUNTIME_UPDATE_TICK)){
			static uint64_t last_test_timestamp = 0;
			uint64_t cur_timestamp = CPU_GET_TIMER_VAL();
			uint32_t time_diff_div100 = cur_timestamp - last_test_timestamp;
			time_diff_div100 = time_diff_div100/100;
			if(time_diff_div100 <1) time_diff_div100 = 1;
			last_test_timestamp = cur_timestamp;
			uint32_t TotalActiveTime = 0;
			TCB_t * tcb = (TCB_t*)OS_TaskList;
			OS_UpdateTaskRunTime(SystemTaskHandle);
			//-- update CPU usage of R_TaskList
			while(tcb!= NULL){
				if(tcb != (TCB_t*)SystemTaskHandle)  TotalActiveTime += (tcb->runtime);
				tcb->usage = tcb->runtime/time_diff_div100;
				if(tcb->usage>100) tcb->usage = 100;
				tcb->runtime =0;
				if(tcb->pStack <= tcb->pStack_Mark || *tcb->pStack_Mark != 0xdeadbeaf){
					OS_PRINTF("\r\n[emOS] Task %s stack corrupt. Please assign more stack for this task %08x", tcb->name,*tcb->pStack_Mark);
				}
				tcb = tcb->next;
			}
			CPU_Usage  = TotalActiveTime/time_diff_div100;
			LastTime = OS_Tick;
#if configOS_CAL_CPU_HEAP_STATUS
			heap_remaining = heap_free_left();
#endif
		}
#endif
#if configOS_ENABLE_IDLE_CALLBACK
		CPU_IDLE_CALLBACK();
#endif
		OS_CheckTerminal();
		OS_SelectNextTaskToRun_ns();
		if(CurrentTCB != NextTCB) {
			OS_EXIT_CRITICAL();
			CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
		}
		else{
			uint32_t volatile i;
			OS_EXIT_CRITICAL();
			for(i=0;i<1000;i++);
		}
	}
}
/********************************************************************************
 *   Function    : OS_TaskNew
 *   Description : Create a new task.
 *   Input       : TaskCode : address where task code is locate.
                             Format:   void TaskCode( void* param){
                                          for{;;}{
                                             do smth
                                          }
                                       }
 *                 name     : task name
 *                 StackDeep: stack for new task( in word)
 *                 Param    : task parameter
 *                 Priority : higher value is higher priority
 *   Output      : None
 *   Return      : TCB_t
 ********************************************************************************/
TCB_t*		OS_TaskNew(TaskFunction_t   TaskCode,const char ARRAY_INDEX(name,configMAX_TASK_NAME_LEN+1), uint16_t	StackDeep, void* Param , int8_t Priority){
	TCB_t * new_tcb=NULL;
	uint32_t	i;
	uint8_t retry = 1;

	while(retry--){
		//01. Malloc memory for new TCB and Stack for new task
		if(StackDeep < configOS_MIN_STACK_DEEP) StackDeep = configOS_MIN_STACK_DEEP;
		new_tcb = OS_MemMalloc((StackDeep<<2) + ALIGNED_SIZE(sizeof (TCB_t)) +4);
		new_tcb = (TCB_t*)(((uint32_t)new_tcb+3)&0xfffffffc);	//Align to 4 byte
		if(new_tcb != NULL) break;
		OS_CleanData_ns();
	}
	if (new_tcb == NULL){
		OS_PRINTF("\r\n[ OS_TaskNew] error. Out of memory");
		return NULL;
	}
	//02. Setup parameter 
	for(i=0; i< configMAX_TASK_NAME_LEN && name[i] != 0;i++){
		new_tcb->name[i] = name[i];
	}
	new_tcb->name[i]    = 0; 
	new_tcb->priority   = Priority;   
	new_tcb->next	    = NULL;
	new_tcb->state      = OS_TASK_STATE_READY;
	new_tcb->timer      = 0;
	new_tcb->usage		= 0;
	new_tcb->quota 		= 1;
	new_tcb->run_remain  = 1;
	new_tcb->sub_priority = 0;
	new_tcb->StackDeep = StackDeep;
	new_tcb->StackRemain = StackDeep;
	new_tcb->pStack_Mark	= (uint32_t*)((uint32_t)ALIGNED_SIZE(sizeof(TCB_t)) + (uint32_t)new_tcb);
	new_tcb->pStack			=   (new_tcb->pStack_Mark + StackDeep) ;

	{
		uint32_t *tmp32 = new_tcb->pStack_Mark;
		while(tmp32 != new_tcb->pStack) {
			*tmp32++ = 0xdeadbeaf;
		}
	}

	//03. Setup task for first run
	CPU_TASK_INIT(new_tcb,TaskCode,Param);
	//04. Push task to Active list
	OS_ENTER_CRITICAL();
	OS_InsertEndList_ns((TCB_t**)&OS_TaskList,new_tcb);
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
	return	(TCB_t*)new_tcb;
}
/********************************************************************************
 *   Function    : OS_Start
 *   Description : Start OS service.
 *   Input       : None
 *   Output      : None
 *   Return      : number of stack deep unused
 ********************************************************************************/
uint32_t OS_GetTaskStackRemain(TCB_t* handle){
	OS_ENTER_CRITICAL();
	uint32_t unused =0;
	uint32_t stack_deep = (handle ==NULL)? CurrentTCB->StackDeep: ((TCB_t*)handle)->StackDeep;
	uint32_t *p32 = (handle ==NULL)? CurrentTCB->pStack_Mark: ((TCB_t*)handle)->pStack_Mark;
	while(unused < stack_deep){
		if(*p32 != 0xdeadbeaf) break;
		p32++;
		unused++;
	}
	CurrentTCB->StackRemain = unused;
	OS_SystemCheck(1);
	OS_EXIT_CRITICAL();
	return unused;
}
/********************************************************************************
 *   Function    : OS_GetTaskName
 *   Description : Get task name
 *   Input       : None
 *   Output      : None
 *   Return      : taskname pointer
 ********************************************************************************/
char* OS_GetTaskName(void){
	return (char*)CurrentTCB->name;
}
/********************************************************************************
 *   Function    : OS_Start
 *   Description : Start OS service.
 *   Input       : None
 *   Output      : None
 *   Return      : This function never return
 ********************************************************************************/
void OS_Start(){
	extern	void CPU_START_OS();
	extern	void CPU_DISABLE_INTERUPT();
	extern	void CPU_CONFIG_SYSTEM_TIMER();
	extern	void CPU_CONFIG_INTERRUPT();
	extern  void CPU_TICK_CALLBACK(void);
	//01.Create System task 
	SystemTaskHandle = OS_TaskNew(OS_SystemTask,"System Task",configOS_MIN_STACK_DEEP,NULL,-1);
	if( SystemTaskHandle == NULL){
		OS_PRINTF("\r\n[OS_Start] Could not create System task. OS could not start");
		return;
	} 
	//02.Disable all interrupt.Global interrupt will be enable when Scheduler start
	CPU_DISABLE_INTERUPT();


	CPU_CONFIG_SYSTEM_TIMER(configOS_TICK_PER_SECOND);
	CPU_CONFIG_INTERRUPT();
	CPU_PERFORM_OTHER_CONFIG();
#if configPORT_REDIRECT_STDOUT_TO_UDP_PORT
#ifndef configOS_STDOUT_BUF_SIZE
	OS_register_terminal_driver(0, NULL, NULL, NULL, OS_UDP_Terminal_task, 2048);
#else
	OS_register_terminal_driver(0, NULL, NULL, NULL, OS_UDP_Terminal_task, configOS_STDOUT_BUF_SIZE);
#endif
#endif
	OS_AddTickCallbackFunction(CPU_TICK_CALLBACK);
	OS_PRINTF("\r\n\n\n\n");
	OS_PRINTF("\r\n*********************************");
	OS_PRINTF("\r\n[SYSYEM] emOS for embedded MCU");
	OS_PRINTF("\r\n[SYSYEM] Version %u.%u.%u",OS_MAIN_VERSION,OS_SUB_VERSION,OS_PATCH_VERSION);
	OS_PRINTF("\r\n[SYSYEM] Build at %s@%s",__DATE__,__TIME__);
	OS_PRINTF("\r\n[SYSYEM] Timer tick per sec: %u. Timer period: %u",configOS_TICK_PER_SECOND, CPU_GET_TIMER_PERIOD());
	OS_PRINTF("\r\n*********************************");
	OS_SelectNextTaskToRun_ns();
	SchedulerState =1;
	OS_Tick = 0;
	OS_TimeSecond = 0;
	CPU_START_OS();
} 
/********************************************************************************
 *   Function    : OS_TaskSleep
 *   Description : Sleep a task for a number of system tick
 *   Input       : Tick : Tick to sleep
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_TaskSleep(uint32_t Tick){
	OS_ENTER_CRITICAL();
	OS_SystemCheck(1);
	//System task handle could not sleep.
	//Also not sleep in IRQ or Scheduler is not ready
	if(CurrentTCB == SystemTaskHandle || SchedulerState ==0 || OS_INTERRUPT_CONTEXT){
		OS_EXIT_CRITICAL();
		return;
	}
	if(Tick >0){
		CurrentTCB->state = OS_TASK_STATE_SLEEP;
		CurrentTCB->timer = Tick;
	}
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/********************************************************************************
 *   Function    : OS_TaskSleepUntil
 *   Description :
 *   Input       :
 *   Output      : None
 *   Return      : None.
 *   Note        : Fix error when OS_Tick - *PreTime > SleepTime
 ********************************************************************************/
void OS_TaskSleepUntil(uint32_t * PreTime,uint32_t SleepTime){
	OS_ENTER_CRITICAL();
	if(OS_Tick - *PreTime > SleepTime){

	}else
		SleepTime-= (OS_Tick - *PreTime);
	*PreTime   = OS_Tick + SleepTime; 
	OS_EXIT_CRITICAL();
	OS_TaskSleep(SleepTime);
}
/********************************************************************************
 *   Function    : OS_TaskSleepMs
 *   Description : Sleep in ms unit
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_TaskSleepMs(uint32_t msec){
	OS_TaskSleep((msec*(configOS_TICK_PER_SECOND*65)+65535)>>16);
}
/********************************************************************************
 *   Function    : OS_TaskSuspend
 *   Description : Suspend current task
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_TaskSuspend(){
	OS_ENTER_CRITICAL();
	CurrentTCB->state = OS_TASK_STATE_SUSPEND;
	OS_SelectNextTaskToRun_ns();
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/********************************************************************************
 *   Function    : OS_SetTimeQuota
 *   Description : Set number of system tick task run continous before suspend for other task
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_SetTimeQuota(TCB_t * handle ,uint16_t quota){
	quota =quota>0?quota:1;
	OS_ENTER_CRITICAL();
	if(handle == NULL){
		CurrentTCB->quota = quota;
	}
	else{
		((TCB_t *)handle)->quota = quota;
	}
	OS_EXIT_CRITICAL();
}
/********************************************************************************
 *   Function    : OS_TaskDelete
 *   Description : Delete a task
 *   Input       : handle : handles of task need to delete.
 *                 Note: Pass NULL to delete current task
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_TaskDelete(const TCB_t * handle){
	OS_ENTER_CRITICAL();
	if(handle == NULL) handle = (TCB_t*)CurrentTCB;
	if(handle == SystemTaskHandle){
		OS_EXIT_CRITICAL();
		OS_Err_Handler(2,0);
	}
	else{
		((TCB_t *)handle)->state = OS_TASK_STATE_DELETED;
		OS_SelectNextTaskToRun_ns();
		OS_EXIT_CRITICAL();
		CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
	}
}
/********************************************************************************
 *   Function    : OS_SetPriority
 *   Description : Change task priority
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_SetPriority(const TCB_t * handle,int16_t Priority){
	OS_ENTER_CRITICAL();
	if(handle == NULL){	//CurrentTCB task ?
		CurrentTCB->priority =Priority;
		OS_SelectNextTaskToRun_ns();
	}
	else{				//Change other task.Not supported now
	}
	OS_EXIT_CRITICAL();
	CPU_REQUEST_CONTEXT_SWITCH(SchedulerState);
}
/********************************************************************************
 *   Function    : OS_GetCurrentTick
 *   Description : get current OS tick
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint32_t OS_GetCurrentTick(){
	return OS_Tick;
}
/********************************************************************************
 *   Function    : OS_GetCurrentTimeusec
 *   Description : get current OS time in usec
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint64_t OS_GetCurrentTimeusec(void){
	uint64_t result;
	result = (uint64_t)OS_Tick * 1000000/configOS_TICK_PER_SECOND +  (uint64_t)CPU_GET_TIMER_VAL()*1000000/ configOS_TICK_PER_SECOND/CPU_GET_TIMER_PERIOD();
	return result;
}
/********************************************************************************
 *   Function    : OS_GetCurrentTimeSec
 *   Description : get current OS time in sec
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint64_t OS_GetCurrentTimeSec(void){
	return OS_TimeSecond;
}
/********************************************************************************
 *   Function    : OS_GetCPU_Usage
 *   Description : get percent CPU run user task(0-100).
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint16_t OS_GetCPU_Usage(){
	return CPU_Usage;
}
/********************************************************************************
 *   Function    : OS_GetHeapRemain
 *   Description : get number of byte available in OS heap memory
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint32_t OS_GetHeapRemain(void){
#if configOS_CAL_CPU_HEAP_STATUS
	return heap_remaining;
#else
	return heap_free_left();
#endif
}
/********************************************************************************
 *   Function    : OS_GetHeapTotal
 *   Description : get total number of byte in OS heap memory
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
uint32_t OS_GetHeapTotal(void){
#if configOS_CAL_CPU_HEAP_STATUS
	return heap_total();
#else
	return heap_free_left();
#endif
}
/********************************************************************************
 *   Function    : OS_Err_Handler
 *   Description :
 *   Input       :
 *   Output      : None
 *   Return      : None.
 ********************************************************************************/
void OS_Err_Handler(uint32_t err_code, uint32_t err_addr){
	switch (err_code){
	case 0 :
		OS_PRINTF("\r\n[OS_Err_Handler] User Programming Error. Exit Critical region when we're not in");
		break;
	case 1 :
		OS_PRINTF(	"\r\n[OS_Err_Handler] User Programming Error. Task with name: \"%s\" exits without call task delete"
				"\r\n[OS_Err_Handler] OS will delete this task now",CurrentTCB->name);
		OS_TaskDelete(NULL);
		break;
	case 2 :
		OS_PRINTF("\r\n[OS_Err_Handler] OS_TaskDelete could not delete system task. System error");
		break;
	case 3:
		OS_PRINTF("\r\n[OS_Err_Handler] Invalid instruction at address 0x%08x. Instruction code 0x%08x",err_addr, *(uint32_t*) err_addr);
		OS_PRINTF("\r\n[OS_Err_Handler] Task cause error %s", CurrentTCB->name);
		OS_PRINTF("\r\n[OS_Err_Handler] Delete this task");
		OS_TaskDelete((const TCB_t * )CurrentTCB);
		break;
	case 4:
		OS_PRINTF("\r\n[OS_Err_Handler] Invalid Function context usage at instruction",err_addr);
		OS_PRINTF("\r\n[OS_Err_Handler] Task cause error %s", CurrentTCB->name);
		OS_PRINTF("\r\n[OS_Err_Handler] Delete this task");
		OS_TaskDelete((const TCB_t * )CurrentTCB);
		break;
	default	:
		OS_PRINTF("\r\n[OS_Err_Handler] SystemError. Unknown OPCODE");
		break;
	}
}
//=========================================================
void OS_ExitLoop_Error_Handler(){
	OS_Err_Handler(1,0);
}
//=========================================================
void OS_Critical_Error_Handler(){
	OS_Err_Handler(0,0);
}
//=========================================================
void OS_SaveTaskCallback(){
	OS_UpdateTaskRunTime((TCB_t*)CurrentTCB);
	CurrentTCB->run_remain = CurrentTCB->quota;
	OS_SystemCheck(1);
}
//=========================================================
void OS_LoadTaskCallback(){
	CurrentTCB->start_time = CPU_GET_TIMER_VAL();
	CurrentTCB->sub_priority =0;
	for(TCB_t * tcb = (TCB_t*)OS_TaskList; tcb!= NULL;tcb= tcb->next){
		if(tcb == CurrentTCB) continue;
		if(tcb->priority == CurrentTCB->priority) tcb->sub_priority++;
	}
	OS_SystemCheck(1);
}
//=========================================================
#if configOS_RUN_SYSTEM_CHECK
void OS_SystemCheck(uint32_t check_cur_task){
	TCB_t *t1 = R_TaskList;
	uint32_t found_cur_tcb = check_cur_task?0:1;
	if(SchedulerState ==0) found_cur_tcb = 1;
	while(t1!= NULL){
		if(t1 == CurrentTCB) found_cur_tcb = 1;
		//Check T_TaskList
		TCB_t *t2 = T_TaskList;
		while(t2!=NULL){
			if(t2 == CurrentTCB) found_cur_tcb = 1;
			while(t1 ==t2);	//Same task in difference list
			while(t2 == SystemTaskHandle); //system task sleep
			t2 = t2->next;
		}
		//Check Sem_list
#if configOS_ENABLE_SEM
		{
			extern volatile OS_Sem_t	*SemphrList;
			OS_Sem_t *sem= (OS_Sem_t*)SemphrList;
			while(sem != NULL){
				t2 = sem->ReceiveWaiting;
				while(t2!=NULL){
					if(t2 == CurrentTCB) found_cur_tcb = 1;
					while(t1 ==t2);	//Same task in difference list
					while(t2 == SystemTaskHandle); //system task sleep
					t2 = t2->next;
				}
				sem = sem->next;
			}
		}
#endif
#if configOS_ENABLE_MUTEX
		{
			extern volatile OS_Mutex_t	*MutexList;
			OS_Mutex_t* mutex= (OS_Mutex_t*)MutexList;
			while(mutex != NULL){
				t2 = mutex->ReceiveWaiting;
				while(t2!=NULL){
					if(t2 == CurrentTCB) found_cur_tcb = 1;
					while(t1 ==t2);	//Same task in difference list
					while(t2 == SystemTaskHandle); //system task sleep
					t2 = t2->next;
				}
				mutex = mutex->next;
			}
		}
#endif
		t1 = t1->next;
	}
	t1 = T_TaskList;
	while(t1!= NULL){
		//Check T_TaskList
		TCB_t *t2;
		//Check Sem_list
#if configOS_ENABLE_SEM
		{
			extern volatile OS_Sem_t	*SemphrList;
			OS_Sem_t *sem= (OS_Sem_t*)SemphrList;
			while(sem != NULL){
				t2 = sem->ReceiveWaiting;
				while(t2!=NULL){
					if(t2 == CurrentTCB) found_cur_tcb = 1;
					while(t1 ==t2);	//Same task in difference list
					while(t2 == SystemTaskHandle); //system task sleep
					t2 = t2->next;
				}
				sem = sem->next;
			}
		}
#endif
#if configOS_ENABLE_MUTEX
		{
			extern volatile OS_Mutex_t	*MutexList;
			OS_Mutex_t* mutex= (OS_Mutex_t*)MutexList;
			while(mutex != NULL){
				t2 = mutex->ReceiveWaiting;
				while(t2!=NULL){
					if(t2 == CurrentTCB) found_cur_tcb = 1;
					while(t1 ==t2);	//Same task in difference list
					while(t2 == SystemTaskHandle); //system task sleep
					t2 = t2->next;
				}
				mutex = mutex->next;
			}
		}
#endif
		t1 = t1->next;
	}
	t1 = S_TaskList;
	while(t1!= NULL){
		if(t1 == CurrentTCB) found_cur_tcb = 1;
		t1 = t1->next;
	}
	t1 = D_TaskList;
	while(t1!= NULL){
		if(t1 == CurrentTCB) found_cur_tcb = 1;
		t1 = t1->next;
	}
	while(found_cur_tcb ==0);

}
#endif
//=========================================================
void OS_SysTimer_Tick(){
	extern void 				CPU_CLEAR_TIMER_IRQ();
	extern uint32_t volatile	cpu_critical_nesting;

	CPU_CLEAR_TIMER_IRQ(); 
	CPU_OS_TICK_CALBACK();
	OS_Tick ++;
	if(CurrentTCB->run_remain) CurrentTCB->run_remain --;
	if(CurrentTCB->run_remain > CurrentTCB->quota) CurrentTCB->run_remain = CurrentTCB->quota;
	OS_SystemCheck(1);

	static uint32_t sec_sub_count =0;
	sec_sub_count ++;
	if(sec_sub_count >= configOS_TICK_PER_SECOND){
		sec_sub_count = 0;
		OS_TimeSecond ++;
	}
	//Only run if scheduler is ready
	if(SchedulerState ==0){
		NeedSwitchContext =0;
		return;
	}
	if(cpu_critical_nesting)  OS_PRINTF("\r\nBug %u",cpu_critical_nesting);
#if configOS_ENABLE_TIMER_TICK_CALLBACK
	for(uint32_t i=0;i< sizeof(tick_callback_list)/4; i++){
		if(tick_callback_list[i] != NULL) tick_callback_list[i]();
	}
#endif
	OS_CheckTerminal();
	//check timeout task
	for( TCB_t * cur = (TCB_t*)OS_TaskList; cur != NULL; cur = cur->next){
		if(		cur->state == OS_TASK_STATE_SLEEP 	||
				cur->state == OS_TASK_WAIT_MUTEX  	||
				cur->state == OS_TASK_WAIT_SEM		||
				cur->state == OS_TASK_WAIT_QUEUE
		){
			if(cur->timer)cur->timer --;
			if(cur->timer ==0){
				cur->flag = 0;
				cur->state = OS_TASK_STATE_READY;
			}
		}
	}
	//Select next task to run
	OS_SelectNextTaskToRun_ns();
	//Tell CPU change working context
	NeedSwitchContext = (CurrentTCB != NextTCB);
}
//================================================
uint8_t OS_GetSchedulerState(void){
	return SchedulerState;
}
//================================================
void OS_ShowSystemInfo(void){
	OS_ENTER_CRITICAL();
	TCB_t *cur_tcb;

	uint32_t heap_remain =  OS_GetHeapRemain();
	uint32_t ss = OS_Tick/configOS_TICK_PER_SECOND;
	uint32_t hh = ss/3600;
	uint32_t mm;
	ss -=hh*3600;
	mm = ss/60;
	ss -=mm*60;
	OS_PRINTF("\r\n======================================================");
	OS_PRINTF("\r\n CPU USAGE: %u%%. Mem remain %u(%u%%). System ticks %u. Boot time %dh_%02dm_%02ds", CPU_Usage,heap_remain, heap_remain*100/OS_GetHeapTotal(),(unsigned int) OS_Tick,hh,mm,ss);
	cur_tcb = (TCB_t*)OS_TaskList;
	while(cur_tcb!=NULL){
		if(cur_tcb->state >= OS_TASK_STATE_UNKOWN) cur_tcb->state = OS_TASK_STATE_UNKOWN;
		OS_PRINTF("\r\n[OS_TASK_INFO] %-20s: State %-10s. Stack Remain: %4u. CPU %3u%%. Pri: %3d", cur_tcb->name, task_state_string[cur_tcb->state], OS_GetTaskStackRemain((TCB_t*)cur_tcb),cur_tcb->usage, cur_tcb->priority);
		cur_tcb = cur_tcb->next;
	}
	OS_EXIT_CRITICAL();
}

//================================================================================
uint8_t volatile OS_OutbyteReady =1;
static volatile OS_terminal_driver_t terminal_driver[4] = {0};
static volatile uint32_t total_terminal_driver =0;
void OS_register_terminal_driver(uint32_t base,void (*init)(uint32_t),uint32_t (*is_tx_ready)(uint32_t),uint32_t (*send_byte)(uint32_t , uint8_t ),void (*task_code)(void*), uint32_t tx_buf_size){
	uint16_t i;
	if(total_terminal_driver >= sizeof(terminal_driver)/sizeof(OS_terminal_driver_t)){
		OS_PRINTF("\r\n[OS_register_terminal_driver] could not register new driver. Reached max driver %u ",total_terminal_driver);
		return;
	}
	if((base == 0 || is_tx_ready == NULL ||  send_byte == NULL) && task_code == NULL) return;
	//Check if driver is registered before
	for(i=0;i< total_terminal_driver;i++){
		if(terminal_driver[i].base_add == base && terminal_driver[i].is_tx_ready == is_tx_ready && terminal_driver[i].send_byte == send_byte && terminal_driver[i].task_code == task_code){
			OS_PRINTF("\r\n[OS_register_terminal_driver] Driver exits. Abort register new terminal driver");
			return;
		}
	}

	OS_ENTER_CRITICAL();
	terminal_driver[total_terminal_driver].tx_buf = OS_MemMalloc(tx_buf_size);
	if(terminal_driver[total_terminal_driver].tx_buf == NULL){
		OS_PRINTF("\r\n[OS_register_terminal_driver] could not register new driver. Out of memory ");
		OS_EXIT_CRITICAL();
		return;
	}
	terminal_driver[total_terminal_driver].base_add    = base;
	terminal_driver[total_terminal_driver].tx_wr_index = 0;
	terminal_driver[total_terminal_driver].tx_rd_index = 0;
	terminal_driver[total_terminal_driver].tx_buf_size = tx_buf_size;
	terminal_driver[total_terminal_driver].task_code   = task_code;
	terminal_driver[total_terminal_driver].init 	   = init;
	terminal_driver[total_terminal_driver].is_tx_ready = is_tx_ready;
	terminal_driver[total_terminal_driver].send_byte   = send_byte;
	if(init != NULL) (*init)(base);
	if(task_code != NULL) OS_TaskNew(task_code,"OS_Terminal_task",configOS_MIN_STACK_DEEP,(void*)&terminal_driver[total_terminal_driver],1);
	total_terminal_driver ++;
	OS_EXIT_CRITICAL();
	return;
}
//================================================================================

void OS_OutByte(char ch){
	uint32_t i;
	if(OS_OutbyteReady ==0) return;
	for(i=0;i< total_terminal_driver;i++){
		uint32_t wr_next = terminal_driver[i].tx_wr_index +1;
		if(wr_next >= terminal_driver[i].tx_buf_size)  wr_next = 0;
		if(wr_next != terminal_driver[i].tx_rd_index){
			terminal_driver[i].tx_buf[terminal_driver[i].tx_wr_index] = ch;
			terminal_driver[i].tx_wr_index = wr_next;
		}
	}
}
//================================================================================
void OS_CheckTerminal(){
	uint32_t i;
	for(i=0;i< total_terminal_driver;i++){
		if(terminal_driver[i].task_code) continue; //not process Terminal with thread processing
		while( terminal_driver[i].tx_rd_index != terminal_driver[i].tx_wr_index &&
				terminal_driver[i].is_tx_ready(terminal_driver[i].base_add)
		)
		{
			uint32_t rd_next = terminal_driver[i].tx_rd_index +1;
			if(rd_next >= terminal_driver[i].tx_buf_size)  rd_next = 0;
			terminal_driver[i].send_byte(terminal_driver[i].base_add,terminal_driver[i].tx_buf[terminal_driver[i].tx_rd_index]);
			terminal_driver[i].tx_rd_index = rd_next;
		}
	}
}
//=====================================================================
void _putchar(char character){
	OS_OutByte(character);
}
//=====================================================================
void outbyte(char c){
	OS_OutByte(c);
}
//======================================================================
int fputc(int ch, FILE *f) {
	OS_OutByte(ch);
	return (ch);
}
//======================================================================
int _write(int fd, const char * ptr, int len)
{
	int i;
	for(i=0;i<len;i++){
		fputc(ptr[i],stdout);
	}
	return len;
}
//============================================================================
#if configPORT_REDIRECT_STDOUT_TO_UDP_PORT >0
void OS_UDP_Terminal_task(void* param){
	int stdout_so = -1;
	struct sockaddr_in addr;
	OS_terminal_driver_t *info = (OS_terminal_driver_t *) param;
	if(info == NULL) return;
	while(net_is_ready()==0){
		OS_TaskSleep(100);
	}

	stdout_so = socket(AF_INET, SOCK_DGRAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(configPORT_REDIRECT_STDOUT_TO_UDP_PORT);
	addr.sin_addr.s_addr = IPV4_ADDRESS_VAL(224,0,0,1);
	if( stdout_so <0){
		OS_PRINTF("\r\n[TerminalUDPTask] Could not start terminal view via UDP service");
		OS_TaskDelete(NULL);
	}
	OS_PRINTF("\r\n[TerminalUDPTask] Startup");
	for(;;){
		OS_OutbyteReady =1;
		if(info->tx_rd_index != info->tx_wr_index ){
			if(info->tx_rd_index > info->tx_wr_index){
				uint32_t send_len = info->tx_buf_size - info->tx_rd_index;
				if(send_len > 1024) send_len = 1024;
				OS_OutbyteReady =0;
				sendto(stdout_so,(void*)(info->tx_buf + info->tx_rd_index) , send_len ,MSG_DONTWAIT,(struct sockaddr *)&addr,sizeof(addr));
				OS_OutbyteReady =1;
				info->tx_rd_index += send_len;
				if(info->tx_rd_index >= info->tx_buf_size) info->tx_rd_index = 0;
			}
			else {
				uint32_t send_len = info->tx_wr_index - info->tx_rd_index;
				if(send_len > 1024) send_len = 1024;
				OS_OutbyteReady =0;
				sendto(stdout_so,(void*)(info->tx_buf + info->tx_rd_index) , send_len ,MSG_DONTWAIT,(struct sockaddr *)&addr,sizeof(addr));
				OS_OutbyteReady =1;
				info->tx_rd_index += send_len;
				if(info->tx_rd_index >= info->tx_buf_size) info->tx_rd_index = 0;
			}
		}
		OS_TaskSleep(configOS_TICK_PER_SECOND/10+1);
	}
}
#endif
//==============================================================
void OS_AddTickCallbackFunction( void (*fcn)(void)){
	uint32_t i;
	for(i=0;i< sizeof(tick_callback_list)/4;i++){
		if(tick_callback_list[i] == NULL ){
			tick_callback_list[i] = fcn;
			return;
		}
	}
}
//=========================================
#ifndef useconds_t
#define useconds_t	uint32_t
#endif
int  OS_usleep (useconds_t us){
	uint32_t ticks;
	if (OS_GetSchedulerState() == 0)   CPU_USLEEP (us);
	ticks     = us /(1000000/ configOS_TICK_PER_SECOND);
	if(ticks) OS_TaskSleep(ticks);
	us -= ticks*(1000000/ configOS_TICK_PER_SECOND);
	if(us) CPU_USLEEP(us);
	return us;
}
//=========================================
unsigned OS_sleep (unsigned int __seconds){
	if (OS_GetSchedulerState() == 0)   CPU_USLEEP (__seconds*1000000);
	OS_TaskSleep(__seconds*configOS_TICK_PER_SECOND);
	return __seconds;
}
