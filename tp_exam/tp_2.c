/* GPLv2 (c) Airbus */
#include <debug.h>
#include <segmem.h>
#include <grub_mbi.h>
#include <info.h>
#include <cr.h>
#include <pagemem.h>
#include <intr.h>
#include <pic.h>
#include <io.h>

/*Besoin d'également modifier idt.s, intr.c et linker.lds --> Voir TP Margot & Léa*/


extern info_t   *info;

// Les codes users et noyau
extern uint32_t __kernel_start__, __kernel_end__;
extern uint32_t __user_start__, __user_end__;

// Les piles ring3 user 1 et user 2
extern uint32_t __user1_stack_base__, __user1_stack_end__;
extern uint32_t __user2_stack_base__, __user2_stack_end__;

// La zone partagee
extern uint32_t __shared_base__, __shared_end__;

// Les piles ring0 user 1, user 2 et noyau
extern uint32_t __kernel_stack_user1_base__, __kernel_stack_user1_end__;
extern uint32_t __kernel_stack_user2_base__, __kernel_stack_user2_end__;
extern uint32_t __kernel_stack_base__, __kernel_stack_end__;


#define __user__ __attribute__((section(".user")))

#define __user_data__ __attribute__((section(".user_data"),aligned(4096))) // Pour les piles utilisateurs

#define USER_STACK_SIZE 0x1000 // Piles utilisateurs de 4KB

typedef struct {
  uint32_t kernel_base; // base - pile noyau de la tâche
  uint32_t kernel_esp;  // esp pile noyau 
  uint32_t cr3;         // CR3
} task_t;

task_t task_user1;
task_t task_user2;
task_t *current_task = 0;

// Tache user 1
void __user__ user1() {

    volatile uint32_t *counter = (volatile uint32_t*)VADDR_COUNTER_USER1;
    while (1){

        // Petit delai afin de voir l'incrementation de 1 en 1 dans les affichages de user 2
        for (volatile int i=0; i<4000000; i++){
            asm volatile("nop");
        }

        (*counter)++;
    }
}

// Tache user 2
void __user__ user2() {

    volatile uint32_t *counter = (volatile uint32_t*)VADDR_COUNTER_USER2; 

    while (1){
    
        // Appel systeme pour l'affichage de la valeur du compteur par ring 0
        asm volatile(
        "movl %0, %%eax \n\t"
        "int $0x80      \n\t"
        :
        : "r"(counter)
        : "eax", "memory"
        );
    }
}


/* Function GDT setup */

