/************************************************
 *   RTOS for ARM M3-M4 CPU                     *
 *   Module :  Task Scheduler                *
 *   Author : Hoangdx1@viettel.com.vn           *
 *   Version:   1.1                             *
 ************************************************/
// ============================================================================
// Revision History:
// ============================================================================
//      Ver.: |Author:      |Mod. Date:       |Changes Made:
//    V1.1  | Hoangdx     |Jan,04,2021   | Fix Cpp check warning
//    V1.1  | Hoangdx     |Apr,25,2021   | Fix Change OS_ENTER_CRITICAL and OS_EXIT_CRITICAL from void to uint32_t
//    V1.1  | Hoangdx       |Dec,10,2021      | cpu_critical_nesting change size to 4. Please check all CPU porting
//    V1.1  | Hoangdx       |Dec,10,2021      | Add checking parameter configOS_ENABLE_IDLE_CALLBACK before call CPU_TICK_CALLBACK
//    V1.1  | Hoangdx     |Dec,26,2021      | Change check heap remaining from each time system task run to configOS_RUNTIME_UPDATE_TICK
//    V1.1  | Hoangdx     |Dec,26,2021      | Printf out when alloc memory for new task not successful
//    V1.1  | Hoangdx     |Jan,02,2022      | Add task stack checking in system task
//    V1.1  | Hoangdx     |Jan,19,2022      | Change OS_TaskSleepUntil function behave
//    V1.1  | Hoangdx     |Jan,19,2022      | Change OS_SystemTask wakeup method. using
//    V1.1  | Hoangdx     |Jan,19,2022      | Add  NextIsSystemTask flag to select SystemTaskHandle or R_Tasklist
//    V1.1  | Hoangdx     |Jan,19,2022      | Fix interrupt issue when OS not started yet. When SchedulerState =0 => Temporarity Disable interupp
// ============================================================================
#include "../../emOS_Cfg.h"
#if CPU_TYPE_NIOS2
#include "system.h"

   .set noat
//=== INTERRUPT START =================================
   .globl alt_exception
   .type alt_exception, @function
   .globl alt_irq_entry
   .type alt_irq_entry, @function
   .globl  nios2_emOS_Start_OS
   .type nios2_emOS_Start_OS, @function

   .globl   software_context_switch
   .type    software_context_switch, @function

//=============================================
.macro   save_register_to_stack
   addi  sp, sp, -116
   stw   ra,  0(sp)
   stw   r1,   8(sp)
   stw   r2,  12(sp)
   stw   r3,  16(sp)
   stw   r4,  20(sp)
   stw   r5,  24(sp)
   stw   r6,  28(sp)
   rdctl r5,   estatus
   addi  r6,   ea,-4
   stw   r7,  32(sp)
   stw   r8,  36(sp)
   stw   r9,  40(sp)
   stw   r10, 44(sp)
   stw   r11, 48(sp)
   stw   r12, 52(sp)
   stw   r13, 56(sp)
   stw   r14, 60(sp)
   stw   r15, 64(sp)
   stw   r5,  68(sp)
   stw   r6,  72(sp)
   stw   r16, 76(sp)
   stw   r17, 80(sp)
   stw   r18, 84(sp)
   stw   r19, 88(sp)
   stw   r20, 92(sp)
   stw   r21, 96(sp)
   stw   r22, 100(sp)
   stw   r23, 104(sp)
   stw   gp, 108(sp)
   stw   fp, 112(sp)

.endm
//=============================================
.macro   load_register_from_stack
   ldw   r5,  68(sp)
   wrctl estatus, r5
   ldw   ra,   0(sp)
   ldw   r1,   8(sp)
   ldw   r2,  12(sp)
   ldw   r3,  16(sp)
   ldw   r4,  20(sp)
   ldw   r5,  24(sp)
   ldw   r6,  28(sp)
   ldw   r7,  32(sp)
   ldw   r8,  36(sp)
   ldw   r9,  40(sp)
   ldw   r10, 44(sp)
   ldw   r11, 48(sp)
   ldw   r12, 52(sp)
   ldw   r13, 56(sp)
   ldw   r14, 60(sp)
   ldw   r15, 64(sp)
   ldw   ea,  72(sp)
   ldw   r16, 76(sp)
   ldw   r17, 80(sp)
   ldw   r18, 84(sp)
   ldw   r19, 88(sp)
   ldw   r20, 92(sp)
   ldw   r21, 96(sp)
   ldw   r22, 100(sp)
   ldw   r23, 104(sp)
   ldw   gp, 108(sp)
   ldw   fp, 112(sp)
   addi  sp, sp, 116
