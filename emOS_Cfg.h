/*
 * emOS_Cfg.h
 *
 *  Created on: Apr 22, 2020
 *      Author: kyle
 */

#ifndef EMOS_PORT_EMOS_CFG_H_
#define EMOS_PORT_EMOS_CFG_H_

//************************************************************
//==================== OS Parameter =========================*
//************************************************************
#define configOS_TICK_PER_SECOND               	1000
#define configOS_MIN_STACK_DEEP               	2048
#define configOS_SUPPORT_RUNTIME_STATUS         1
#define configOS_HEAP_SIZE						(1024*1024*4)
#define configOS_RUNTIME_UPDATE_TICK         	(configOS_TICK_PER_SECOND)
#define configMAX_TASK_NAME_LEN              	16
#define configOS_DEBUG_LEVEL                    0
#define configOS_ENABLE_MUTEX                   1
#define configOS_ENABLE_SEM                     1
#define configOS_ENABLE_QUEUE                 	1
#define configOS_ENABLE_IDLE_CALLBACK			1
#define configOS_ENABLE_TIMER_TICK_CALLBACK		1
#define configOS_ENABLE_CONTEXT_SWITCH_HOOK 	1

//#define	configPORT_REDIRECT_STDOUT_TO_UDP_PORT	9994
#define configOS_STDOUT_BUF_SIZE				(1024*4)

#define	configOS_SYSTEM_CALL_PRIORITY			0x2f
#define CPU_TYPE_UBLAZE							1
#define CPU_TYPE_STM32_CM3						0
#define CPU_TYPE_STM32_CM4						0
#define CPU_TYPE_STM32_CM7						0
#define CPU_TYPE_NIOS2							0
#define CPU_TYPE_A53							0
#define CPU_TYPE_A53							0
#define CPU_TYPE_XMC4700						0

#define OS_PRINTF	h_printf



#endif /* EMOS_PORT_EMOS_CFG_H_ */
