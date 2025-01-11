/*
 * nios2_port.c
 *
 *  Created on: Jul 26, 2018
 *      Author: hoang
 * History:
 *    +2021_12_26: change NeedSwitchContext from uint8_t to uint32_t
 */
#include "../../emOS_Cfg.h"
#if CPU_TYPE_A53

#include <stdint.h>
#include "../../../src/emOS/emOS.h"


/* Xilinx includes. */
#include "xttcps.h"
#include "xscugic.h"
#include "bspconfig.h"

#if configCPU_USE_DDC_STDOUT
#include "xcoresightpsdcc.h"
#endif

#if HYP_GUEST
#error NOT_TEST FOR HYP_GUEST MODE YET
#endif


#if  configCPU_NUM_PRIORITY==2
#define CPU_PRIORIRY_SHIFT			7
#elif  configCPU_NUM_PRIORITY==4
#define CPU_PRIORIRY_SHIFT			6
#elif  configCPU_NUM_PRIORITY==8
#define CPU_PRIORIRY_SHIFT			5
#elif  configCPU_NUM_PRIORITY==16
#define CPU_PRIORIRY_SHIFT			4
#elif  configCPU_NUM_PRIORITY==32
#define CPU_PRIORIRY_SHIFT			3
#elif  configCPU_NUM_PRIORITY==64
#define CPU_PRIORIRY_SHIFT			2
#elif  configCPU_NUM_PRIORITY==128
#define CPU_PRIORIRY_SHIFT			1
#elif  configCPU_NUM_PRIORITY==256
#define CPU_PRIORIRY_SHIFT			0
#else
#error "Invalid CPU INTERRUPT PRIORITY SETTING"
#endif

