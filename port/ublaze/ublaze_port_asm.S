/*
 * nios2.S
 *
 *  Created on: Jul 27, 2018
 *    Author: hoang
 */
 #include "../../emOS_Cfg.h"

#if  CPU_TYPE_UBLAZE
#include "xparameters.h"
#include "ublaze_macro.h"

//==== external variable
.extern XIntc_DeviceInterruptHandler
.extern emOS_INTERRUPT_info
.extern NeedSwitchContext
.extern cpu_enter_interrupt
//==== global function ==============================
.global _interrupt_handler
.global _exception_handler
.global  emOS_Load_New_Task
.global software_context_switch
//==== SOME MACRO ====================================
.macro os_save_register_level1
#if XPAR_MICROBLAZE_USE_FPU
   addik  r1, r1, -76
#else
   addik  r1, r1, -72
#endif
   swi   r18,  r1 ,  0
   swi   r17,  r1 ,  4
   swi   r16,  r1 ,  8
   swi   r15,  r1 ,  12
   swi   r14,  r1 ,  16
   swi   r13,  r1 ,  20
   swi   r12,  r1 ,  24
   swi   r11,  r1 ,  28
   swi   r10,  r1 ,  32
   swi   r9,   r1 ,  36
   swi   r8,   r1 ,  40
   swi   r7,   r1 ,  44
   swi   r6,   r1 ,  48
   swi   r5,   r1 ,  52
   swi   r4,   r1 ,  56
   swi   r3,   r1 ,  60
   swi   r2,   r1 ,  64
   mfs   r18, rmsr
   swi   r18,   r1 , 68
#if XPAR_MICROBLAZE_USE_FPU
   mfs   r17, rfsr
   swi   r17, r1,   72
#endif
.endm
//=====================================================
.macro os_save_register_level2
   addik  r1, r1, -52
   swi   r31,  r1 ,  0
   swi   r30,  r1 ,  4
   swi   r29,  r1 ,  8
   swi   r28,  r1 ,  12
   swi   r27,  r1 ,  16
   swi   r26,  r1 ,  20
   swi   r25,  r1 ,  24
   swi   r24,  r1 ,  28
   swi   r23,  r1 ,  32
   swi   r22,  r1 ,  36
   swi   r21,  r1 ,  40
   swi   r20,  r1 ,  44
   swi   r19,  r1 ,  48
.endm

//=====================================================
.macro os_load_register_level2
   lwi   r31,  r1 ,  0
   lwi   r30,  r1 ,  4
   lwi   r29,  r1 ,  8
   lwi   r28,  r1 ,  12
   lwi   r27,  r1 ,  16
   lwi   r26,  r1 ,  20
   lwi   r25,  r1 ,  24
   lwi   r24,  r1 ,  28
   lwi   r23,  r1 ,  32
   lwi   r22,  r1 ,  36
   lwi   r21,  r1 ,  40
   lwi   r20,  r1 ,  44
   lwi   r19,  r1 ,  48
   addik  r1,  r1 ,   52
.endm
//======================================================
.macro os_load_register_level1
#if XPAR_MICROBLAZE_USE_FPU
   lwi   r17, r1, 72
   mts   rfsr, r17
#endif
   lwi   r18, r1, 68
   mts   rmsr, r18
   lwi   r18,  r1 ,  0
   lwi   r17,  r1 ,  4
   lwi   r16,  r1 ,  8
   lwi   r15,  r1 ,  12
   lwi   r14,  r1 ,  16
   lwi   r13,  r1 ,  20
   lwi   r12,  r1 ,  24
   lwi   r11,  r1 ,  28
   lwi   r10,  r1 ,  32
   lwi   r9,   r1 ,  36
   lwi   r8,   r1 ,  40
   lwi   r7,   r1 ,  44
   lwi   r6,   r1 ,  48
   lwi   r5,   r1 ,  52
   lwi   r4,   r1 ,  56
   lwi   r3,   r1 ,  60
   lwi   r2,   r1 ,  64
#if XPAR_MICROBLAZE_USE_FPU
   addik  r1, r1, 76
#else
   addik  r1, r1, 72
#endif
.endm
//==== NORMAL INTERRUPT HANDLER =============================
   .text
   .align  4
_interrupt_handler:
save_context:
   //emOS_context_save
   os_save_register_level1
   addi		r5,r0,1
   swi		r5,r0,cpu_enter_interrupt				//tell C function we're enter IRQ
   ori      r5,r0, UBLAZE_GET_ID(SYS_INTERRUPT_NAME)
   bralid   r15, XIntc_DeviceInterruptHandler      //Call interrupt C handler
   or       r0,r0,r0
   swi		r0,r0,cpu_enter_interrupt				//tell C function we're exit  IRQ
   lwi      r18, r0, NeedSwitchContext             //Check if need perform conext switch now
   bneid    r18, exe_context_switch
   or       r0,r0,r0
restore_context:
   os_load_register_level1
   rtid     r14, 0
   or       r0,r0,r0
exe_context_switch:
   swi      r0, r0, NeedSwitchContext             //Clear  context request for next time
store_stack_pointer_to_current_tcb:
   os_save_register_level2
   lwi      r3, r0, CurrentTCB                    //Store Stack pointer
   sw       r1, r0, r3

 	bralid   r15, OS_SaveTaskCallback
    or       r0,r0,r0

emOS_Load_New_Task:
change_task_to_run:
   lwi      r4,r0, NextTCB
   swi      r4,r0, CurrentTCB
   bralid   r15, OS_LoadTaskCallback
   or       r0,r0,r0
   lwi      r4,r0, CurrentTCB
   lw       r1,r0,r4                               //Get stack pointer
   //emOS_context_restore
   os_load_register_level2
   os_load_register_level1
   rtid     r14, 0
   or       r0,r0,r0

   .text
   .align  4
_exception_handler:
   addi     r14, r14 ,4
   os_save_register_level1
   os_save_register_level2
   lwi      r3, r0, CurrentTCB                     //Store Stack pointer
   sw       r1, r0, r3
   bralid   r15, OS_SaveTaskCallback
   or       r0,r0,r0
   lwi      r4,r0, NextTCB                      	//Update current TCP
   swi      r4,r0, CurrentTCB
   bralid   r15, OS_LoadTaskCallback
   or       r0,r0,r0
   lwi      r4,r0, CurrentTCB
   lw       r1,r0,r4                               //Get stack pointer
 //  emOS_context_restore
   os_load_register_level2
   os_load_register_level1
   rtid     r14, 0
   or      	r0,r0,r0

#endif
