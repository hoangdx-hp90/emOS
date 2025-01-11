/*
 * nios2.S
 *
 *  Created on: Jul 27, 2018
 *    Author: hoang
 */
 
#include "../../emOS_Cfg.h"
#if CPU_TYPE_A53

.globl _boot
.globl _vector_table

.globl FIQInterrupt
.globl IRQInterrupt
.globl SErrorInterrupt
.globl SynchronousInterrupt
.globl CPU_START_FIRST_TASK

//=============================================
.macro   save_interruptive_register 
   STP      X0, X1, [SP, #-0x10]!
   STP      X2, X3, [SP, #-0x10]!
   STP      X4, X5, [SP, #-0x10]!
   STP      X6, X7, [SP, #-0x10]!
   STP      X8, X9, [SP, #-0x10]!
   STP      X10, X11, [SP, #-0x10]!
   STP      X12, X13, [SP, #-0x10]!
   STP      X14, X15, [SP, #-0x10]!
   STP      X16, X17, [SP, #-0x10]!
   STP      X18, X19, [SP, #-0x10]!
   STP      X28, X29, [SP, #-0x10]!
   STP      X30, XZR, [SP, #-0x10]!
.endm
.macro   load_interruptive_register
   LDP      X30, XZR, [SP], #0x10
   LDP      X28, X29, [SP], #0x10
   LDP      X18, X19, [SP], #0x10
   LDP      X16, X17, [SP], #0x10
   LDP      X14, X15, [SP], #0x10
   LDP      X12, X13, [SP], #0x10
   LDP      X10, X11, [SP], #0x10
   LDP      X8, X9, [SP], #0x10
   LDP      X6, X7, [SP], #0x10
   LDP      X4, X5, [SP], #0x10
   LDP      X2, X3, [SP], #0x10
   LDP      X0, X1, [SP], #0x10
.endm
//=============================================
.macro   save_FPU_register
   STP      Q0, Q1, [SP,#-0x20]!
   STP      Q2, Q3, [SP,#-0x20]!
   STP      Q4, Q5, [SP,#-0x20]!
   STP      Q6, Q7, [SP,#-0x20]!
   STP      Q8, Q9, [SP,#-0x20]!
   STP      Q10, Q11, [SP,#-0x20]!
   STP      Q12, Q13, [SP,#-0x20]!
   STP      Q14, Q15, [SP,#-0x20]!
   STP      Q16, Q17, [SP,#-0x20]!
   STP      Q18, Q19, [SP,#-0x20]!
   STP      Q20, Q21, [SP,#-0x20]!
   STP      Q22, Q23, [SP,#-0x20]!
   STP      Q24, Q25, [SP,#-0x20]!
   STP      Q26, Q27, [SP,#-0x20]!
   STP      Q28, Q29, [SP,#-0x20]!
   STP      Q30, Q31, [SP,#-0x20]!
   MRS	   x3, FPCR
	MRS	   x2, FPSR
	STP	   x2, x3, [SP, #-0x10]! 
.endm
.macro   load_FPU_register
   LDP	   x2, x3, [SP], #0x10
   MSR	   FPSR,x2
	MSR	   FPCR,x3
   LDP      Q30, Q31, [SP], #0x20
   LDP      Q28, Q29, [SP], #0x20
   LDP      Q26, Q27, [SP], #0x20
   LDP      Q24, Q25, [SP], #0x20
   LDP      Q22, Q23, [SP], #0x20
   LDP      Q20, Q21, [SP], #0x20
   LDP      Q18, Q19, [SP], #0x20
   LDP      Q16, Q17, [SP], #0x20
   LDP      Q14, Q15, [SP], #0x20
   LDP      Q12, Q13, [SP], #0x20
   LDP      Q10, Q11, [SP], #0x20
   LDP      Q8, Q9, [SP], #0x20
   LDP      Q6, Q7, [SP], #0x20
   LDP      Q4, Q5, [SP], #0x20
   LDP      Q2, Q3, [SP], #0x20
   LDP      Q0, Q1, [SP], #0x20
.endm 
//=============================================
.macro  save_SPSR_register  
   MRS      X3, SPSR_EL3
   MRS      X2, ELR_EL3
   STP      X2, X3, [SP, #-0x10]!
.endm
.macro  load_SPSR_register 
   LDP      X2, X3, [SP], #0x10
   MSR      SPSR_EL3,X3
   MSR      ELR_EL3,X2
.endm
//=============================================
.macro   save_callee_register
   STP      X20, X21, [SP, #-0x10]!
   STP      X22, X23, [SP, #-0x10]!
   STP      X24, X25, [SP, #-0x10]!
   STP      X26, X27, [SP, #-0x10]!
.endm 
.macro   load_callee_register
   LDP      X26, X27, [SP], #0x10
   LDP      X24, X25, [SP], #0x10
   LDP      X22, X23, [SP], #0x10
   LDP      X20, X21, [SP], #0x10
.endm  
//== OS VECTOR TABLE ================================
.section .vectors, "a"
.set  OS_VECTOR_OFFSET, 0x1000
.org (OS_VECTOR_OFFSET)
_os_vector_table:
   b  OS_SWI_InterruptHandler
.org (OS_VECTOR_OFFSET +0x80)
   b  OS_IRQ_InterruptHandler
.org (OS_VECTOR_OFFSET +0x100)
   b  OS_FIQ_InterruptHandler
.org (OS_VECTOR_OFFSET +0x180)
   b  OS_ERR_InterruptHandler
 
//===============================================
OS_SWI_InterruptHandler:
   //Store context
   MSR   SPSEL, #0
   save_interruptive_register
   save_FPU_register
   save_SPSR_register
   save_callee_register
//Clear NeedSwitchContext
   LDR    X0,=NeedSwitchContext
   STR   XZR,[X0]
//Store Stack pointer
   LDR   X0,=CurrentTCB
   LDR   X1,[X0]
   MOV   X2,SP
   STR   X2,[X1]
//Load new task
   LDR   X3,=R_TaskList
   LDR   X3,[X3]
   LDR   X4,[X3]  //get SP value
   MOV   SP,X4    //Restore stack pointer
   STR   X3,[X0]  //Change current TCB
//Restore context
   load_callee_register
   load_SPSR_register
   load_FPU_register
   load_interruptive_register
   MSR   SPSEL, #1
   eret
//===============================================
OS_IRQ_InterruptHandler:
   //Store context
   MSR 	SPSEL, #0
   save_interruptive_register
   save_FPU_register
   save_SPSR_register
   save_callee_register
   //tell os we enter interrupt
   LDR    X0,= cpu_enter_interrupt
   MOV   X1,#1
   STR   X1,[X0]
   //Run interrupt hander
   bl	   IRQInterrupt
   //tell os we exit interrupt
   LDR    X0,= cpu_enter_interrupt
   STR   XZR,[X0]

   //Check if need swith context now
   LDR    X0,=NeedSwitchContext
   LDR   X1,[X0]
   STR   XZR,[X0] //Clear NeedSwitchContext
   CMP   X1,XZR
   B.EQ  RESTORE_TASK
//Store Stack pointer
   LDR   X0,=CurrentTCB
   LDR   X1,[X0]
   MOV   X2,SP
   STR   X2,[X1]
//Load new task
   LDR   X3,=R_TaskList
   LDR   X3,[X3]
   LDR   X4,[X3]  //get SP value
   MOV   SP,X4    //Restore stack pointer
   STR   X3,[X0]  //Change current TCB
RESTORE_TASK:
   load_callee_register
   load_SPSR_register
   load_FPU_register
   load_interruptive_register
   MSR   SPSEL, #1
   eret
//===============================================
OS_FIQ_InterruptHandler:
   MSR 	SPSEL, #0   //Set process register
   save_interruptive_register
   save_FPU_register
   save_SPSR_register
   save_callee_register
   
   bl	   FIQInterrupt
    
   load_callee_register
   load_SPSR_register
   load_FPU_register
   load_interruptive_register
   MSR   SPSEL, #1
   eret
//===============================================
OS_ERR_InterruptHandler:
   MSR 	SPSEL, #0   //Set process register
   save_interruptive_register
   save_FPU_register
   save_SPSR_register
   save_callee_register
   
   bl	   SErrorInterrupt
   
   load_callee_register
   load_SPSR_register
   load_FPU_register
   load_interruptive_register
   MSR   SPSEL, #1
   eret 
 //===============================================
 CPU_START_FIRST_TASK:
   MSR   SPSEL, #0
   LDR      X1, =_os_vector_table
   MSR      VBAR_EL3, X1
   DSB      SY
   ISB      SY

   LDR   X1,=R_TaskList
   LDR   X1,[X1]
   LDR   X2,[X1]  //get SP value
   MOV   SP,X2    //Restore stack pointer

   LDR   X0,=CurrentTCB
   STR   X1,[X0]  //Change current TCB

   load_callee_register
   load_SPSR_register

   load_FPU_register
   load_interruptive_register
   eret
#endif