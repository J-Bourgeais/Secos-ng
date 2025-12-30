#include <intr.h>
#include <debug.h>
#include <info.h>
#include <segmem.h>
#include <grub_mbi.h>
#include <cr.h>
#include <../../tp_exam/task.h>

#define STACK_SZ          0x1000
//#include <../../tp_exam/tp.c>

extern process_t task1;
extern process_t task2;
extern process_t *current_task;
extern tss_t system_tss;

extern uint32_t __kernel_stack_user1_end__, __kernel_stack_user2_end__;

extern info_t *info;
extern void idt_trampoline();
static int_desc_t IDT[IDT_NR_DESC];

/**
 * Handles the 0x80 System Call.
 * Reads the counter value from the virtual address passed in EAX.
 */
uint32_t syscall_hdlr(int_ctx_t *ctx)
{
    /* Retrieve virtual pointer passed from userland via EAX */
    uint32_t user_ptr = ctx->gpr.eax.raw; 

    /* Access shared memory page through task's virtual mapping */
    uint32_t value = *(volatile uint32_t*)user_ptr; 
    
    static uint32_t last = 0xFFFFFFFF;

    /* Display counter only when the value changes */
    if (value != last) {
        debug("[syscall_hdlr] Counter Value: %u\n", value);
        last = value;
    }

    ctx->gpr.eax.raw = 0; // Return value 0
    return (uint32_t)ctx;
}

/**
 * Initializes the IDT and sets 0x80 DPL to 3 to allow userland calls.
 */
void intr_init()
{
   idtr_t idtr;
   offset_t  isr;
   size_t    i;

   isr = (offset_t)idt_trampoline;

   /* Setup IDT descriptors */
   for(i=0 ; i<IDT_NR_DESC ; i++, isr += IDT_ISR_ALGN)
      build_int_desc(&IDT[i], gdt_krn_seg_sel(1), isr);

   /* Syscall 0x80: Set DPL to 3 so Ring 3 can trigger it */
   IDT[0x80].dpl = 3;
   IDT[0x80].p   = 1;

   idtr.desc  = IDT;
   idtr.limit = sizeof(IDT) - 1;
   set_idtr(idtr);
}

/**
 * Main interrupt handler. Manages CPU exceptions, Syscalls, and IRQ0 Scheduler.
 */
uint32_t __regparm__(1) intr_hdlr(int_ctx_t *ctx)
{
   uint8_t vector = ctx->nr.blow;

   /* 1. Handle CPU Exceptions */
   if (vector < NR_EXCP) {
       excp_hdlr(ctx);
       return (uint32_t)ctx;
   }

   /* 2. Handle System Call (int 0x80) */
   if (vector == 0x80) {
      return syscall_hdlr(ctx);
   }

   /* 3. Handle Timer (IRQ 0 / Vector 32) - Preemptive Scheduler */
   if(vector == 32){
      /* Determine CPL at the time of interruption */
      uint32_t cpl = ctx->cs.raw & 3;

      /* Only perform context switch if we interrupted Userland (Ring 3) */
      if(cpl == 3){
         if (current_task == &task1) {
            /* Save current state into Task 1 structure */
            task1.kstack_ptr = (uint32_t)ctx;

            /* Switch to Task 2 */
            current_task = &task2;
            set_cr3(task2.cr3);

            // Mise a jour system_tss avec bonne pile kernel pour la prochaine interruption
            //system_tss.s0.esp = (uint32_t)&__kernel_stack_user2_end__; // adresse 0x00c0 2000
            system_tss.s0.esp = (uint32_t)0x00c01000 + STACK_SZ;
            system_tss.s0.ss  = gdt_krn_seg_sel(3);

            /* Return Task 2's stack pointer to idt.s for restoration */
            return task2.kstack_ptr;
         } 
         else if (current_task == &task2) {
            /* Save current state into Task 2 structure */
            task2.kstack_ptr = (uint32_t)ctx;

            /* Switch back to Task 1 */
            current_task = &task1;
            set_cr3(task1.cr3);

            // Mise a jour system_tss avec bonne pile kernel pour la prochaine interruption
            //system_tss.s0.esp = (uint32_t)&__kernel_stack_user1_end__; // adresse 0x00c0 2000
            system_tss.s0.esp = (uint32_t)(0x00c00000+STACK_SZ);
            system_tss.s0.ss  = gdt_krn_seg_sel(2);

            return task1.kstack_ptr;
         }
      } else {
         /* Interrupted the kernel itself - skip scheduling switch */
         // debug("[intr] Timer tick in Kernel mode\n");
      }
   }

   /* Return current context for non-switching interrupts */
   return (uint32_t)ctx;
}

