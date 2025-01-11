#ifndef   emOS_H__
#define   emOS_H__

#include <stdio.h>
#include <stdint.h>
#include "emOS_Cfg.h"
#include "emOS_ATTT.x"
//**************************************************************************** 
//=======================   OS Type definition ===============================*
//****************************************************************************
#ifndef   ALIGNED_SIZE
#define   ALIGNED_SIZE(x)            (((uint32_t)(x) + 3)&0xfffffffcUL)
#endif

#define OS_ERR_NONE          0
#define OS_ERR_ERR          -1
#define OS_ERR_NOT_FOUND    -2
#define OS_ERR_OUT_OF_MEM   -3
#define	configOS_RUN_SYSTEM_CHECK	0

#define OS_MAIN_VERSION		3
#define OS_SUB_VERSION		1
#define OS_PATCH_VERSION	3

typedef enum {
	OS_TASK_STATE_READY=0,
	OS_TASK_STATE_SUSPEND,
	OS_TASK_STATE_SLEEP,
	OS_TASK_WAIT_MUTEX,
	OS_TASK_WAIT_SEM,
	OS_TASK_WAIT_QUEUE,
	OS_TASK_STATE_DELETED,
	OS_TASK_STATE_UNKOWN	//Must at the end of list
}OS_TASK_STATE_STATE;
//**************************************************************************** 
//=======================   OS Default configuration =========================*
//****************************************************************************
#ifndef configOS_DEBUG_LEVEL
#define configOS_DEBUG_LEVEL 0
#endif

#ifndef configOS_MIN_STACK_DEEP
#define configOS_MIN_STACK_DEEP 128
#endif

#ifndef configMAX_TASK_NAME_LEN
#define configMAX_TASK_NAME_LEN 16
#endif

#ifndef configOS_ENABLE_MUTEX
#define configOS_ENABLE_MUTEX   1
#endif

#ifndef configOS_ENABLE_SEM
#define configOS_ENABLE_SEM     1
#endif

#ifndef configOS_ENABLE_QUEUE
#define configOS_ENABLE_QUEUE   16
#endif

#ifndef configOS_TICK_PER_SECOND
#define configOS_TICK_PER_SECOND  1000
#endif

#if configOS_TICK_PER_SECOND >1000
#error "Invalid configOS_TICK_PER_SECOND setting. Maximum value is 1000"
#endif

//**************************************************************************** 
//=======================   OS Debug printf support =========================*
//****************************************************************************
#if  configOS_DEBUG_LEVEL >0
#define OS_DEBUG(level,format)   if((level) <= configOS_DEBUG_LEVEL) OS_PRINTF format ;
#else
#define OS_DEBUG(...)
#endif

