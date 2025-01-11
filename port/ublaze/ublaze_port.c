/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module :  ublaze cpu porting         		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 *   Version:   1.0                    			*
 ************************************************/

// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made: 
//		V1.0  | Hoangdx	    |Dec,26,2021      |Change NeedSwitchContext from uint8_t to uint32_t
// ============================================================================
#include "../../emOS.h"
#if CPU_TYPE_UBLAZE
#include <stdint.h>
#include "xparameters.h"
#include "xtmrctr.h"
#include "xil_printf.h"
#include "xintc.h"
#ifdef STDOUT_BASEADDRESS
#include "xuartlite_l.h"
#endif
#include "unistd.h"


//==  SOME MACRO TO GET MODULE INFO =====================================
#include "ublaze_macro.h"

//============================================================
//== MANUAL CONFIG TEMPLATE ==================================
//============================================================
#ifndef SYS_TIMER_NAME
//#define SYS_TIMER_NAME
#endif

#ifndef SYS_INTERRUPT_NAME
//#define SYS_INTERRUPT_NAME
#endif

#ifndef OS_PRINTF
#define OS_PRINTF xil_printf
#endif
//=== CHECK  PARAM ================================
#ifndef SYS_TIMER_NAME
#error "SYSTEM TIMER NOT FOUND. PLEASE MANUAL CONFIG ABOVE"
#endif
#ifndef SYS_INTERRUPT_NAME
#error "SYSTEM INTERRUPT NOT FOUND. PLEASE MANUAL CONFIG ABOVE"
#endif
//=========================================
uint32_t volatile			cpu_enter_interrupt =0;
uint32_t volatile			cpu_critical_nesting =0;
static uint32_t volatile	timer_period = 0,internal_tick=1;
static XTmrCtr 				system_timer_inst;
void CPU_CLEAR_TIMER_IRQ();
//=========================================
void OS_ENTER_CRITICAL(){
	microblaze_disable_interrupts();
	cpu_critical_nesting ++;
}
//=========================================
void CPU_CRICIAL_LEVEL_ERROR(){
	xil_printf("\r\n CPU CRITICAL LEVEL ERROR. OS BUG. STOP HERE %s:%d",__FILE__,__LINE__);
	while(1);
}
//=========================================
void CPU_EXIT_TASK_ERROR(){
	xil_printf("\r\n TASK EXIT BUG.STOP HERE %s:%d",__FILE__,__LINE__);
	while(1);
}
//=========================================
void OS_EXIT_CRITICAL(){
	if(cpu_critical_nesting){
		cpu_critical_nesting --;
	}
	else{
		CPU_CRICIAL_LEVEL_ERROR();
	}
	if(cpu_critical_nesting ==0 && cpu_enter_interrupt ==0){
		microblaze_enable_interrupts();
	}
}
//=========================================
void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State){
	if(State) {
		if(cpu_critical_nesting ==0 && cpu_enter_interrupt ==0){
			extern void software_context_switch();
			microblaze_disable_interrupts();
			asm volatile (	"bralid r14, 0x08		\n\t" \
					"or r0, r0, r0			\n\t" );
		}
		else{
			extern volatile 		uint32_t			NeedSwitchContext;
			NeedSwitchContext =1;
		}
	}
}
//=============================================================
uint64_t CPU_GET_TIMER_VAL(void){
	return (uint64_t )internal_tick*timer_period + (timer_period-1-XTmrCtr_GetValue(&system_timer_inst,0));
}
//=============================================================
uint32_t CPU_GET_TIMER_PERIOD(void){
	return timer_period;
}
//=================================================
void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param){
	//
	uint32_t *SP = tcb->pStack;
	extern void *_SDA2_BASE_, *_SDA_BASE_;
	const uint32_t ulR2 = ( uint32_t ) &_SDA2_BASE_;
	const uint32_t ulR13 = ( uint32_t ) &_SDA_BASE_;

	while(((uint32_t)SP &0x07))	SP--; //Align 8 byte
	//FPU ???
#if  XPAR_MICROBLAZE_USE_FPU
	SP --;*SP = 0;
#endif
	//CPU_STATUS_REGISTER.CPU MODE MUST INIT Before REGISTER a task
	SP--; *SP = mfmsr() | 0x02;	//DEFAULT TASK ENABLE INTERRUPT
	SP--; *SP = ulR2;	//SMALL_RO_DATA_AREA_DELICATE_REGISTER (R2)
	SP--; *SP =0;		//R3
	SP--; *SP =0;		//R4
	SP--; *SP =(uint32_t)param;	//R5. FUNCTION PARAMERTER
	SP--; *SP =0;		//R6
	SP--; *SP =0;		//R7
	SP--; *SP =0;		//R8
	SP--; *SP =0;		//R9
	SP--; *SP =0;		//R10
	SP--; *SP =0;		//R11
	SP--; *SP =0;		//R12
	SP--; *SP =ulR13;	//R13 //SMALL_RW_DATA_AREA_DELICATE_REGISTER (R13)
	SP--; *SP =(uint32_t)TaskCode;	//R14
	SP--; *SP = (uint32_t)TaskCode;	//R15
	SP--; *SP = (uint32_t)CPU_EXIT_TASK_ERROR;	//R16
	SP--; *SP = (uint32_t)TaskCode;				//R17 TRAP RETURN
	SP--; *SP = (uint32_t)CPU_EXIT_TASK_ERROR;	//R18.
	SP--; *SP = 0;								//R19. Check later
	SP--; *SP = 0;								//R20.
	SP--; *SP = 0;								//R21.
	SP--; *SP = 0;								//R22.
	SP--; *SP = 0;								//R23.
	SP--; *SP = 0;								//R24.
	SP--; *SP = 0;								//R25.
	SP--; *SP = 0;								//R26.
	SP--; *SP = 0;								//R27.
	SP--; *SP = 0;								//R28.
	SP--; *SP = 0;								//R29.
	SP--; *SP = 0;								//R30.
	SP--; *SP = 0;								//R31.
	tcb->pStack = SP;
}
//=========================================
void CPU_START_OS(){
	extern void emOS_Load_New_Task();
	emOS_Load_New_Task();
}
//=========================================
void CPU_DISABLE_INTERUPT(){
	microblaze_disable_interrupts();
}
//=========================================
XIntc emOS_INTERRUPT_info;
static uint8_t interrupt_init_status = 0;
//=========================================
void CPU_CONFIG_INTERRUPT_CONTROLLER(){
	if(interrupt_init_status ==0){
		interrupt_init_status =1;
		if(XIntc_Initialize( &emOS_INTERRUPT_info, UBLAZE_GET_ID(SYS_INTERRUPT_NAME)) != XST_SUCCESS){
			OS_PRINTF("\r\n[CPU_CONFIG_INTERRUPT_CONTROLLER] Interrupt controller init error.Stop here %s:%d",__FILE__,__LINE__);
			while(1);
		}
		OS_PRINTF("\r\n[CPU_CONFIG_INTERRUPT_CONTROLLER] Config interrupt controller driver to process all IRQ at a time");
		XIntc_SetIntrSvcOption( emOS_INTERRUPT_info.BaseAddress, XIN_SVC_ALL_ISRS_OPTION );
		OS_PRINTF("\r\n[CPU_CONFIG_INTERRUPT_CONTROLLER] Start interrupt controller");
		XIntc_Start(&emOS_INTERRUPT_info, XIN_REAL_MODE );
	}
}
//=========================================
void CPU_CONFIG_INTERRUPT(){
	cpu_critical_nesting =0;
	CPU_CONFIG_INTERRUPT_CONTROLLER();
	microblaze_enable_interrupts();
}
//=========================================
void  CPU_OS_TICK_CALBACK(){
	internal_tick ++;
}
//=========================================
void CPU_USLEEP(uint64_t us){
	uint64_t start_time = CPU_GET_TIMER_VAL();
	uint64_t wait_time = UBLAZE_GET_FREQ(SYS_TIMER_NAME)/1000000*us;
	while(1){
		uint64_t cur_time = CPU_GET_TIMER_VAL();
		uint64_t delta_time = cur_time - start_time;
		if(delta_time >= wait_time) break;
	}
}
//=========================================
void CPU_CONFIG_SYSTEM_TIMER(){
	//Config timer interval and timer callback
	extern uint32_t OS_SysTimer_Tick();
	timer_period = UBLAZE_GET_FREQ(SYS_TIMER_NAME)/configOS_TICK_PER_SECOND;
	if(XST_SUCCESS != XTmrCtr_Initialize(&system_timer_inst,UBLAZE_GET_ID(SYS_TIMER_NAME))){
		OS_PRINTF("\\r\n[CPU_CONFIG_SYSTEM_TIMER] SYSTEM TIMER INIT ERROR. OS COULD NOT START. STOP HERE %s:%d",__FILE__,__LINE__);
		while(1);
	}
	OS_PRINTF("\r\n[CPU_CONFIG_SYSTEM_TIMER] Found timer at address 0x%08x. Config timer with interval %d",system_timer_inst.BaseAddress,timer_period);
	XTmrCtr_Reset(&system_timer_inst,0);

	XTmrCtr_SetResetValue(&system_timer_inst,0, timer_period-1);	//RELOAD VALUE OF TIMER0
	//INIT MODE. COUNT DOWN. INTERRUPT. AUTO RELOAD
	XTmrCtr_SetOptions(&system_timer_inst,0, XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_INT_MODE_OPTION);
	//Register call back
	CPU_CONFIG_INTERRUPT_CONTROLLER();	//Ensure interrupt controller setup rountine is done before register other handler

	XIntc_Connect(&emOS_INTERRUPT_info, UBLAZE_GET_IRQ(SYS_TIMER_IRQ_NAME), (XInterruptHandler)OS_SysTimer_Tick, NULL);
	XIntc_Enable(&emOS_INTERRUPT_info, UBLAZE_GET_IRQ(SYS_TIMER_IRQ_NAME));
	CPU_CLEAR_TIMER_IRQ();
	XTmrCtr_Start( &system_timer_inst, 0 );
	OS_PRINTF("\r\n[CPU_CONFIG_SYSTEM_TIMER] Init timer done");
}
//=========================================
void CPU_CLEAR_TIMER_IRQ(){
	XTmrCtr_SetControlStatusReg( UBLAZE_GET_BASE(SYS_TIMER_NAME), 0, XTmrCtr_GetControlStatusReg( UBLAZE_GET_BASE(SYS_TIMER_NAME), 0 ) );
}
//=========================================
#if STDOUT_BASEADDRESS
static __inline uint32_t XUartLite_IsTransmitReady(uint32_t base){
	return XUartLite_IsTransmitFull(base)?0:1;
}
static __inline uint32_t XUartLite_write_fifo(uint32_t base, uint8_t ch){
	XUartLite_WriteReg(base, XUL_TX_FIFO_OFFSET, ch);
	return 0;
}
void CPU_PERFORM_OTHER_CONFIG(void){
	OS_register_terminal_driver(STDOUT_BASEADDRESS,NULL,XUartLite_IsTransmitReady,XUartLite_write_fifo,NULL,2048);
}
#endif
#endif