.endm
//=============================================
.macro store_info_and_load_new_task
   movia    r3, CurrentTCB
   ldw      r4, (r3)
   stw      sp, (r4)
#if configOS_ENABLE_CONTEXT_SWITCH_HOOK
   call    OS_SaveTaskCallback
#endif
   //--- load new task ------------------------
   movia    r5, NextTCB                   	//Load R_Tasklist instead of system_Task
1: ldw      r6, (r5)
   movia    r3, CurrentTCB                   //update CurrentTCB
   stw      r6, (r3)                         //
   ldw      sp, (r6)                         //Get stack pointer
#if configOS_ENABLE_CONTEXT_SWITCH_HOOK
   call    OS_LoadTaskCallback
#endif
   movia    r1, OS_INTERRUPT_CONTEXT         // TELL OS WE EXIT  INTERRUPT PROCESS
   stw      zero,(r1)
   load_register_from_stack
   eret
.endm
//=============================================
// INTERRUPT HANDLER
//=============================================
   .section .exceptions.entry, "xa"
alt_exception:
   save_register_to_stack
   movia    r3, SchedulerState
   ldw      r3,(r3)
   bne      r3,zero,emOS_Ready
emOS_Not_Ready:
   call     alt_irq_handler
   load_register_from_stack
   wrctl    estatus, zero              //Disable interrupt
   eret
emOS_Ready:
   movia    r1, OS_INTERRUPT_CONTEXT    // TELL OS WE ENTER INTERRUPT PROCESS
   ori      r2,zero,1
   stw      r2,(r1)
test_Trap:
   addi  r6,ea,-4                   //Load address  when irq occured
   ldw   r7,(r6)                    //Load instruction code
#ifdef ALT_CPU_CPU_ARCH_NIOS2_R2
   movhi r3,0xb41d                  /* upper half of trap opcode */
   ori   r3,r3,0x0020               /* lower half of trap opcode */
#else
   movhi r3,0x003b                  /* upper half of trap opcode */
   ori   r3,r3,0x683a               /* lower half of trap opcode */
#endif
   bne   r7,r3,test_hardware_irq    //Not trap instruction => Test hardware IRQ
trap_IRQ_Handler:
   stw   ea,72(sp)                  //Abort current instruction(TRAP) which cause IRQ
   store_info_and_load_new_task     //Store info and reaload new task
   eret
//=== INTERUPT TEST =================================
test_hardware_irq:
   rdctl    r5, estatus
   rdctl    r4, ipending
   andi     r6, r5, 1
   beq      r4, zero, emOS_unknown_exception
   beq      r6, zero, emOS_unknown_exception
normal_hardware_interrupt:
   call     alt_irq_handler
emOS_CheckContext_Status:
   movia    r5,NeedSwitchContext    //Check if we need perform context switch now
   ldw      r6,(r5)
   stw      zero,(r5)
   beq      zero,r6,normal_hardware_interrupt_return
   store_info_and_load_new_task
   eret
normal_hardware_interrupt_return:
   movia    r1, OS_INTERRUPT_CONTEXT    // TELL OS WE EXIT  INTERRUPT PROCESS
   stw      zero,(r1)
   load_register_from_stack
   eret
emOS_unknown_exception:
   stw   ea,72(sp)                  //Abort current instruction  which cause IRQ
   addi r4,zero,3 //Error code
   addi r5,ea,-4  //Instruction where error occured
   call OS_Err_Handler
nios2_emOS_Start_OS:
   movia    r5, NextTCB
   movia    r3, CurrentTCB
   ldw      r6, (r5)
   stw      r6, (r3)             //update CurrentTCB
   ldw      sp, (r6)             //Load stack pointer
   movia    r1, OS_INTERRUPT_CONTEXT    // TELL OS WE EXIT  INTERRUPT PROCESS
   stw      zero,(r1)
   load_register_from_stack
   eret
CPU_CHANGE_CONTEXT_IN_CRITICAL_SECTION:
   save_register_to_stack
   rdctl r5,status               //Because we change in normal process =>save status instead of estatus and ra instead of ea
   stw   r5,68(sp)
   stw   ra,72(sp)
   store_info_and_load_new_task
   eret
#endif