//--------------Task control block--------------------------------------------
typedef struct TaskControlBlock{
	uint32_t                *pStack;      			// Stack pointer
	uint32_t                *pStack_Mark;   		// End of stack
	uint32_t            	StackDeep;
	uint32_t            	StackRemain;
	struct   TaskControlBlock     *next;			//Next TCB block
	void*                  	wait_param;				//SEM/MUTEX task is waiting for
	uint32_t				wait_seq;				//Wait sequence to perform First Request First Severed
	void*					rx_buf;					//Rx buffer when wait for queue/mailbox
	uint32_t               	timer;					//Run/Sleep remain counter
	uint32_t				start_time;
	uint32_t				runtime;				//Total tick cpu run per monitor cycle
	uint8_t         		quota; 					//Continuous cycle when multiple task has same priority
	uint8_t					run_remain;
	int8_t             		priority;
	uint8_t					sub_priority;			//Round robin priority
	uint8_t              	flag;
	uint8_t             	usage;
	OS_TASK_STATE_STATE		state;
	char             		ARRAY_INDEX(name,ALIGNED_SIZE(configMAX_TASK_NAME_LEN+1));//   Task name
	//-->>> Task Stack is located here ----------------------
}TCB_t;
typedef TCB_t    TaskHandle;
typedef void   (*TaskFunction_t)( void * );
//------------------------------------------------------------
#if configOS_ENABLE_MUTEX
typedef struct OS_Mutex_struct{
	struct OS_Mutex_struct *next;
	TCB_t	*Holder;
	uint32_t lock_time;
	uint16_t RecursiveLevel;
}OS_Mutex_t;
#endif
//------------------------------------------------------------
#if configOS_ENABLE_SEM
typedef struct OS_Sem_struct{
	struct OS_Sem_struct *next;
	uint16_t  	count;
	uint16_t  	max_count;
}OS_Sem_t;
#endif
//------------------------------------------------------------
#if configOS_ENABLE_QUEUE
typedef struct OS_Queue_struct{
	struct OS_Queue_struct *next;
	uint8_t*            buf;
	uint16_t			wr_index,rd_index;
	uint16_t			buf_size;
	uint16_t            element_size;
	uint16_t			byteAvailable;
}OS_Queue_t;
#endif
//------------------------------------------------------------
struct OS_terminal_driver{
	uint32_t	base_add;
	uint8_t 	*tx_buf;
	uint32_t 	tx_wr_index,tx_rd_index,tx_buf_size;
	void 		(*task_code)(void* param);
	void 		(*init)(uint32_t base);
	uint32_t 	(*is_tx_ready)(uint32_t base);
	uint32_t 	(*send_byte)(uint32_t base, uint8_t ch);
	uint8_t		lock;
};
typedef struct OS_terminal_driver    OS_terminal_driver_t;
//**************************************************************************** 
//====================== API Function Prototype ==============================*
//****************************************************************************
int h_printf( const char *format_const, ...);
//==== MEM API ===========
void *	     	OS_MemMalloc(uint32_t	size);
uint32_t		OS_MemFree(void*	p);
uint32_t		OS_MemSize(void*	p);
//==== Task API ===========
TCB_t*   		OS_TaskNew(TaskFunction_t,const char * , uint16_t, void*, int8_t);
void     		OS_Start(void);
void     		OS_TaskSleep(uint32_t Tick);
void     		OS_TaskSleepMs(uint32_t msec);
void       		OS_TaskSleepUntil(uint32_t * PreTime,uint32_t SleepTime);
void          	OS_TaskSuspend(void);
void 			   OS_SetTimeQuota(TCB_t * handle ,uint16_t quota);
void          	OS_TaskDelete(const TCB_t * handle);
void 			   OS_SetPriority(const TCB_t * handle,int16_t Priority);
uint32_t       OS_GetTaskStackRemain(TCB_t* handle);
char* 			OS_GetTaskName(void);
uint32_t    	OS_GetCurrentTick(void);
uint64_t    	OS_GetCurrentTimeusec(void);
uint64_t 		OS_GetCurrentTimeSec(void);
uint16_t     	OS_GetCPU_Usage(void);
uint8_t			OS_GetSchedulerState(void);


void 			OS_ShowSystemInfo(void);
void 			OS_Heapstatus(void);
//=== Semaphore/mutex API =========================
#if configOS_ENABLE_MUTEX
OS_Mutex_t 		*OS_MutexNew(void);
uint8_t     	OS_MutexTake(OS_Mutex_t *m,uint32_t Timeout);
uint8_t     	OS_MutexRelease(OS_Mutex_t *m);
void        	OS_MutexDelete(OS_Mutex_t *m);
#endif
//---- Event internal API. User must not using it direct from application.Use above macro instead
#if configOS_ENABLE_SEM
OS_Sem_t   		*OS_SemNew(uint16_t InitCount);
void        	OS_SemDelete(OS_Sem_t* Sem);
uint32_t      	OS_SemPend(OS_Sem_t * Sem, uint32_t Timeout);
uint32_t      	OS_SemPost(OS_Sem_t *Sem);
void          	OS_SemSet(OS_Sem_t * Sem, uint16_t Count);
#endif
//=== Queue API ================================== 
#if configOS_ENABLE_QUEUE
#define   OS_MboxNew(x,y)   	OS_QueueNew((x),(y))
#define   OS_MboxPend(x,y,z)   	OS_QueuePend((x),(y),(z))
#define   OS_MboxPost(x,y)   	OS_QueuePost((x),(y))
#endif
//== TERMINAL DRIVER API ==========================
void OS_register_terminal_driver(uint32_t base,void (*init)(uint32_t),uint32_t (*is_tx_ready)(uint32_t),uint32_t (*send_byte)(uint32_t , uint8_t ),void (*task_code)(void*), uint32_t tx_buf_size);
void OS_CheckTerminal();
void OS_OutByte(char ch);
void OS_UDP_Terminal_task(void* param);
//==============================================================
void OS_AddTickCallbackFunction( void (*fcn)(void));

#endif