#define CPU_PRIORITY_REG			(*(uint32_t* volatile )0xF9020004)
//=========================================
size_t volatile		cpu_enter_interrupt =0;
size_t volatile		cpu_critical_nesting =0;
static uint32_t 	timer_period = 1;
size_t				cpu_status_reg=0;
//=========================================
void CPU_CONFIG_SYSTEM_TIMER(void);
void CPU_DISABLE_INTERUPT(void);
void CPU_ENABLE_INTERUPT(void);
void CPU_CLEAR_TIMER_IRQ(void);
void CPU_START_FIRST_TASK(void);
//=========================================
void CPU_CRICIAL_LEVEL_ERROR(void){
	while(1);
}
//=========================================
void OS_ENTER_CRITICAL(){
//	if(cpu_critical_nesting ==0){
//	if(cpu_enter_interrupt ==0) CPU_DISABLE_INTERUPT();
//	}
//	cpu_critical_nesting++;
	//DISABLE INTERRUPT
//
//	CPU_DISABLE_INTERUPT();
	if(CPU_PRIORITY_REG != (configCPU_SYSTEM_PRIORITY << CPU_PRIORIRY_SHIFT)){
		CPU_PRIORITY_REG = (configCPU_SYSTEM_PRIORITY << CPU_PRIORIRY_SHIFT);
		__asm volatile (	"dsb sy		\n"
				"isb sy		\n" ::: "memory" );
	}
	cpu_critical_nesting ++;
//	CPU_ENABLE_INTERUPT();
}
//=========================================
void OS_EXIT_CRITICAL(){
//	if(cpu_critical_nesting) cpu_critical_nesting --;
//	else CPU_CRICIAL_LEVEL_ERROR();
//	if(cpu_critical_nesting ==0 && cpu_enter_interrupt==0){
//		CPU_ENABLE_INTERUPT();
//	}


//	CPU_DISABLE_INTERUPT();
	if(cpu_critical_nesting) cpu_critical_nesting --;
	else CPU_CRICIAL_LEVEL_ERROR();
	if(cpu_critical_nesting ==0){
		CPU_PRIORITY_REG = ((configCPU_NUM_PRIORITY-1)<< CPU_PRIORIRY_SHIFT);
		__asm volatile (	"dsb sy		\n"
				"isb sy		\n" );
	}
//	CPU_ENABLE_INTERUPT();
}
//=========================================
void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State){
	if(State) {
		if(cpu_critical_nesting ==0 && cpu_enter_interrupt ==0){
			// request software interrupt
			__asm volatile ( "SMC 0" ::: "memory" );
		}
		else{
			extern volatile 		uint32_t			NeedSwitchContext;
			NeedSwitchContext =1;
		}
	}
}
//=========================================
uint32_t CPU_GET_TIMER_FRACTION(void){
	return 0;
}
//=================================================
void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param){
	//
	size_t *SP = tcb->pStack;
	SP = (size_t*)((size_t)SP & (~(size_t)(sizeof(size_t)*2-1))); //Align to 128Bit
	//Interruptive register
	SP--;*SP =1;			         //X1
	SP--;*SP = (size_t)param;		 //X0
	SP--;*SP =3;			         //X3
	SP--;*SP =2;			         //X2
	SP--;*SP =5;			         //X5
	SP--;*SP =4;			         //X4
	SP--;*SP =7;			         //X7
	SP--;*SP =6;			         //X6
	SP--;*SP =9;			         //X9
	SP--;*SP =8;			         //X8
	SP--;*SP =11;			         //X11
	SP--;*SP =10;			         //X10
	SP--;*SP =13;			         //X13
	SP--;*SP =12;			         //X12
	SP--;*SP =15;			         //X15
	SP--;*SP =14;			         //X14
	SP--;*SP =17;			         //X17
	SP--;*SP =16;			         //X16
	SP--;*SP =19;			         //X19
	SP--;*SP =18;			         //X18
	SP--;*SP =29;			         //X29
	SP--;*SP =28;			         //X28
	SP--;*SP =0;			         //XZR
	SP--;*SP =30;			         //X30
	//FPU register
	for(uint32_t i=0;i<32;i++){
		SP--;*SP =0;	            //Qx
		SP--;*SP =0;		         //Qx
	}
	SP--;*SP =0;					//FPCR
	SP--;*SP =0;					//FPSR
	//SPSR register
	SP--;*SP =0x0C;	   //PROCESSER_STATE(EL3)
	SP--;*SP =(size_t)TaskCode;//RETURN ADDRESS
	SP--;*SP =21;			//X21
	SP--;*SP =20;			//X20
	SP--;*SP =23;			//X23
	SP--;*SP =22;			//X22
	SP--;*SP =25;			//X25
	SP--;*SP =24;			//X24
	SP--;*SP =27;			//X25
	SP--;*SP =26;			//X24
	tcb->pStack = SP;
}
//=========================================
void CPU_START_OS(void){
	*(uint32_t* volatile)0xFF11000C &= ~1;				//Start timer
	Xil_ExceptionEnable();								//Enable exception
	CPU_START_FIRST_TASK();
}
//=========================================
void CPU_DISABLE_INTERUPT(void){
	__asm volatile ( "MSR DAIFSET, #2" ::: "memory" );				\
	__asm volatile ( "DSB SY" );									\
	__asm volatile ( "ISB SY" );
}
//=========================================
void CPU_ENABLE_INTERUPT(void){
	__asm volatile ( "MSR DAIFCLR, #2" ::: "memory" );				\
	__asm volatile ( "DSB SY" );									\
	__asm volatile ( "ISB SY" );
}
//=========================================
static XScuGic xInterruptController;
void CPU_CONFIG_SYSTEM_TIMER(void){
	//Config timer interval and timer callback
	extern uint32_t OS_SysTimer_Tick();
	//always using TTC0 as system timer
	timer_period = configCPU_TIMER_CLOCK_RATE_HZ/configOS_TICK_PER_SECOND;
	*(uint32_t* volatile)0xFF11000C = 1|(1<<1)|(1<<5); //Interval mode.No waveform out. Stopped
	*(uint32_t* volatile)0xFF110024 = timer_period-1;  //period register
	*(uint32_t* volatile)0xFF110060 = 1;               //Enable interval interrupt
	*(uint32_t* volatile)0xFF110000 = 0;               //Clock mode normal without prescale
	*(uint32_t* volatile)0xFF11000C |= (1<<4);         //Reset counter
	CPU_CLEAR_TIMER_IRQ();
	//Connect timer interrupt handler
	XScuGic_Connect( &xInterruptController, configCPU_TIMER_IRQ_ID, ( Xil_InterruptHandler ) OS_SysTimer_Tick, NULL);
	XScuGic_SetPriorityTriggerType(&xInterruptController,configCPU_TIMER_IRQ_ID,configCPU_SYSTEM_PRIORITY << CPU_PRIORIRY_SHIFT,3);
	//Enable Timer interrupt in GIC
	XScuGic_Enable( &xInterruptController, configCPU_TIMER_IRQ_ID );
}
//=========================================
void CPU_CONFIG_INTERRUPT(void){
	cpu_critical_nesting =0;
	//Init interrupt controller
	XScuGic_Config *pxInterruptControllerConfig  = XScuGic_LookupConfig(0); //assume has only one GIC connected to CPU
	XScuGic_CfgInitialize( &xInterruptController,  pxInterruptControllerConfig, pxInterruptControllerConfig->CpuBaseAddress );
	Xil_ExceptionRegisterHandler( XIL_EXCEPTION_ID_IRQ_INT, ( Xil_ExceptionHandler ) XScuGic_InterruptHandler, &xInterruptController);
	//force all interrupt to lowest. Fault is active high
	for(uint32_t irq=0;irq < XSCUGIC_MAX_NUM_INTR_INPUTS;irq++){
		XScuGic_SetPriorityTriggerType(&xInterruptController,irq,(configCPU_NUM_PRIORITY-1) << CPU_PRIORIRY_SHIFT,3);
	}
}
//=========================================
void CPU_CLEAR_TIMER_IRQ(void){
	volatile uint32_t timer_irq = *(uint32_t* volatile)0xFF110054; //Clear IRQ flags by read interrupt register
	*(uint32_t* volatile)0xFF110054 =timer_irq;
	__asm volatile( "DSB SY" );
	__asm volatile( "ISB SY" );
}
//=========================================
int usleep_t (uint32_t us){
	uint32_t ticks;
	extern int usleep_A53(unsigned long useconds);
	if (OS_GetSchedulerState() == 0)
	{
		usleep_A53(us);
		return 0;
	}
	ticks     = us *configOS_TICK_PER_SECOND/1000000;
	if(ticks){
		OS_TaskSleep(ticks);
	}
	us -= ticks*(1000000/configOS_TICK_PER_SECOND);
	if(us) usleep_A53(us);
	return 0;
}
//=========================================
//void* memset(void* dst, int val, size_t len){
//	void * r = dst;
//	val &= 0xff;
//	while(((size_t)dst & (sizeof(size_t)-1)) && len>0){
//		*(uint8_t*)dst = val;
//		len--;
//		dst = (void*)(((size_t)dst)+1);
//	}
//	size_t tmp=0;
//	for(uint32_t i=0;i<sizeof(size_t);i++) tmp =( tmp<<8) |val;
//	while(len >= sizeof(size_t)){
//		*(size_t*)dst = tmp;
//		dst = (void*)(((size_t)dst)+sizeof(size_t));
//		len -= sizeof(size_t);
//	}
//	while(len>0){
//		*(uint8_t*)dst = val;
//		len--;
//		dst = (void*)(((size_t)dst)+1);
//	}
//	return r;
//}
//=========================================
//==== JTAG UART WRITE DRIVER =============
//=========================================
#if  configCPU_USE_DDC_STDOUT && (configOS_ENABLE_IDLE_HOOK || configOS_ENABLE_TICK_HOOK)
//=========================================
#ifndef config_OS_STDOUT_BUF_SIZE
#define config_OS_STDOUT_BUF_SIZE	(1024*32)
#endif
uint8_t volatile jtag_uart_init_status =0;
uint8_t	jtag_uart_tx_buf[config_OS_STDOUT_BUF_SIZE];
uint32_t volatile jtag_uart_tx_wr_index,jtag_uart_tx_rd_index;