seg_desc_t my_gdt[11];
void segmentation_setup_gdt(void){

    my_gdt[0].raw = 0ULL;

    //GDT[1] : Code ring 0 - noyau 
    my_gdt[1].limit_1 = 0xFFFF;
    my_gdt[1].base_1 = 0x0000;
    my_gdt[1].base_2 = 0x00;
    my_gdt[1].type = 11;
    my_gdt[1].s = 1;
    my_gdt[1].dpl = 0;
    my_gdt[1].p = 1;
    my_gdt[1].limit_2 = 0xF;
    my_gdt[1].avl = 1;
    my_gdt[1].l = 0;
    my_gdt[1].d = 1;
    my_gdt[1].g = 1;
    my_gdt[1].base_3 = 0x00;

    // Pile noyau ring 0 - User 1
    my_gdt[2].limit_1 = 0xFFFF;
    my_gdt[2].base_1 = 0x0000;
    my_gdt[2].base_2 = 0x00;
    my_gdt[2].type = 3;
    my_gdt[2].s = 1;
    my_gdt[2].dpl = 0;
    my_gdt[2].p = 1;
    my_gdt[2].limit_2 = 0xF;
    my_gdt[2].avl = 1;
    my_gdt[2].l = 0;
    my_gdt[2].d = 1;
    my_gdt[2].g = 1;
    my_gdt[2].base_3 = 0x00;

    // Pile noyau ring 0 - User 2
    my_gdt[3].limit_1 = 0xFFFF;
    my_gdt[3].base_1 = 0x0000;
    my_gdt[3].base_2 = 0x00;
    my_gdt[3].type = 3;
    my_gdt[3].s = 1;
    my_gdt[3].dpl = 0;
    my_gdt[3].p = 1;
    my_gdt[3].limit_2 = 0xF;
    my_gdt[3].avl = 1;
    my_gdt[3].l = 0;
    my_gdt[3].d = 1;
    my_gdt[3].g = 1;
    my_gdt[3].base_3 = 0x00;

    // Code ring 3 - User 1
    my_gdt[4].limit_1 = 0xFFFF;
    my_gdt[4].base_1 = 0x0000;
    my_gdt[4].base_2 = 0x00;
    my_gdt[4].type = 11;
    my_gdt[4].s = 1;
    my_gdt[4].dpl = 3;
    my_gdt[4].p = 1;
    my_gdt[4].limit_2 = 0xF;
    my_gdt[4].avl = 1;
    my_gdt[4].l = 0;
    my_gdt[4].d = 1;
    my_gdt[4].g = 1;
    my_gdt[4].base_3 = 0x00;

    // Pile ring 3 - User 1
    my_gdt[5].limit_1 = 0xFFFF;
    my_gdt[5].base_1 = 0x0000;
    my_gdt[5].base_2 = 0x00;
    my_gdt[5].type = 3;
    my_gdt[5].s = 1;
    my_gdt[5].dpl = 3;
    my_gdt[5].p = 1;
    my_gdt[5].limit_2 = 0xF;
    my_gdt[5].avl = 1;
    my_gdt[5].l = 0;
    my_gdt[5].d = 1;
    my_gdt[5].g = 1;
    my_gdt[5].base_3 = 0x00;

    // Code ring 3 - User 2
    my_gdt[6].limit_1 = 0xFFFF;
    my_gdt[6].base_1 = 0x0000;
    my_gdt[6].base_2 = 0x00;
    my_gdt[6].type = 11;
    my_gdt[6].s = 1;
    my_gdt[6].dpl = 3;
    my_gdt[6].p = 1;
    my_gdt[6].limit_2 = 0xF;
    my_gdt[6].avl = 1;
    my_gdt[6].l = 0;
    my_gdt[6].d = 1;
    my_gdt[6].g = 1;
    my_gdt[6].base_3 = 0x00;

    // Pile ring 3 - User 2
    my_gdt[7].limit_1 = 0xFFFF;
    my_gdt[7].base_1 = 0x0000;
    my_gdt[7].base_2 = 0x00;
    my_gdt[7].type = 3;
    my_gdt[7].s = 1;
    my_gdt[7].dpl = 3;
    my_gdt[7].p = 1;
    my_gdt[7].limit_2 = 0xF;
    my_gdt[7].avl = 1;
    my_gdt[7].l = 0;
    my_gdt[7].d = 1;
    my_gdt[7].g = 1;
    my_gdt[7].base_3 = 0x00;

    // Data ring 3 - Partage User 1 & User 2
    my_gdt[8].limit_1 = 0xFFFF;
    my_gdt[8].base_1 = 0x0000;
    my_gdt[8].base_2 = 0x00;
    my_gdt[8].type = 3;
    my_gdt[8].s = 1;
    my_gdt[8].dpl = 3;
    my_gdt[8].p = 1;
    my_gdt[8].limit_2 = 0xF;
    my_gdt[8].avl = 1;
    my_gdt[8].l = 0;
    my_gdt[8].d = 1;
    my_gdt[8].g = 1;
    my_gdt[8].base_3 = 0x00;

    // Pile noyau ring 0
    my_gdt[9].limit_1 = 0xFFFF;
    my_gdt[9].base_1 = 0x0000;
    my_gdt[9].base_2 = 0x00;
    my_gdt[9].type = 3;
    my_gdt[9].s = 1;
    my_gdt[9].dpl = 0;
    my_gdt[9].p = 1;
    my_gdt[9].limit_2 = 0xF;
    my_gdt[9].avl = 1;
    my_gdt[9].l = 0;
    my_gdt[9].d = 1;
    my_gdt[9].g = 1;
    my_gdt[9].base_3 = 0x00;

    // Mise a jour de la GDTR
    gdt_reg_t gdtr_of_my_gdt;
    gdtr_of_my_gdt.addr = (long unsigned int) my_gdt;
    gdtr_of_my_gdt.limit = sizeof(my_gdt) - 1;
    
    set_gdtr(gdtr_of_my_gdt); // Update du registre GDTR
    
    //Mettre a jour les selecteurs de segments
    set_cs(gdt_krn_seg_sel(1));
    set_ds(gdt_krn_seg_sel(9));
    set_ss(gdt_krn_seg_sel(9));

}

