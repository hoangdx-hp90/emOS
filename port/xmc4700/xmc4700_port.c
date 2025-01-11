/***************************************
* 	OS port for ARM cortex M3_M4 CPU		*
*	Support RealView MDK complier		   *
*	Author: Hoangdx1@viettel.com.vn	   *
*	Date	: September-20-2014		      *
*	Ver	: 1.0.0							   *
****************************************/
#include "../../emOS.h"
#if CPU_TYPE_XMC4700
#include <stdio.h>
#include <stdint.h> 

//#include "stm32f4xx.h"
//#include "misc.h"

#ifndef	config_SYSTEM_CALL_PRIORITY
#define	config_SYSTEM_CALL_PRIORITY	0x07; // XMC4700 using 6 bit for priority.
#endif



uint32_t	CriticalNesting =0;
uint32_t	Pre_BasePri;
 
//======= Setup interrupt base priority register ==============
__asm uint32_t CPU_SET_BASE_PRI(void){
	PRESERVE8
	mrs 	r0, basepri							//Get old base-priority. 
	mov 	r1,#config_SYSTEM_CALL_PRIORITY	
	msr 	basepri,r1								//Set-up new priority
	bx	 	r14										//return
}
//======= Restore interrupt base priority register =============
__asm void CPU_RESTORE_BASE_PRI(uint32_t old_pri){
	PRESERVE8
	msr 	basepri,r0
	bx		r14
}    
//==============================================================
void OS_ENTER_CRITICAL(){
 	if(CriticalNesting ==0)	Pre_BasePri = CPU_SET_BASE_PRI(); 
	CriticalNesting++;
}
//==============================================================
void OS_EXIT_CRITICAL(){
	extern void OS_Critical_Error_Handler();
	if( CriticalNesting ==0) OS_Critical_Error_Handler(0);
	CriticalNesting--; 
	if(CriticalNesting ==0){
		CPU_RESTORE_BASE_PRI(Pre_BasePri); 
	}
}
//==============================================================
//#define	configOS_SYSTIMER_PREQ				168000000      //72000000
void CPU_CONFIG_SYSTEM_TIMER(uint32_t tick_per_sec){
	uint32_t sys_period;
   extern uint32_t SystemCoreClock;
   sys_period = SystemCoreClock/configOS_TICK_PER_SECOND -1;
	*(uint32_t*)0xE000E014	=	sys_period;			//Read Cortex M3 Device,Section 4.4
	*(uint32_t*)0xE000E010   =   0x00000007;			//Clock source: CPU clock,Enable interrupt and force run
}
//=============================================================
void CPU_CONFIG_INTERRUPT(){ 
	*(uint32_t*)0xE000ED08 =   0x08000000; //Offset = 0   
	*(uint32_t*)0xE000ED0C = 0x05FA0000 | 0x300; //using 4bit for Priority.No subgroup
	//Config systemtick and PenSV at lowest priority(15)
	*(uint32_t*)0xe000ed20 = 0xFFFF0000;
}
//==============================================================
void CPU_DISABLE_INTERUPT(){
	__disable_irq();
}
//==============================================================
void CPU_ENABLE_INTERRUPT(){
	__enable_irq();
}
//==============================================================
void CPU_REQUEST_CONTEXT_SWITCH(uint32_t State){
   if(State)	*(uint32_t*)0xE000ED04 |= (1UL <<28); 
} 
//==============================================================
void CPU_CLEAR_TIMER_IRQ(){
}
//==============================================================
void CPU_GET_TIMER_FRACTION(){
}
//==============================================================
__asm void SysTick_Handler(){

	extern	OS_SysTimer_Tick;
	extern 	CurrentTCB;
	extern 	R_TaskList; 
   extern   NeedSwitchContext;
   extern   OS_ContexSwitch_Hook;
    PRESERVE8
// Enter System interrupt level
	mrs	r3,basepri
	mov 	r1,#config_SYSTEM_CALL_PRIORITY	
	msr 	basepri,r1 
   push 	{r14,r3}
//Check if we need context switch immediate	now 
	bl.w	OS_SysTimer_Tick		//Call OS tick event handler.
	pop	{r14,r3}
//R0 = 0 mean no context switch must perform here  
   ldr   r0,=NeedSwitchContext
   ldr   r1,[r0];
   cbnz	r0,save_context	
   
// Restore base priority	and return   
	msr 	basepri,r3 
	bx			r14
	
//=== Perform context switch from system timer interrupt ================
save_context 
	//Get PSP register.	
	mrs		r2, psp
	isb   
	//Test FPU State. and save remaining FPU status register
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)    
//	tst r14, #0x10
//	it eq
	vstmdbeq r2!, {s16-s31} 
