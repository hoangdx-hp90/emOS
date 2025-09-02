/************************************************
 *	RTOS for Cortext M3-M4 CPU 					*
 *	Module : CM3-PORT					     	*
 *	Author : Hoangdx1@viettel.com.vn	      	*
 *	Version:	1.2      			 			*
 *************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//		V1.1  | Hoangdx     |Apr,25,2021	  | Fix Change OS_ENTER_CRITICAL and OS_EXIT_CRITICAL from void to uint32_t
//		V1.1  | Hoangdx     |Apr,25,2021	  | Remove TIMER_FRACTION FUNCTION
//		V1.1  | Hoangdx     |Apr,25,2021	  | Add priority convert to 4bit for STM32 CM3
//		V1.2  | Hoangdx     |Feb,06,2022	  | Add support for cortex M7
// ============================================================================

#include <stdio.h>
#include <stdint.h>
#include "../../emOS.h"

#if CPU_TYPE_STM32_CM7 || CPU_TYPE_STM32_CM3 || CPU_TYPE_STM32_CM4

#if CPU_TYPE_STM32_CM7
#include <stm32h7xx.h>
#endif

#if CPU_TYPE_STM32_CM3
#include "stm32f10x.h"
#include "misc.h"
#endif

#if CPU_TYPE_STM32_CM4
#include "stm32f4xx.h"
#include "misc.h"
#endif

uint32_t volatile 	cpu_critical_nesting =0;
uint32_t volatile 	Pre_BasePri;
uint32_t 			timer_period=0;
uint32_t 			internal_tick=1;
void  CPU_INVALID_PRIORITY_HANDLER();
//=====================================================================
uint32_t   __attribute__((naked)) CPU_SET_BASE_PRI(){
	__asm volatile
	(
			".align 2                    	\n\t"//
			".thumb_func                	\n\t"//
			"mov    r1,%0                 	\n\t"//
			"mrs  	r0, basepri          	\n\t"// Get old base-priority.
			"msr  	basepri,r1            	\n\t"// Set-up new priority
			"bx 	lr                   	\n\t"// return
			::"i" ( configOS_SYSTEM_CALL_PRIORITY)
	);
}
//=====================================================================
void   __attribute__((naked)) CPU_RESTORE_BASE_PRI(uint32_t old_pri){
	__asm volatile
	(
			".align 2                    	\n\t"//
			".thumb_func                	\n\t"//
			"msr     	basepri,r0			\n\t"//
			"bx        lr					\n\t"//
	);
}
//=====================================================================
void   __attribute__((naked)) CPU_START_OS(){
	__asm volatile
	(
			".align 2                    	\n\t"//
			".thumb_func                	\n\t"//
			//====Use the NVIC offset register to locate the stack====
			"ldr r0, =0xE000ED08         	\n\t"//
			"ldr r0, [r0]                 	\n\t"//
			"ldr r0, [r0]                 	\n\t"//
			//=== Set the msp back to the start of the stack==========
			"msr msp, r0                 	\n\t"//
			//=== Tell CPU use PSP register when in thread mode ======
#if (__FPU_PRESENT) && (__FPU_USED)
			"mov    r0,#0x00000006         	\n\t"//Also set FPU context
#else
			"mov    r0,#0x00000002         	\n\t"//
#endif
			"msr    control,r0             	\n\t"//
			"isb                         	\n\t"//
			"dsb                         	\n\t"//
			//=== Set-up base-priority ==============================
			"mov    r0,#0                 	\n\t"//
			"msr    basepri,r0             	\n\t"//
			//=== Pop data from tcb stack ===========================
			"ldr  	r2, =NextTCB     	    \n\t"//
			"ldr  	r1,[r2]             	\n\t"//	Get location of First TCB in Ready list
			"ldr	r0, =CurrentTCB      	\n\t"//	Get location of current tcb
			"str 	r1,[r0]            		\n\t"//	Update current task
			"ldr   	r1,[r1]            		\n\t"//	Get stack pointer from top of tcb
			"mov   	sp,r1            		\n\t"// Update stack pointer
			"isb                        	\n\t"//
			"dsb                         	\n\t"//
			"ldmia 	sp!,{r4-r11}        	\n\t"//Reload register R4-R11
#if (__FPU_PRESENT) && (__FPU_USED)
			"vldmia	sp!, {s16-s31}        	\n\t"//Reload USER STORE FPU register
#endif
			"ldmia 	sp!,{r0,r1,r2,r3,r12,lr}\n\t"//Reload R0-R3,R12,R14(LR)
			"ldmia 	sp!,{r1,r2}            	\n\t"//Reload Estatus, PC register.
			"msr   	EPSR,r2            		\n\t"//
#if (__FPU_PRESENT) &&  (__FPU_USED)
			"vldmia	sp!, {s0-s15}           \n\t"//Reload AUTO STORE FPU register
			"ldmia 	sp!, {r2}          		\n\t"//
			"vmsr   fpscr,r2 				\n\t"//
#endif
			"cpsie i                     	\n\t"//
			"cpsie f                     	\n\t"//
			"dsb                         	\n\t"//
			"isb                        	\n\t"//
			"bx  r1                         \n\t"//
	);
}
//================================================================
//  switch_context_macro
//================================================================
__attribute__((always_inline)) static inline  void  switch_context_macro (){
	__asm volatile
	(
			"mov  	r1,%0  					\n\t"//
			"msr 	basepri,r1  			\n\t"//
			"mrs  	r2, psp  				\n\t"//
			"dsb                         	\n\t"//
			"isb  							\n\t"//
#if (__FPU_PRESENT) && (__FPU_USED)
			"vstmdb	r2!, {s16-s31}        	\n\t"//Store USER STORE FPU register
#endif
			"stmdb	r2!,{r4-r11}  			\n\t"//
			"ldr  	r1, =CurrentTCB  		\n\t"//
			"ldr  	r0, [r1]  				\n\t"//Get location of current PCB
			"str   	r2, [r0]   				\n\t"//Save SP pointer to top of tcb
			"bl.w  	OS_SaveTaskCallback  	\n\t"//
			//== LOAD NEXT TASK =======================================
			"ldr    r2,=NextTCB  		    \n\t"//
			//=== LOAD REGISTER =========================================
			"ldr    r1,[r2]               	\n\t"//Get location of TCB
			"ldr    r0, =CurrentTCB       	\n\t"//Get location of current tcb
			"ldr    r2,[r1]              	\n\t"//Get stack pointer from top of tcb
			"str    r1,[r0]               	\n\t"//Update current task
			"ldmia  r2!,{r4-r11}          	\n\t"//Reload register
#if (__FPU_PRESENT) && (__FPU_USED)
			"vldmia r2!, {s16-s31}        	\n\t"//Reload USER STORE FPU register
#endif
			"msr    psp,r2                	\n\t"//Update stack pointer
			"isb  							\n\t"//
			"bl.w   OS_LoadTaskCallback  	\n\t"//
#if (__FPU_PRESENT) &&  (__FPU_USED)
			"mov   lr,#0xFFFFFFED			\n\t"//
#else
			"mov   lr,#0xFFFFFFFD			\n\t"//
#endif
			//=== EXIT PROTECTION SECTION ==============================
			"mov	r0,#0 					\n\t"//
			"msr    basepri,r0  			\n\t"//
			"isb                         	\n\t"//
			"dsb                         	\n\t"//
			"bx   	lr  					\n\t"//
			"nop 							\n\t"//
			"nop 							\n\t"//
			::"i" ( configOS_SYSTEM_CALL_PRIORITY)
	);
}
//=====================================================================
void   __attribute__((naked,section(".itcmram"))) PendSV_Handler (){
	switch_context_macro();
}
//=====================================================================
void   __attribute__((naked,section(".itcmram"))) SysTick_Handler() {
	// Enter System interrupt level
	__asm volatile
	(
			//=== ENTER PROTECTION SECTION ==============================
			"mov  	r1,%0  					\n\t"//
			"msr 	basepri,r1  			\n\t"//
			//==== INCREASE TICK =======================================
			"ldr   	r1,=internal_tick	    \n\t"
			"ldr   	r2,[r1] 				\n\t"//
			"add    r2,r2,1  				\n\t"//
			"str  	r2,[r1]					\n\t"//
			//=== Call OS tick event handler ===========================
			"bl.w  	OS_SysTimer_Tick  		\n\t"//
			//=== Check if need perform switch context or not ==========
			"ldr   	r1,=NeedSwitchContext	\n\t"//
			"ldr   	r2,[r1] 				\n\t"//
			"mov  	r0,#0 					\n\t"//
			"cmp	r2,r0 					\n\t"//
			"ITTT  	EQ						\n\t"//
			//=== IF EQUAL 0 (Mean no context switch => clear base_priority and return
#if (__FPU_PRESENT) &&  (__FPU_USED)
			"moveq   lr,#0xFFFFFFED			\n\t"//
#else
			"moveq   lr,#0xFFFFFFFD			\n\t"//
#endif
			"msreq  basepri,r0				\n\t"//
			"bxeq	lr						\n\t"//
			"str  	r0,[r1]					\n\t"//Clear Request
			::"i" ( configOS_SYSTEM_CALL_PRIORITY)
	);
	switch_context_macro();
}
//==============================================================
int32_t  __attribute__((section(".itcmram"))) OS_ENTER_CRITICAL(){
	if(cpu_critical_nesting ==0)    {
		Pre_BasePri = CPU_SET_BASE_PRI();
		if(Pre_BasePri != 0) CPU_INVALID_PRIORITY_HANDLER();
	}
	cpu_critical_nesting++;
	return (Pre_BasePri == 0);
}
//==============================================================
int32_t __attribute__((section(".itcmram"))) OS_EXIT_CRITICAL(){
	extern void OS_Critical_Error_Handler();
	if( cpu_critical_nesting ==0) OS_Critical_Error_Handler(0);
	cpu_critical_nesting--;
	if(cpu_critical_nesting ==0){
		if(Pre_BasePri != 0) CPU_INVALID_PRIORITY_HANDLER();
		CPU_RESTORE_BASE_PRI(Pre_BasePri);
	}
	return (Pre_BasePri == 0);
}
//==============================================================
void CPU_INVALID_PRIORITY_HANDLER(){
	//    while(1);
}
//=============================================================
void CPU_CONFIG_SYSTEM_TIMER(uint32_t tick_per_sec){
	timer_period = SystemCoreClock/configOS_TICK_PER_SECOND;
	*(uint32_t*)0xE000E014    =    timer_period-1;    	//Read Cortex M3 Device,Section 4.4
	*(uint32_t*)0xE000E010   =   0x00000007;            //Clock source: CPU clock,Enable interrupt and force run
}
//=============================================================
uint32_t __attribute__((section(".itcmram"))) CPU_GET_TIMER_PERIOD(void){
	return timer_period;
}
//=============================================================
uint64_t __attribute__((section(".itcmram"))) CPU_GET_TIMER_VAL(void){
	return (uint64_t )internal_tick*timer_period - *(uint32_t*)0xE000E018;
}
//=============================================================
void CPU_CONFIG_INTERRUPT(){
	//Change interrupt priority setting
	uint32_t priogroup = ((SCB->AIRCR) & (uint32_t)0x700)>> 0x08;
	uint32_t prio_shift = priogroup+1;
	uint32_t prio_mask = (0xff << prio_shift)&0xff;
	uint32_t sub_mask = (1<<prio_shift)-1;
	uint32_t irq_channel;
	for(irq_channel =0; irq_channel <240;irq_channel++){
		uint8_t channel_irq = NVIC->IP[irq_channel];
		uint8_t sub_prio = channel_irq& sub_mask;
		uint8_t main_prio = (channel_irq&prio_mask) >> prio_shift;
		//convert to 4,4 format
		if(sub_prio <16 && main_prio < 16){
			NVIC->IP[irq_channel] = ((main_prio<<4)&0xf0) | (sub_prio&0x0f);
		}
		else{
			OS_PRINTF("\r\n Invalid IRQ priority at channel %u",(unsigned int)irq_channel);
		}
	}
	//    *(uint32_t*)0xE000ED08 =   0x08000000; //Offset = 0
	*(uint32_t*)0xE000ED0C = 0x05FA0300 ; //using 4bit for Priority.
	//Configure system tick and PenSV at lowest priority(15)
	*(uint32_t*)0xe000ed20 = 0xFFFF0000;
}
//==============================================================
void __attribute__((section(".itcmram"))) CPU_DISABLE_INTERUPT(){
	__disable_irq();
}
//==============================================================
void __attribute__((section(".itcmram"))) CPU_ENABLE_INTERRUPT(){
	__enable_irq();
}
//==============================================================
void __attribute__((section(".itcmram"))) CPU_REQUEST_CONTEXT_SWITCH(uint32_t State){
	if(State){
		*(uint32_t*)0xE000ED04 |= (1UL <<28);
		__asm volatile ( "dsb" ::: "memory" );
		__asm volatile ( "isb" );
	}
}
//==============================================================
void CPU_CLEAR_TIMER_IRQ(){
}
//==============================================================
void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param){
	extern    void OS_ExitLoop_Error_Handler(uint32_t code);
	uint32_t *SP;
	SP = tcb->pStack;
	//===AUTO SAVE REGISTER ================================
	SP = (uint32_t*)((uint32_t)SP &0xfffffff8);			//Force align stack to 8byte boundary
#if (__FPU_PRESENT) && (__FPU_USED)
	SP--; *SP =0;										//Reserved
	SP--; *SP =__get_FPSCR();                            //FPSCR
	SP--; *SP =15;                                        //S15
	SP--; *SP =14;                                        //S14
	SP--; *SP =13;                                        //S13
	SP--; *SP =12;                                        //S12
	SP--; *SP =11;                                        //S11
	SP--; *SP =10;                                        //S10
	SP--; *SP =9;                                        //S9
	SP--; *SP =8;                                        //S8
	SP--; *SP =7;                                        //S7
	SP--; *SP =6;                                        //S6
	SP--; *SP =5;                                        //S5
	SP--; *SP =4;                                        //S4
	SP--; *SP =3;                                        //S3
	SP--; *SP =2;                                        //S2
	SP--; *SP =1;                                        //S1
	SP--; *SP =0;                                        //S0
#endif
	SP--; *SP = 0x01000000;                         	//Program Status register. Thumb MODE
	SP--; *SP = (uint32_t) TaskCode |1;                 //PC register
	SP--; *SP = (uint32_t)OS_ExitLoop_Error_Handler|1 ;	//when task exit -> brand to CM3_TASK_EXIT_LOOP to process error
	SP--; *SP = 12;                                  	//R12
	SP--; *SP = 3;                                   	//R3
	SP--; *SP = 2;                                     	//R2
	SP--; *SP = 1;                                    	//R1
	SP--;*SP = (uint32_t)param;                         //R0 register hold argument pass to task when start
	//=== USER SAVE REGISTER =================================
#if (__FPU_PRESENT) && (__FPU_USED)
	SP--; *SP =31;                                        //S31
	SP--; *SP =30;                                        //S30
	SP--; *SP =29;                                        //S29
	SP--; *SP =28;                                        //S28
	SP--; *SP =27;                                        //S27
	SP--; *SP =26;                                        //S26
	SP--; *SP =25;                                        //S25
	SP--; *SP =24;                                        //S24
	SP--; *SP =23;                                        //S23
	SP--; *SP =22;                                        //S22
	SP--; *SP =21;                                        //S21
	SP--; *SP =20;                                        //S20
	SP--; *SP =19;                                        //S19
	SP--; *SP =18;                                        //S18
	SP--; *SP =17;                                        //S17
	SP--; *SP =16;                                        //S16
#endif
	SP--; *SP = 11;                                        //R11
	SP--; *SP = 10;                                        //R10
	SP--; *SP = 9;                                        //R9
	SP--; *SP = 8;                                        //R8
	SP--; *SP = 7;                                        //R7
	SP--; *SP = 6;                                        //R6
	SP--; *SP = 5;                                        //R5
	SP--; *SP = 4;                                        //R4
	tcb->pStack = SP;                                    //Store Stack pointer
}
//================================================================================================
void swo_init(uint32_t base ){}
//================================================================================================
uint32_t swo_write_char(uint32_t base, uint8_t ch)
{
	ITM->PORT[0].u8 = (uint8_t) ch;
	return 1;
}
//================================================================================================
uint32_t swo_is_ready(uint32_t base){
	if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)  &&      /* Trace enabled */
			(ITM->TCR & ITM_TCR_ITMENA_Msk)                  &&      /* ITM enabled */
			(ITM->TER & (1ul << 0)        )                    )     /* ITM Port #0 enabled */
	{
		if (ITM->PORT[0].u32 == 0) return 0;
		return 1;
	}
	return 0;
}
//=========================================
extern void OS_register_terminal_driver(uint32_t base,void (*init)(uint32_t),uint32_t (*is_tx_ready)(uint32_t),uint32_t (*send_byte)(uint32_t , uint8_t ),void (*task_code)(void*), uint32_t tx_buf_size);

#ifdef STM32H743xx
static uint8_t ram_d1[1024*512]  __attribute__((aligned(4), section(".ram_d1")));
static uint8_t ram_d2[1024*256]  __attribute__((aligned(4), section(".ram_d2")));
#endif
void CPU_PERFORM_OTHER_CONFIG(){
	static uint8_t init_status =0;
	if(init_status) return;
	init_status =1;
#ifdef STM32H743xx
	OS_HeapAddMemAtTheEnd(ram_d1,sizeof(ram_d1));
	OS_HeapAddMemAtTheEnd(ram_d2,sizeof(ram_d2));
#endif
#ifndef configOS_STDOUT_BUF_SIZE
	OS_register_terminal_driver(1,swo_init,swo_is_ready,swo_write_char,NULL,2048);
#else
	OS_register_terminal_driver(1,swo_init,swo_is_ready,swo_write_char,NULL,configOS_STDOUT_BUF_SIZE);
#endif
}
//=========================================



#endif