/* Function TSS setup */

tss_t TSS;

void tss_setup(void){

    memset(&TSS, 0, sizeof(TSS));

    // Si le CPU rentres en ring 0, il utilisera cette pile -> Changera a chaque changement de tache
    TSS.s0.esp = get_ebp();            // haut de la pile kernel (ring 0)
    TSS.s0.ss  = gdt_krn_seg_sel(9);   // segment data ring 0

    // Construction du descripteur TSS dans la GDT à l'indice 10
    {
        uint32_t base  = (uint32_t)&TSS;
        uint32_t limit = sizeof(TSS) - 1;

        my_gdt[10].limit_1 = limit & 0xFFFF;
        my_gdt[10].base_1  = base & 0xFFFF;
        my_gdt[10].base_2  = (base >> 16) & 0xFF;
        my_gdt[10].type    = 0x9;
        my_gdt[10].s       = 0;
        my_gdt[10].dpl     = 0;
        my_gdt[10].p       = 1;
        my_gdt[10].limit_2 = (limit >> 16) & 0xF;
        my_gdt[10].avl     = 0;
        my_gdt[10].l       = 0;
        my_gdt[10].d       = 0;
        my_gdt[10].g       = 0;
        my_gdt[10].base_3  = (base >> 24) & 0xFF;
    }

    set_tr(gdt_krn_seg_sel(10)); // On met l'entree TSS de la GDT dans le registre TR
}


/* Functions Kernel Paging Setup */

// Adresses des tables paging noyau (choisis une zone différente de user1/user2)
#define K_PGD_PA   0x00600000u
#define K_PTB0_PA  0x00601000u  // PDE 0 : 0..4MB
#define K_PTB1_PA  0x00602000u  // PDE 1 : 4..8MB
#define K_PTB2_PA  0x00603000u  // PDE 2 : 8..12MB
#define K_PTB3_PA  0x00604000u  // PDE 3 : 12..16MB

static inline void ptb_clear(pte32_t *ptb) {
    memset((void*)ptb, 0, PAGE_SIZE);
}
static inline void pgd_clear(pde32_t *pgd) {
    memset((void*)pgd, 0, PAGE_SIZE);
}

static inline void map_ptb_identity(pte32_t *ptb, uint32_t base_frame, uint32_t flags) {
    for (int i = 0; i < 1024; i++) {
        pg_set_entry(&ptb[i], flags, base_frame + (uint32_t)i);
    }
}

void paging_setup_kernel(void)
{
    pde32_t *pgd_kernel = (pde32_t*)K_PGD_PA;

    pte32_t *ptb0_kernel = (pte32_t*)K_PTB0_PA;
    pte32_t *ptb1_kernel = (pte32_t*)K_PTB1_PA;
    pte32_t *ptb2_kernel = (pte32_t*)K_PTB2_PA;
    pte32_t *ptb3_kernel = (pte32_t*)K_PTB3_PA;

    // Nettoyage
    pgd_clear(pgd_kernel);
    ptb_clear(ptb0_kernel);
    ptb_clear(ptb1_kernel);
    ptb_clear(ptb2_kernel);
    ptb_clear(ptb3_kernel);

    /*  --- PDE 0 : 0..4MB (identity & supervisor) --- */
    map_ptb_identity(ptb0_kernel, 0, PG_KRN | PG_RW);
    pg_set_entry(&pgd_kernel[0], PG_KRN | PG_RW, page_get_nr(ptb0_kernel));


    /*  --- PDE 1 : 4..8MB (identity & supervisor) --- */
    map_ptb_identity(ptb1_kernel, 1024, PG_KRN | PG_RW);
    pg_set_entry(&pgd_kernel[1], PG_KRN | PG_RW, page_get_nr(ptb1_kernel));


    /*  --- PDE 2 : 8..12MB (identity & supervisor) --- */
    map_ptb_identity(ptb2_kernel, 2048, PG_KRN | PG_RW);
    pg_set_entry(&pgd_kernel[2], PG_KRN | PG_RW, page_get_nr(ptb2_kernel));

    /*  --- PDE 2 : 12..16MB (identity & supervisor) --- */
    map_ptb_identity(ptb3_kernel, 3072, PG_KRN | PG_RW);
    pg_set_entry(&pgd_kernel[3], PG_KRN | PG_RW, page_get_nr(ptb3_kernel));

    // Charger CR3
    set_cr3((uint32_t)pgd_kernel);
}