//================================================================================================

static INLINE u32 XCoresightPs_DccGetStatus(void)
{
	u32 Status = 0U;

#ifdef __aarch64__
	asm volatile ("mrs %0, mdccsr_el0" : "=r" (Status));
#elif defined (__GNUC__) || defined (__ICCARM__)
	asm volatile("mrc p14, 0, %0, c0, c1, 0"
			: "=r" (Status) : : "cc");
#else
	{
		volatile register u32 Reg __asm("cp14:0:c0:c1:0");
		Status = Reg;
	}
#endif
	return Status;
}

//================================================================================================
void jtag_uart_init(){
	if(jtag_uart_init_status) return;
	jtag_uart_init_status =1;
	jtag_uart_tx_wr_index =0;
	jtag_uart_tx_rd_index =0;
}
//================================================================================================
void jtag_uart_process(){
	extern void XCoresightPs_DccSendByte(uint32_t BaseAddress, uint8_t Data);
	//=== SEND DATA IN BUFFER ======================================
	if(jtag_uart_tx_wr_index == jtag_uart_tx_rd_index){
		jtag_uart_tx_wr_index = jtag_uart_tx_rd_index =0;
	}
	else{
		if(XCoresightPs_DccGetStatus() & (1 << 29)) return; //TX busy
		XCoresightPs_DccSendByte(0, jtag_uart_tx_buf[jtag_uart_tx_rd_index]);
		jtag_uart_tx_rd_index++;
		while(jtag_uart_tx_rd_index>= config_OS_STDOUT_BUF_SIZE) jtag_uart_tx_rd_index-=config_OS_STDOUT_BUF_SIZE;
	}
}
//================================================================================================
void outbyte(char c)
{
	//=== CHECK LOCK KEY INIT STATUS ========================
	if(jtag_uart_init_status == 0) jtag_uart_init();
	//=== WRITE DATA ====================================
	uint32_t next_wr_index = (jtag_uart_tx_wr_index+1);
	while(next_wr_index >= config_OS_STDOUT_BUF_SIZE)next_wr_index -= config_OS_STDOUT_BUF_SIZE;

	if(next_wr_index!= jtag_uart_tx_rd_index) {
		jtag_uart_tx_buf[jtag_uart_tx_wr_index] =c;
		jtag_uart_tx_wr_index = next_wr_index;
	}
}
#endif
//=========================================
#if configOS_ENABLE_IDLE_HOOK
void CPU_IDLE_HOOK_FCN(void *param){
	//	fflush(stdout);
	jtag_uart_process();
}
#endif
//=========================================
#if configOS_ENABLE_TICK_HOOK
void CPU_TICK_HOOK_FCN(void * param){
	//	fflush(stdout);
	//	jtag_uart_process();
}
#endif
#endif
