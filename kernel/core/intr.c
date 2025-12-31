/* GPLv2 (c) Airbus */
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


extern uint32_t __kernel_stack_user1_base__, __kernel_stack_user1_end__;
extern uint32_t __kernel_stack_user2_base__, __kernel_stack_user2_end__;

uint32_t k1_base = (uint32_t)&__kernel_stack_user1_base__;
uint32_t k1_end  = (uint32_t)&__kernel_stack_user1_end__;

uint32_t k2_base = (uint32_t)&__kernel_stack_user2_base__;
uint32_t k2_end  = (uint32_t)&__kernel_stack_user2_end__;

extern info_t *info;
extern void idt_trampoline();
static int_desc_t IDT[IDT_NR_DESC];


uint32_t syscall_hdlr(int_ctx_t *ctx)
{
    uint32_t user_ptr = ctx->gpr.eax.raw;  // On recupere registre eax passe par user2
                                          // et donc @ de compteur (cf sys_counter)

    uint32_t value = *(volatile uint32_t*)user_ptr;  // lecture dans la page partagee
    
    static uint32_t last = 0;

    // if afin de n'afficher la valeur du compteur qu'une seule fois
    if (value != last) {
        debug("[syscall_hdlr] --- compteur=%u --- \n", value); // affichage par ring 0 du compteur 
        
        // debug pour verif
        //debug("[syscall_hdlr] - (ptr=0x%x)\n", user_ptr); 
        
        last = value;
    }

    ctx->gpr.eax.raw = 0; // On met 0 dans eax

    // On revient a intr.c
    return (uint32_t)ctx;
}

void intr_init()
{
   idt_reg_t idtr;
   offset_t  isr;
   size_t    i;

   isr = (offset_t)idt_trampoline;

   /* re-use default grub GDT code descriptor */
   for(i=0 ; i<IDT_NR_DESC ; i++, isr += IDT_ISR_ALGN)
      build_int_desc(&IDT[i], gdt_krn_seg_sel(1), isr);

   // syscall : les users en ring3 doivent pouvoir faire int 0x80
   IDT[0x80].dpl = 3;
   IDT[0x80].p  = 1;

   idtr.desc  = IDT;
   idtr.limit = sizeof(IDT) - 1;
   set_idtr(idtr);
}

uint32_t __regparm__(1) intr_hdlr(int_ctx_t *ctx)
{

   /*
   debug("\nIDT event\n"
         " . int    #%d\n"
         " . error  0x%x\n"
         " . cs:eip 0x%x:0x%x\n"
         " . ss:esp 0x%x:0x%x\n"
         " . eflags 0x%x\n"
         "\n- GPR\n"
         "eax     : 0x%x\n"
         "ecx     : 0x%x\n"
         "edx     : 0x%x\n"
         "ebx     : 0x%x\n"
         "esp     : 0x%x\n"
         "ebp     : 0x%x\n"
         "esi     : 0x%x\n"
         "edi     : 0x%x\n"
         ,ctx->nr.raw, ctx->err.raw
         ,ctx->cs.raw, ctx->eip.raw
         ,ctx->ss.raw, ctx->esp.raw
         ,ctx->eflags.raw
         ,ctx->gpr.eax.raw
         ,ctx->gpr.ecx.raw
         ,ctx->gpr.edx.raw
         ,ctx->gpr.ebx.raw
         ,ctx->gpr.esp.raw
         ,ctx->gpr.ebp.raw
         ,ctx->gpr.esi.raw
         ,ctx->gpr.edi.raw);
   */

   uint8_t vector = ctx->nr.blow;

   /* Exceptions generales du CPU */
   if (vector < NR_EXCP) {
   excp_hdlr(ctx);
   return (uint32_t)ctx;
   }

   // Syscall 80 -> Appel du handler dans tp.c
   if (vector == 0x80) {
      syscall_hdlr(ctx);

      // Retour ici de syscall.c/syscall_hdlr
      return (uint32_t)ctx; // On ne change pas de user (pas de switch de contexte ici) car ce n'est pas irq0
   }

   // Gestion de irq0
   if(vector == 32){

      // cpl au moment de l'interruption
      uint32_t cpl = ctx->cs.raw & 3;
      // esp de la pile noyau courant
      uint32_t kstack_ptr = (uint32_t)ctx;

      //debug("Int 32 ! \n");

      if(cpl == 3){
         //debug("l'interruption a ete declanchee par un user\n");

         if (current_task == &task1) {

            
            // On sauvegarde le pointeur de pile kernel de user 1 dans sa struct
            task1.kstack_ptr = (uint32_t)ctx;

            // Switch la tache courante
            current_task = &task2;

            // Switch CR3
            set_cr3(task2.cr3);

            // Mise a jour system_tss avec bonne pile kernel pour la prochaine interruption
            //system_tss.s0.esp = (uint32_t)&__kernel_stack_user2_end__; // adresse 0x00c0 2000
            system_tss.s0.esp = (uint32_t)0x00c01000 + STACK_SZ;
            system_tss.s0.ss  = gdt_krn_seg_sel(3);

            // SWITCH DEBUG
            //debug("SWITCH user 1 ->user 2 \n");
            //debug("u2.kesp=0x%x u2.cr3=0x%x system_tss.esp0=0x%x\n",task2.kernel_esp, task2.cr3, system_tss.s0.esp);

            //On envoit le nouveau esp pile user 2 a idt.s
            return task2.kstack_ptr;
         } 
         else if (current_task == &task2) {

            // On sauvegarde le pointeur de pile kernel de user 1 dans sa struct
            task2.kstack_ptr = (uint32_t)ctx;

            // Switch la tache courante
            current_task = &task1;

            // Switch CR3
            set_cr3(task1.cr3);

            // Mise a jour system_tss avec bonne pile kernel pour la prochaine interruption
            //system_tss.s0.esp = (uint32_t)&__kernel_stack_user1_end__; // adresse 0x00c0 2000
            system_tss.s0.esp = (uint32_t)(0x00c00000+STACK_SZ);
            system_tss.s0.ss  = gdt_krn_seg_sel(2);

            // SWITCH DEBUG
            //debug("SWITCH user 2 ->user 1 \n");

            //On envoit le nouveau esp pile user 1 a idt.s
            return task1.kstack_ptr;
         }
         else {
            debug("IRQ sur pile noyau inconnue (kesp=0x%x)\n", kstack_ptr);
            debug("Erreur lors de la recuperation du user en cours au moment de l'irq0\n");
         }

      }else{
         debug("[intr.c] - l'interruption a ete declanchee par le noyau\n");
      }
   }else{
      debug("[intr.c] - l'interruption declanchee n'est pas l'interruption 32\n");
   }
   // IRQ depuis ring0 : pas de switch
   return (uint32_t)ctx;
}