/* Functions User 1 Paging Setup */

// extern uint32_t __kernel_start__, __kernel_end__;
// extern uint32_t __user_start__,   __user_end__;
// extern uint32_t __user1_stack_base__, __user1_stack_end__;
// extern uint32_t __shared_base__, __shared_end__;
// extern uint32_t __kernel_stack_user1_base__, __kernel_stack_user1_end__;
// extern uint32_t __kernel_stack_user2_base__, __kernel_stack_user2_end__;

// Choix : VA partagee
#define USER1_SHARED_VA   ((void*)0xb0001000u)   // PDE=704, PTE=1

// Adresses des tables
#define U1_PGD_PA   0x00610000u
#define U1_PTB0_PA  0x00611000u  // PDE 0 : 0..4MB
#define U1_PTB1_PA  0x00612000u  // PDE 1 : 4..8MB
#define U1_PTB2_PA  0x00613000u  // PDE 2 : 8..12MB (pile user1, shared phys)
#define U1_PTB3_PA  0x00614000u  // PDE 3 : 12..16MB (piles noyau à 0x00c00000)
#define U1_PTB704_PA 0x00615000u  // PDE 320 : shared VA

static inline void ptb_clear(pte32_t *ptb) {
    memset((void*)ptb, 0, PAGE_SIZE);
}
static inline void pgd_clear(pde32_t *pgd) {
    memset((void*)pgd, 0, PAGE_SIZE);
}

static inline void map_ptb_identity(pte32_t *ptb, uint32_t base_frame, uint32_t flags) {
    // remplit 1024 PTE => 4MB
    for (int i = 0; i < 1024; i++) {
        pg_set_entry(&ptb[i], flags, base_frame + (uint32_t)i);
    }
}

// mappe une page dans une PTB avec va -> pa
static inline void map_page_in_ptb(pte32_t *ptb, void *va, uint32_t pa, uint32_t flags) {
    int pti = pt32_get_idx(va);
    pg_set_entry(&ptb[pti], flags, page_get_nr(pa));
}

pde32_t *pgd_user1 = (pde32_t*)U1_PGD_PA;

