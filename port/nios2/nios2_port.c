/************************************************
 *   RTOS for ARM M3-M4 CPU                   	*
 *   Module :  nios2 cpu porting         		*
 *   Author : Hoangdx1@viettel.com.vn        	*
 *   Version:   1.1                    			*
 ************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V1.1  | Hoangdx	    |Dec,10,2021      |Add CPU_TICK_CALLBACK
//		V1.1  | Hoangdx	    |Dec,17,2021      |Fix incorrect function jtag_uart_process
//		V1.1  | Hoangdx	    |Dec,26,2021      |Change NeedSwitchContext from uint8_t to uint32_t
// ============================================================================


#include "../../emOS_Cfg.h"
#if CPU_TYPE_NIOS2

#include <stdint.h>
#include "../../emOS.h"
#include "nios2.h"
#include "sys/alt_irq.h"
#include "priv/alt_busy_sleep.h"
#include "altera_avalon_timer_regs.h"
#include "system.h"

#ifdef ALT_STDOUT_PRESENT
#include "altera_avalon_jtag_uart_regs.h"
#include "altera_avalon_jtag_uart.h" 
#endif
#include "string.h"

#define NIOS2_JTAG_UART_BASE	ALT_STDOUT_BASE

#define __NIOS2_GET_BASE(name) name##_BASE
#define NIOS2_GET_BASE(name) __NIOS2_GET_BASE(name)

#define __NIOS2_GET_IRQ(name) name##_IRQ
#define NIOS2_GET_IRQ(name) __NIOS2_GET_IRQ(name)


#define __NIOS2_GET_FREQ(name) name##_FREQ
#define NIOS2_GET_FREQ(name) __NIOS2_GET_FREQ(name)
//=========================================
uint32_t volatile				cpu_critical_nesting =0;
static uint32_t volatile		cpu_context =0;
static uint32_t 				timer_period = 1;
static uint32_t 				timer_tick=0;
//=========================================
int32_t OS_ENTER_CRITICAL(void){
	uint32_t cur_cpu_context;
	NIOS2_READ_STATUS (cur_cpu_context);
	NIOS2_WRITE_STATUS (cur_cpu_context & ~NIOS2_STATUS_PIE_MSK);
	if(cpu_critical_nesting ==0){
		cpu_context =cur_cpu_context;
	}
	cpu_critical_nesting ++;
	return cpu_critical_nesting;
}
//=========================================
void CPU_CRICIAL_LEVEL_ERROR(){
	while(1);
}
//=========================================
int32_t OS_EXIT_CRITICAL(void){
	if(cpu_critical_nesting){
		cpu_critical_nesting --;
	}
	else{
		CPU_CRICIAL_LEVEL_ERROR();
	}
	if(cpu_critical_nesting ==0){
		NIOS2_WRITE_STATUS (cpu_context);
	}
	return cpu_critical_nesting;
}
//=========================================
void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State){
	if(State) {
		extern volatile		uint32_t		OS_INTERRUPT_CONTEXT;
		if(cpu_critical_nesting ==0 && OS_INTERRUPT_CONTEXT ==0){
			__asm volatile ("trap");
		}
		else{
			extern volatile 		uint32_t			NeedSwitchContext;
			NeedSwitchContext =1;
		}
	}
}
//=========================================
uint64_t CPU_GET_TIMER_VAL(void){
	uint32_t timer_counter;
	IOWR_ALTERA_AVALON_TIMER_SNAPL(NIOS2_GET_BASE(ALT_SYS_CLK),0);
	timer_counter = (IORD_ALTERA_AVALON_TIMER_SNAPH(NIOS2_GET_BASE(ALT_SYS_CLK)) <<16) + IORD_ALTERA_AVALON_TIMER_SNAPL(NIOS2_GET_BASE(ALT_SYS_CLK));
	timer_counter = timer_period-timer_counter;
	return timer_counter + (uint64_t)timer_period*timer_tick;
}
//=========================================
uint32_t CPU_GET_TIMER_PERIOD(){
	return timer_period;
}
//=========================================
static void Nios2_GetGlobalPointer( unsigned long *ulValue )
{
	asm( "stw gp, (%0)" :: "r"(ulValue) );
}
//=================================================
void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param){
	//
	uint32_t *SP = tcb->pStack;
	uint32_t GP;

	Nios2_GetGlobalPointer(&GP);
	SP -= 116;
	SP[0] = 0;	//RA
	SP[1] = 0;	//DIV_GAP???
	SP[2] = 1;	//R1
	SP[3] = 2;	//R2
	SP[4] = 3;	//R3
	SP[5] = (uint32_t)param;	//R4
	SP[6] = 5;	//R5
	SP[7] = 6;	//R6
	SP[8] = 7;	//R7
	SP[9] = 8;	//R8
	SP[10] = 9;	//R9
	SP[11] = 10;	//R10
	SP[12] = 11;	//R11
	SP[13] = 12;	//R12
	SP[14] = 13;	//R13
	SP[15] = 14;	//R14
	SP[16] = 15;	//R15
	SP[17] = 1;	//estatus
	SP[18] = (uint32_t)TaskCode;	//ea
	SP[19] = 16;	//R16
	SP[20] = 17;	//R17
	SP[21] = 18;	//R18
	SP[22] = 19;	//R19
	SP[23] = 20;	//R20
	SP[24] = 21;	//R21
	SP[25] = 22;	//R22
	SP[26] = 23;	//R23
	SP[27] = GP;	//GP
	SP[28] =(uint32_t)SP;//SP
	tcb->pStack = SP;
}
//=========================================
void CPU_START_OS(){
	extern void nios2_emOS_Start_OS();
	nios2_emOS_Start_OS();
}
//=========================================
void CPU_DISABLE_INTERUPT(){
	uint32_t cur_cpu_context;
	NIOS2_READ_STATUS (cur_cpu_context);
	NIOS2_WRITE_STATUS (cur_cpu_context & ~NIOS2_STATUS_PIE_MSK);
}
//=========================================
void CPU_CONFIG_SYSTEM_TIMER(){
	//Config timer interval and timer callback
	extern uint32_t OS_SysTimer_Tick();
	timer_period = NIOS2_GET_FREQ(ALT_SYS_CLK)/ configOS_TICK_PER_SECOND;
	OS_PRINTF("\r\n[OS] Timer period %u", timer_period);
	IOWR_ALTERA_AVALON_TIMER_CONTROL( NIOS2_GET_BASE(ALT_SYS_CLK), ALTERA_AVALON_TIMER_CONTROL_STOP_MSK );


	IOWR_ALTERA_AVALON_TIMER_PERIODH(NIOS2_GET_BASE(ALT_SYS_CLK),(timer_period-1)>>16);
	IOWR_ALTERA_AVALON_TIMER_PERIODL(NIOS2_GET_BASE(ALT_SYS_CLK),(timer_period-1)&0xffff);

#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
	alt_ic_isr_register(	NIOS2_GET_IRQ(ALT_SYS_CLK),
			NIOS2_GET_IRQ(ALT_SYS_CLK),
			(alt_isr_func)OS_SysTimer_Tick,
			NULL,
			NULL);
#else
	alt_irq_register (NIOS2_GET_IRQ(ALT_SYS_CLK),
			NULL,
			(alt_isr_func)OS_SysTimer_Tick);

#endif
	IOWR_ALTERA_AVALON_TIMER_CONTROL( NIOS2_GET_BASE(ALT_SYS_CLK), ALTERA_AVALON_TIMER_CONTROL_CONT_MSK | ALTERA_AVALON_TIMER_CONTROL_START_MSK | ALTERA_AVALON_TIMER_CONTROL_ITO_MSK );
}
//=========================================
void CPU_CONFIG_INTERRUPT(){
	uint32_t cur_cpu_context;
	cpu_critical_nesting =0;
	NIOS2_READ_STATUS (cur_cpu_context);
	//	NIOS2_WRITE_STATUS (cur_cpu_context | NIOS2_STATUS_PIE_MSK);
	NIOS2_WRITE_STATUS (cur_cpu_context &(~ NIOS2_STATUS_PIE_MSK));
}
//=========================================
void CPU_CLEAR_TIMER_IRQ(){
	IOWR_ALTERA_AVALON_TIMER_STATUS( NIOS2_GET_BASE(ALT_SYS_CLK), ~ALTERA_AVALON_TIMER_STATUS_TO_MSK );
	timer_tick++;
}
//=========================================
uint32_t CPU_USLEEP(uint32_t usec){
	if(timer_tick){
		uint32_t start_time = CPU_GET_TIMER_VAL();
		uint32_t wait_time  = usec*(NIOS2_GET_FREQ(ALT_SYS_CLK)/1000000);
		while(1){
			uint32_t cur_time = CPU_GET_TIMER_VAL();
			uint32_t delta_time = cur_time-start_time;
			if(delta_time >= wait_time) break;
		}
	}
	else{
		while(usec--){
			uint32_t volatile i;
			for(i=0;i< ALT_CPU_CPU_FREQ/8000000;i++);
		}
	}
	return usec;
}
//=========================================
int usleep_t (uint32_t us){
	uint32_t ticks;
	if (OS_GetSchedulerState() == 0) return CPU_USLEEP (us);
	ticks     = us /(1000000/ configOS_TICK_PER_SECOND);
	if(ticks) OS_TaskSleep(ticks);
	us -= ticks*(1000000/ configOS_TICK_PER_SECOND);
	if(us) CPU_USLEEP(us);
	return 0;
}

//=========================================
//==== JTAG UART WRITE DRIVER =============
//=========================================
#ifdef ALT_STDOUT_PRESENT

#ifndef configOS_STDOUT_BUF_SIZE
#define configOS_STDOUT_BUF_SIZE	(1024*8)
#endif

#if (configOS_STDOUT_BUF_SIZE-1) & (configOS_STDOUT_BUF_SIZE)
#error "Invalid configOS_STDOUT_BUF_SIZE setting. Must 2^N"
#endif
//================================================================================================
void jtag_uart_init(uint32_t base ){
	IOWR_ALTERA_AVALON_JTAG_UART_CONTROL(base,0);	//DISABLE INTERRUPT
}
//================================================================================================
int altera_avalon_jtag_uart_write(altera_avalon_jtag_uart_state* sp,  const char * ptr, int count, int flags)
{
	int32_t i;
	for(i=0;i<count;i++){
		OS_OutByte(ptr[i]);
	}
	return i;
}
//==========================================================================
void alt_avalon_timer_sc_init (void* base, alt_u32 irq_controller_id, alt_u32 irq, alt_u32 freq){

}
//=========================================
uint32_t jtag_uart_is_ready(uint32_t base){
	unsigned int control = IORD_ALTERA_AVALON_JTAG_UART_CONTROL(base);
	uint16_t num_space_available = control>>16;
	return num_space_available?1:0;
}
//=========================================
uint32_t jtag_uart_write_char(uint32_t base, uint8_t ch){
	IOWR_ALTERA_AVALON_JTAG_UART_DATA(base,ch);
	return 1;
}
//=========================================
static uint8_t terminal_init_state =0;
void CPU_PERFORM_OTHER_CONFIG(){
	if(terminal_init_state) return;
	terminal_init_state =1;
	OS_register_terminal_driver(NIOS2_JTAG_UART_BASE,jtag_uart_init,jtag_uart_is_ready,jtag_uart_write_char,NULL,configOS_STDOUT_BUF_SIZE);
}
#endif
#endif