#endif
	//Store register not auto-save to Current Task stack
	stmdb r2!, {r4-r11,r14}	//Store remaining register
	ldr	r1, =CurrentTCB 	 
	ldr	r0, [r1] 			//Get location of current PCB
	str 	r2, [r0]				//Save SP pointer to top of tcb 

   push  {r1,r3}
   bl.w  OS_ContexSwitch_Hook
   pop   {r1,r3}
   
restore_context
	ldr	r2, =R_TaskList 	
	ldr	r2,[r2]				//Get location of First TCB in Ready list   
	str	r2,[r1]				//Update current task
	ldr	r0,[r2]				//Get stack pointer from top of tcb 
	ldmia	r0!,{r4-r11,r14}	//Reload register
	
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)  
	//Test FPU State. and restore remaining FPU status register 
//	tst r14, #0x10
//	it eq
	vldmiaeq r0!, {s16-s31} 
#endif
	msr	psp,r0				//Update stack pointer 
	// Exit System interrupt level
	msr 		basepri,r3 
	bx			r14 
	nop
}
//==============================================================
__asm void PendSV_Handler(){
   PRESERVE8
   // PROTECT
	mrs	r0,basepri
	mov 	r1,#config_SYSTEM_CALL_PRIORITY	
	msr 	basepri,r1 
   push 	{r14,r0}
   
   //SAVE CONTEXT
	mrs		r0, psp		 				
	isb    
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)  
//	tst r14, #0x10
//	it eq
	vstmdbeq r0!, {s16-s31} 
#endif
	stmdb r0!, {r4-r11,r14}		//Store remaining register
	ldr	r3, =CurrentTCB 	 
	ldr	r2, [r3] 			   //Get location of current PCB
	str 	r0, [r2]				   //Save SP pointer to top of tcb 
   //LOAD CONTEXT
	ldr	r2, =R_TaskList 	
	ldr	r2,[r2]				   //Get location of First TCB in Ready list 
	str	r2,[r3]				   //Update current task 
	ldr	r0,[r2]				   //Get stack pointer from top of tcb
	ldmia	r0!,{r4-r11,r14}		//Reload register
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
//	tst r14, #0x10
//	it eq
	vldmiaeq r0!, {s16-s31} 
#endif
   //Update stack pointer	
	msr	psp,r0				
	isb
   pop		{r14,r0} 
   
   //UNPROTECT
   msr 		basepri,r0 
	bx			r14
   nop
} 
//==============================================================
__asm void CPU_START_OS(){
	extern 	CurrentTCB;
	
	PRESERVE8 
 #if (__FPU_PRESENT == 1) && (__FPU_USED == 1) 
	ldr.w r0, =0xE000ED88
	ldr	r1, [r0]
	orr	r1, r1, #( 0xf << 20 )
	str r1, [r0]
	isb
#endif
	/* Use the NVIC offset register to locate the stack. */
	ldr r0, =0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]
	/* Set the msp back to the start of the stack. */
	msr msp, r0
	//Tell CPU use PSP register when in thread mode
	mov	r0,#0x00000002
	msr	control,r0
	isb
	//Set-up base-priority
	mov	r0,#0
	msr	basepri,r0

	// Pop data from tcb stack
	ldr	r3, =CurrentTCB
	ldr	r2, [r3]
	ldr	r2, [r2]    
	ldmia	r2!,{r4-r11,r14}	//r4-r11,tempvalue
	ldmia	r2!,{r0}			//Task param
	ldmia r2!,{r4-r7,r14} //pop R1,r2,r3,r12,LR register. using r4-r7 instead of R1,r2,r3,r12 is not bug here because we only use r0 as register hold task parameter
	ldmia	r2!,{r3-r4}		//Pop PSW and return Address .R3 hold the the address of task code we jump to
	msr	psp,r2			//Update stack pointer.
	isb
	// Enable interrupt
	cpsie i
	cpsie f
	dsb
	isb
	//Brand to first task code now 
	bx	r3 
	nop
}  
//==============================================================
void CPU_TASK_INIT(TCB_t * tcb, TaskFunction_t TaskCode ,void *param){
	extern	void OS_ExitLoop_Error_Handler(uint32_t code);
	uint32_t *SP;
	
	SP = tcb->pStack;
   SP-=17;  //FPU
   
	SP--;
	*SP =	0x01000000;							//Program Status register.
	SP--;
	*SP = (uint32_t) TaskCode;   			//PC register
	SP--;
	*SP = (uint32_t)OS_ExitLoop_Error_Handler;	//when task exit -> brand to CM3_TASK_EXIT_LOOP to process error
	SP -= 5;										//R12, R3, R2 and R1 
	*SP = (uint32_t)param;					//R0 register hold argument pass to task when start
	SP-=1;
	*SP = (uint32_t)0xFFFFFFFD;			// 
	SP -= 8;										//R11, R10, R9, R8, R7, R6, R5 and R4,default 
   
	tcb->pStack = SP;							//Store Stack pointer 
}
#endif