void paging_setup_user1(void)
{
    pte32_t *ptb0_user1  = (pte32_t*)U1_PTB0_PA;
    pte32_t *ptb1_user1  = (pte32_t*)U1_PTB1_PA;
    pte32_t *ptb2_user1  = (pte32_t*)U1_PTB2_PA;
    pte32_t *ptb3_user1  = (pte32_t*)U1_PTB3_PA;
    pte32_t *ptb704_user1 = (pte32_t*)U1_PTB704_PA;

    // on remplit toutes nos tables a 0
    pgd_clear(pgd_user1);
    ptb_clear(ptb0_user1);
    ptb_clear(ptb1_user1);
    ptb_clear(ptb2_user1);
    ptb_clear(ptb3_user1);
    ptb_clear(ptb704_user1);

    /* ---- PDE 0 ----*/

    // PDE0 correspond a 0..4MB donc on identity-map en supervisor
    // On a besoin de mapper car user1 doit pouvoir resoudre @ du handler intr_hdlr
    map_ptb_identity(ptb0_user1, 0, PG_KRN | PG_RW);
    pg_set_entry(&pgd_user1[0], PG_KRN | PG_RW, page_get_nr(ptb0_user1));

    /* ---- PDE 1 ----*/

    // PDE1 (4..8MB) : identity-map  du code USER (en ring 3)
    //PDE[1] en user (sinon impossible d’acceder ring3 au code user)
    pg_set_entry(&pgd_user1[1], PG_USR | PG_RW, page_get_nr(ptb1_user1));
    
    // Comme mes tables PGD, PTB, etc. sont dans accessibles physiquement par la meme PDE, 
    // J'ajuste au niveau des PTE pour que les @ physiques entre 0x0060 0000 a 0x0080 0000
    // soient accessibles uniquement en ring 0
    uint32_t base_frame = 1024;

    for (int i = 0; i < 1024; i++) {
        uint32_t flags = PG_RW;

        if (i < 512) flags |= PG_USR;  // 4MB..6MB = ring3
        else         flags |= PG_KRN;  // 6MB..8MB = ring0

        pg_set_entry(&ptb1_user1[i], flags, base_frame + (uint32_t)i);
    }

    /* ---- PDE 2 ----*/

    // PDE2 : 8..12MB : identity-map uniquement la pile ring3 user 1
    // pile ring3 user1 : 0x00800000 (index PTE 0)
    // On ne mappe pas en identity la page partagee a 0x00802000
    pg_set_entry(&pgd_user1[2], PG_USR | PG_RW, page_get_nr(ptb2_user1));
    map_page_in_ptb(ptb2_user1, (void*)0x00800000u, 0x00800000u, PG_USR | PG_RW);

    // Page partagee  : PA = 0x00802000, VA = USER1_SHARED_VA (0xb0001000)
    // PDE index de 0xb0001000 = 704, PTE index = 1
    pg_set_entry(&pgd_user1[pd32_get_idx(USER1_SHARED_VA)], PG_USR | PG_RW, page_get_nr(ptb704_user1));
    map_page_in_ptb(ptb704_user1, USER1_SHARED_VA, 0x00802000u, PG_USR | PG_RW);

    /* ---- PDE 3 ----*/

    // PDE3 : 12..16MB : on identity-map en supervisor pour les piles noyau
    map_ptb_identity(ptb3_user1, 3072, PG_KRN | PG_RW);
    pg_set_entry(&pgd_user1[3], PG_KRN | PG_RW, page_get_nr(ptb3_user1));

}

uint32_t get_user1_pgd_addr(void) {
    return (uint32_t)pgd_user1;
}


/* Functions User 2 Paging Setup */

#define USER2_SHARED_VA     ((void*)0x03a0c000u)   /* PDE=13, PTE=524 */

/* Adresses des tables user2 */
#define U2_PGD_PA     0x00620000u
#define U2_PTB0_PA    0x00621000u  /* PDE 0 */
#define U2_PTB1_PA    0x00622000u  /* PDE 1 */
#define U2_PTB2_PA    0x00623000u  /* PDE 2 */
#define U2_PTB3_PA    0x00624000u  /* PDE 3 */

#define U2_PTB13_PA  0x00625000u  /* PDE 13 : shared VA */

static inline void ptb_clear(pte32_t *ptb) {
    memset((void*)ptb, 0, PAGE_SIZE);
}
static inline void pgd_clear(pde32_t *pgd) {
    memset((void*)pgd, 0, PAGE_SIZE);
}

static inline void map_ptb_identity(pte32_t *ptb, uint32_t base_frame, uint32_t flags) {
    for (int i = 0; i < 1024; i++) {
        pg_set_entry(&ptb[i], flags, base_frame + (uint32_t)i);
    }
}

static inline void map_page_in_ptb(pte32_t *ptb, void *va, uint32_t pa, uint32_t flags) {
    int pti = pt32_get_idx(va);
    pg_set_entry(&ptb[pti], flags, page_get_nr(pa));
}

pde32_t *pgd_user2 = (pde32_t*)U2_PGD_PA;

