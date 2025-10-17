/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>

//Q1 : reprendre tp3
#define c0_idx  1
#define d0_idx  2
#define c3_idx  3
#define d3_idx  4
#define ts_idx  5

#define c0_sel  gdt_krn_seg_sel(c0_idx)
#define d0_sel  gdt_krn_seg_sel(d0_idx)
#define c3_sel  gdt_usr_seg_sel(c3_idx)
#define d3_sel  gdt_usr_seg_sel(d3_idx)
#define ts_sel  gdt_krn_seg_sel(ts_idx)

seg_desc_t GDT[6];
tss_t      TSS;

#define gdt_flat_dsc(_dSc_,_pVl_,_tYp_)                                 \
   ({                                                                   \
      (_dSc_)->raw     = 0;                                             \
      (_dSc_)->limit_1 = 0xffff;                                        \
      (_dSc_)->limit_2 = 0xf;                                           \
      (_dSc_)->type    = _tYp_;                                         \
      (_dSc_)->dpl     = _pVl_;                                         \
      (_dSc_)->d       = 1;                                             \
      (_dSc_)->g       = 1;                                             \
      (_dSc_)->s       = 1;                                             \
      (_dSc_)->p       = 1;                                             \
   })

#define tss_dsc(_dSc_,_tSs_)                                            \
   ({                                                                   \
      raw32_t addr    = {.raw = _tSs_};                                 \
      (_dSc_)->raw    = sizeof(tss_t);                                  \
      (_dSc_)->base_1 = addr.wlow;                                      \
      (_dSc_)->base_2 = addr._whigh.blow;                               \
      (_dSc_)->base_3 = addr._whigh.bhigh;                              \
      (_dSc_)->type   = SEG_DESC_SYS_TSS_AVL_32;                        \
      (_dSc_)->p      = 1;                                              \
   })

#define c0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_CODE_XR)
#define d0_dsc(_d) gdt_flat_dsc(_d,0,SEG_DESC_DATA_RW)
#define c3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_CODE_XR)
#define d3_dsc(_d) gdt_flat_dsc(_d,3,SEG_DESC_DATA_RW)

void init_gdt() {
   gdt_reg_t gdtr;

   GDT[0].raw = 0ULL; //no field raw ??? c'est le code de la prof ?

   c0_dsc( &GDT[c0_idx] );
   d0_dsc( &GDT[d0_idx] );
   c3_dsc( &GDT[c3_idx] );
   d3_dsc( &GDT[d3_idx] );

   gdtr.desc  = GDT;
   gdtr.limit = sizeof(GDT) - 1;
   set_gdtr(gdtr);

   set_cs(c0_sel);

   set_ss(d0_sel);
   set_ds(d0_sel);
   set_es(d0_sel);
   set_fs(d0_sel);
   set_gs(d0_sel);
}

void userland() {
   debug("in userland : \\o/\n");
   asm volatile ("mov %eax, %cr0");
}

void ring3() {
   init_gdt();
   set_ds(d3_sel);
   set_es(d3_sel);
   set_fs(d3_sel);
   set_gs(d3_sel);
   // Note: TSS is needed for the "kernel stack"
   // when returning from ring 3 to ring 0
   // during a next interrupt occurence
   TSS.s0.esp = get_ebp();
   TSS.s0.ss  = d0_sel;
   tss_dsc(&GDT[ts_idx], (offset_t)&TSS);
   set_tr(ts_sel);
   //Pourquoi autant ? On fait pas juste comme le tp1 ?? - J

   asm volatile (
   "push %0    \n" // ss
   "push %%ebp \n" // esp
   "pushf      \n" // eflags
   "push %1    \n" // cs
   "push %2    \n" // eip

   "iret"
   ::
    "i"(d3_sel),
    "i"(c3_sel),
    "r"(&userland)
   );
}


//Q5 : Modifier la fonction `syscall_handler()` pour qu'elle affiche une chaîne de caractères dont l'adresse se trouve dans le registre "ESI". 
//Nous venons de créer un appel système permettant d'afficher un message à l'écran et prenant son argument via "ESI"
//--> Pas fait encore
void syscall_isr() {
   asm volatile (
      "leave ; pusha        \n"
      "mov %esp, %eax      \n"
      "call syscall_handler \n"
      "popa ; iret"
      );
}

void __regparm__(1) syscall_handler(int_ctx_t *ctx) {
   debug("SYSCALL eax = %p\n", (void *) ctx->gpr.eax.raw);
}

void userland() {
    // TODO à compléter
   //Q3: on test leur code --> Voir le résultat
   uint32_t arg =  0x2023;
   asm volatile ("int $48"::"a"(arg));
   while(1);
}

void tp() {
   ring3();
    // TODO

    //Q2 : mettre syscall_isr en réponse à l'interruption 48 - inspi tp2
   idt_reg_t idtr;//dans intr.h
   get_idtr(idtr);
   int_desc_t *bp_dsc = &idtr.desc[3];
   bp_dsc->offset_1 = (uint16_t)((uint32_t)syscall_isr);
   bp_dsc->offset_2 = (uint16_t)(((uint32_t)syscall_isr)>>16);

   //Q4 :  Pourquoi observe-t-on une #GP ? Corriger le problème de sorte qu'il soit autorisé d'appeler l'interruption "48" avec un RPL à 3.
   bp_dsc->dpl = 3; //dissonance des niveaux de privilège





}