void paging_setup_user2(void)
{
    pte32_t *ptb0_user2   = (pte32_t*)U2_PTB0_PA;
    pte32_t *ptb1_user2   = (pte32_t*)U2_PTB1_PA;
    pte32_t *ptb2_user2   = (pte32_t*)U2_PTB2_PA;
    pte32_t *ptb3_user2   = (pte32_t*)U2_PTB3_PA;
    pte32_t *ptb13_user2 = (pte32_t*)U2_PTB13_PA;

    /* Remise à zéro */
    pgd_clear(pgd_user2);
    ptb_clear(ptb0_user2);
    ptb_clear(ptb1_user2);
    ptb_clear(ptb2_user2);
    ptb_clear(ptb3_user2);
    ptb_clear(ptb13_user2);

    /* ---- PDE 0 ---- */

    // 0..4MB : identity-map en mode supervisor
    map_ptb_identity(ptb0_user2, 0, PG_KRN | PG_RW);
    pg_set_entry(&pgd_user2[0], PG_KRN | PG_RW, page_get_nr(ptb0_user2));

    /* ---- PDE 1 ---- */

    // 4..8MB, identity map pour code users (4..6MB) et ring0 pour tables (6..8MB) 
    pg_set_entry(&pgd_user2[1], PG_USR | PG_RW, page_get_nr(ptb1_user2));

    uint32_t base_frame = 1024;

    for (int i = 0; i < 1024; i++) {
        uint32_t flags = PG_RW;

        if (i < 512) flags |= PG_USR;  // 4MB..6MB = ring3
        else flags |= PG_KRN;  // 6MB..8MB = ring0

        pg_set_entry(&ptb1_user2[i], flags, base_frame + (uint32_t)i);
    }

    /* ---- PDE 2 ---- */
    // 8..12MB : on mappe que la pile ring3 user2 (0x00801000)
    pg_set_entry(&pgd_user2[2], PG_USR | PG_RW, page_get_nr(ptb2_user2));
    map_page_in_ptb(ptb2_user2, (void*)0x00801000u, 0x00801000u, PG_USR | PG_RW);

    /* ---- PDE 3 ---- */
    // 12..16MB : identity-map en mode supervisor (piles noyau à 0x00c00000)
    map_ptb_identity(ptb3_user2, 3072, PG_KRN | PG_RW);
    pg_set_entry(&pgd_user2[3], PG_KRN | PG_RW, page_get_nr(ptb3_user2));

    /* ---- PDE 13 ---- */
    // Configuration de VA user 2 -> @physique page partagee
    pg_set_entry(&pgd_user2[pd32_get_idx(USER2_SHARED_VA)], PG_USR | PG_RW, page_get_nr(ptb13_user2));
    map_page_in_ptb(ptb13_user2, USER2_SHARED_VA, 0x00802000u, PG_USR | PG_RW);

}

uint32_t get_user2_pgd_addr(void) {
    return (uint32_t)pgd_user2;
}


/* Functions Interruptions Setup */

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }

/* PIT (8253/8254) */
#define PIT_FREQ_HZ 1193182u
#define PIT_CMD     0x43
#define PIT_CH0     0x40

static void pit_init(uint32_t hz)
{
    if (hz == 0) hz = 100;
    uint32_t div = PIT_FREQ_HZ / hz;

    out(0x36, PIT_CMD);
    out((uint8_t)(div & 0xFF), PIT_CH0);
    out((uint8_t)((div >> 8) & 0xFF), PIT_CH0);
}

static void pic_unmask_irq0_only(void)
{
    // n'active que IRQ0 only
    out(0xFE, PIC_IMR(PIC1));

    // Masque tout en mode slave
    out(0xFF, PIC_IMR(PIC2));
}

void interruption_setup(uint32_t hz)
{
    // Desactive les interruptions (dont irq0) pendant la configuration
    cli(); 

    // Charger et initialiser l'IDT 
    intr_init();

    // Remappe les PIC (utilisation de pic.h)
    pic_init();

    // Autoriser uniquement IRQ0
    pic_unmask_irq0_only();

    // Configure la frequence du timer et donc des interruptions
    pit_init(hz);
}

*/


void tp() {

    /* ---------------  Configuration de la memoire physique ------------------- */

    // cf linker.lds pour comprendre l'agencement de la memoire

    debug("\n\n");
    debug("--- Affichage de l'organisation memoire physique (RAM) ---\n\n");
   // Declaration de la pile user 1
    __attribute__((section(".user1_stack"), aligned(16)))
    static uint8_t user1_stack[USER_STACK_SIZE];
    debug("user1_stack base=%p size=0x%x top(ESP init)=%p\n", user1_stack, (unsigned)USER_STACK_SIZE, user1_stack + USER_STACK_SIZE);

    // Declaration de la pile user 2
    __attribute__((section(".user2_stack"), aligned(16)))
    static uint8_t user2_stack[USER_STACK_SIZE];
    debug("user2_stack base=%p size=0x%x top(ESP init)=%p\n", user2_stack, (unsigned)USER_STACK_SIZE, user2_stack + USER_STACK_SIZE);

    // Declaration de la zone partagee
    __attribute__((section(".shared"), aligned(4096)))
    static uint8_t shared_page[4096];
    debug("shared_page base=%p size=0x%x top(ESP init)=%p\n", shared_page, (unsigned)USER_STACK_SIZE, shared_page + USER_STACK_SIZE);

    // Declaration de la pile noyau user 1
    __attribute__((section(".kernel_stack_user1"), aligned(4096)))
    static uint8_t kernel_stack_user1[4096];
    debug("kernel_stack_user1 base=%p size=0x%x top(ESP init)=%p\n", kernel_stack_user1, (unsigned)USER_STACK_SIZE, kernel_stack_user1 + USER_STACK_SIZE);

    // Declaration de la pile noyau user 2
    __attribute__((section(".kernel_stack_user2"), aligned(4096)))
    static uint8_t kernel_stack_user2[4096];
    debug("kernel_stack_user2 base=%p size=0x%x top(ESP init)=%p\n", kernel_stack_user2, (unsigned)USER_STACK_SIZE, kernel_stack_user2 + USER_STACK_SIZE);

    // Declaration de la pile noyau
    __attribute__((section(".kernel_stack"), aligned(4096)))
    static uint8_t kernel_stack[4096];
    debug("kernel_stack base=%p size=0x%x top(ESP init)=%p\n", kernel_stack, (unsigned)USER_STACK_SIZE, kernel_stack + USER_STACK_SIZE);

    debug("\n\n");

    /* ---------------  Configuration de segmentation ------------------- */

    // Creation de la GDT et mise a jour de GDTR et des selecteurs de segments avec code et pile noyau
	segmentation_setup_gdt();

    /* ---------------  Configuration de la TSS ------------------- */

    tss_setup(); // Mise de esp ring0 et ss ring0 dans TSS et mise a jour de TR

   /* ---------------  Configuration de la pagination ------------------- */

   /* -- Configuration des PGD et PTB (Utilisation de paging_setup.h) -- */
   
   paging_setup_kernel(); // met la PGD de kernel dans CR3
   
   paging_setup_user1();
   paging_setup_user2();

    // Activer Pagination
    uint32_t cr0 = get_cr0();
    set_cr0(cr0 | (1u << 31));
    
    // Mise a jour des CR3 des struct des tasks
    task_user1.cr3 = get_user1_pgd_addr();
    task_user2.cr3 = get_user2_pgd_addr();


    /* ---------------  Configuration des interruptions ------------------- */
    
    interruption_setup(1); // Fixe a 50 interruptions irq0 / seconde

    /* ---------------  Preparer la frame d'interruption de user 2 ------------------- */
    /* Necessaire pour le premier switch de contexte de intr_hdlr */
    
    /*** Creation des selecteurs de segments ***/

    // --- Creation du SS ring3 USER 2 ---
    seg_sel_t ss_ring3_user2;

    //segment dans la GDT
    ss_ring3_user2.index = 7;
    ss_ring3_user2.ti = 0;
    ss_ring3_user2.rpl = 3;

    uint16_t ss_selecteur_ring3_user2 = (ss_ring3_user2.index << 3) | (ss_ring3_user2.ti << 2) | (ss_ring3_user2.rpl);

    // --- Creation de ESP ---
    uint32_t esp_ring3_user2 = (uint32_t)user2_stack + sizeof(user2_stack);

    // --- Creation du CS ring3 ---
    seg_sel_t cs_ring3_user2;

    cs_ring3_user2.index = 6; //segment dans la GDT
    cs_ring3_user2.ti = 0;
    cs_ring3_user2.rpl = 3;

    uint16_t cs_selecteur_ring3_user2 = (cs_ring3_user2.index << 3) | (cs_ring3_user2.ti << 2) | (cs_ring3_user2.rpl);


    /*** Creation d'un int_ctx_t ***/

    // On recupere le esp de la pile noyau user 2
    uint32_t *sp = (uint32_t*)&__kernel_stack_user2_end__;

    // On Reserver la place de int_ctx_t sur la pile noyau
    sp = (uint32_t *)((uint8_t*)sp - sizeof(int_ctx_t));

    int_ctx_t *frame = (int_ctx_t*)sp;

    // Push des registres generaux (pour que le popa fonctionne)
    frame->gpr.eax.raw = 0;
    frame->gpr.ecx.raw = 0;
    frame->gpr.edx.raw = 0;
    frame->gpr.ebx.raw = 0;
    frame->gpr.esp.raw = 0;
    frame->gpr.ebp.raw = 0;
    frame->gpr.esi.raw = 0;
    frame->gpr.edi.raw = 0;

    // Champs CPU [ ajoute par le stub d'habitude (nr, err)]
    frame->nr.raw  = 32;      // comme si ça venait de IRQ0
    frame->err.raw = 0;

    frame->eip.raw = (uint32_t)user2;
    frame->cs.raw  = cs_selecteur_ring3_user2;
    frame->eflags.raw = EFLAGS_IF; // assure IF=1

    frame->esp.raw = esp_ring3_user2;
    frame->ss.raw  = ss_selecteur_ring3_user2;

    // Mise a jour de kernel_esp de la structure afin qu'il soit correctement 
    // Utilise dans intr_hdlr au moment du switch    
    task_user2.kernel_esp = (uint32_t)frame;


    /* ---------------  Initialisation en mode user1 ------------------- */

    current_task = &task_user1;

    /* Mise en place du contexte de user 1 sur la pile noyau pour qu'au moment du iret,
       on passe dans le code user1 */

    // --- Creation du SS ring3 User 1 ---
    seg_sel_t ss_ring3_user1;

    //segment dans la GDT
    ss_ring3_user1.index = 5;
    ss_ring3_user1.ti = 0;
    ss_ring3_user1.rpl = 3;

    uint16_t ss_selecteur_ring3_user1 = (ss_ring3_user1.index << 3) | (ss_ring3_user1.ti << 2) | (ss_ring3_user1.rpl);

    // --- Creation de ESP User 1 ---
    uint32_t esp_ring3_user1 = (uint32_t)user1_stack + sizeof(user1_stack);

    // --- Creation du CS ring3 User 1 ---
    seg_sel_t cs_ring3_user1;

    cs_ring3_user1.index = 4; //segment dans la GDT
    cs_ring3_user1.ti = 0;
    cs_ring3_user1.rpl = 3;

    uint16_t cs_selecteur_ring3_user1 = (cs_ring3_user1.index << 3) | (cs_ring3_user1.ti << 2) | (cs_ring3_user1.rpl);

    // --- Autres actions ---

    // changer la pile noyau de la TSS pour la pile noyau user 1
    TSS.s0.esp = (uint32_t)&__kernel_stack_user1_end__; // adresse 0x00c0 1000
    TSS.s0.ss  = gdt_krn_seg_sel(2);

    // Mise a jour de CR3
    set_cr3(task_user1.cr3);

    // Mise a jour de current task
    current_task = &task_user1;

    // Mise a 0 du compteur
    *(volatile uint32_t*)VADDR_COUNTER_USER1 = 0;

    // Affichage uniquement du compteur a partir d'ici
    debug("--- Affichage du compteur ---\n\n");

    // Mise de IF a 1 pour reactiver irq0 (equivalent a sti())
    uint32_t eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    eflags |= (1u << 9);   // IF = 1

    // --- Push du contexte ---

    asm volatile(
        "pushl %[ss]\n\t"
        "pushl %[uesp]\n\t"
        "pushl %[efl]\n\t"
        "pushl %[cs]\n\t"
        "pushl %[eip]\n\t"
        "iret\n\t"
        :
        : [ss]   "r"((uint32_t)ss_selecteur_ring3_user1),
        [uesp] "r"(esp_ring3_user1),
        [efl]  "r"(eflags),
        [cs]   "r"((uint32_t)cs_selecteur_ring3_user1),
        [eip]  "r"((uint32_t)user1)
        : "memory"
    );
    

}