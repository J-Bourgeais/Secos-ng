/* GPLv2 (c) Airbus - Revised Version */
#include <debug.h>
#include <segmem.h>
#include <grub_mbi.h>
#include <info.h>
#include <cr.h>
#include <pagemem.h>
#include <intr.h>
#include <pic.h>
#include <io.h>
#include <../../tp_exam/task.h>


extern info_t *info;

/* --- Symboles Externes (Linker) --- */
extern uint32_t __kernel_start__, __kernel_end__;
extern uint32_t __user_start__, __user_end__;
extern uint32_t __kernel_stack_user1_end__, __kernel_stack_user2_end__;

/* --- Configuration & Macros --- */
#define SECTION_USER      __attribute__((section(".user")))
#define SECTION_STACK_U   __attribute__((section(".user_data"), aligned(4096)))
#define PAGE_SZ           4096
#define STACK_SZ          0x1000

// Mapping Virtuel des compteurs
#define VA_SHARED_U1      ((void*)0xb0001000u)
#define VA_SHARED_U2      ((void*)0x03a0c000u)
#define PA_SHARED_PHYS    0x00802000u

// typedef struct {
//     uint32_t kstack_ptr; // ESP dans la pile noyau
//     uint32_t cr3;    // Registre CR3
// } process_t;

process_t task1, task2;
process_t *current_task = 0;
tss_t system_tss;
seg_desc_t gdt_table[11];

/* --- Fonctions Utilisateurs --- */

void SECTION_USER user1() {
    volatile uint32_t *cnt = (volatile uint32_t*)VA_SHARED_U1;
    while (1) {
        for (volatile int i = 0; i < 3500000; i++) asm volatile("nop");
        (*cnt)++;
    }
}

void SECTION_USER user2() {
    volatile uint32_t *cnt = (volatile uint32_t*)VA_SHARED_U2;
    while (1) {
        // Appel Système (INT 0x80) pour affichage
        asm volatile("movl %0, %%eax; int $0x80" : : "r"(cnt) : "eax", "memory");
    }
}

/* --- Gestion de la Segmentation --- */

static void gdt_entry_init(int idx, uint32_t base, uint32_t limit, uint8_t type, uint8_t dpl) {
    gdt_table[idx].limit_1 = limit & 0xFFFF;
    gdt_table[idx].base_1  = base & 0xFFFF;
    gdt_table[idx].base_2  = (base >> 16) & 0xFF;
    gdt_table[idx].type    = type;
    gdt_table[idx].s       = 1;
    gdt_table[idx].dpl     = dpl;
    gdt_table[idx].p       = 1;
    gdt_table[idx].limit_2 = (limit >> 16) & 0xF;
    gdt_table[idx].avl     = 1;
    gdt_table[idx].l       = 0;
    gdt_table[idx].d       = 1;
    gdt_table[idx].g       = 1;
    gdt_table[idx].base_3  = (base >> 24) & 0xFF;
}

void setup_segmentation(void) {
    memset(gdt_table, 0, sizeof(gdt_table));

    // Segments Noyau (Index 1-3)
    gdt_entry_init(1, 0, 0xFFFFF, 11, 0); // Code Ring 0
    gdt_entry_init(2, 0, 0xFFFFF, 3, 0);  // Data/Stack Ring 0 (U1)
    gdt_entry_init(3, 0, 0xFFFFF, 3, 0);  // Data/Stack Ring 0 (U2)

    // Segments Utilisateurs (Index 4-8)
    gdt_entry_init(4, 0, 0xFFFFF, 11, 3); // Code U1
    gdt_entry_init(5, 0, 0xFFFFF, 3, 3);  // Stack U1
    gdt_entry_init(6, 0, 0xFFFFF, 11, 3); // Code U2
    gdt_entry_init(7, 0, 0xFFFFF, 3, 3);  // Stack U2
    gdt_entry_init(8, 0, 0xFFFFF, 3, 3);  // Shared Data
    
    gdt_entry_init(9, 0, 0xFFFFF, 3, 0);  // Kernel Data global

    gdt_reg_t r_gdt = { .addr = (uint32_t)gdt_table, .limit = sizeof(gdt_table) - 1 };
    set_gdtr(r_gdt);

    set_cs(gdt_krn_seg_sel(1));
    set_ds((uint16_t)gdt_krn_seg_sel(9));
    set_ss((uint16_t)gdt_krn_seg_sel(9));
}

void setup_tss(void) {
    memset(&system_tss, 0, sizeof(tss_t));
    system_tss.s0.ss = gdt_krn_seg_sel(9);
    
    uint32_t base = (uint32_t)&system_tss;
    uint32_t limit = sizeof(system_tss) - 1;

    gdt_table[10].limit_1 = limit & 0xFFFF;
    gdt_table[10].base_1  = base & 0xFFFF;
    gdt_table[10].base_2  = (base >> 16) & 0xFF;
    gdt_table[10].type    = 0x9; // TSS 32-bit available
    gdt_table[10].s       = 0;
    gdt_table[10].dpl     = 0;
    gdt_table[10].p       = 1;
    gdt_table[10].base_3  = (base >> 24) & 0xFF;

    set_tr(gdt_krn_seg_sel(10));
}

/* --- Gestion de la Pagination --- */

static void map_region_identity(pte32_t *ptb, uint32_t start_frame, uint32_t count, uint32_t flags) {
    for (uint32_t i = 0; i < count; i++) {
        pg_set_entry(&ptb[i], flags, start_frame + i);
    }
}

// Initialise un PGD de base avec l'identité noyau (0-4MB et 12-16MB)
uint32_t create_base_pgd(uint32_t pgd_pa, uint32_t ptb_kern_pa, uint32_t ptb_stack_pa) {
    pde32_t *pgd = (pde32_t*)pgd_pa;
    pte32_t *ptb0 = (pte32_t*)ptb_kern_pa;
    pte32_t *ptb3 = (pte32_t*)ptb_stack_pa;

    memset(pgd, 0, PAGE_SZ);
    memset(ptb0, 0, PAGE_SZ);
    memset(ptb3, 0, PAGE_SZ);

    map_region_identity(ptb0, 0, 1024, PG_KRN | PG_RW);
    pg_set_entry(&pgd[0], PG_KRN | PG_RW, page_get_nr(ptb_kern_pa));

    map_region_identity(ptb3, 3072, 1024, PG_KRN | PG_RW);
    pg_set_entry(&pgd[3], PG_KRN | PG_RW, page_get_nr(ptb_stack_pa));

    return pgd_pa;
}

void setup_paging(void) {
    // Adresses physiques arbitraires pour les tables
    uint32_t u1_pgd = 0x00610000;
    uint32_t u2_pgd = 0x00620000;

    // Configuration Tâche 1
    create_base_pgd(u1_pgd, 0x611000, 0x614000);
    pte32_t *ptb1_u1 = (pte32_t*)0x612000; 
    //pte32_t *ptb_sh_u1 = (pte32_t*)0x615000;
    
    // Identity map code user (4-6MB)
    for(int i=0; i<512; i++) pg_set_entry(&ptb1_u1[i], PG_USR | PG_RW, 1024 + i);
    for(int i=512; i<1024; i++) pg_set_entry(&ptb1_u1[i], PG_KRN | PG_RW, 1024 + i);
    pg_set_entry(&((pde32_t*)u1_pgd)[1], PG_USR | PG_RW, page_get_nr(0x612000));

    // Shared & User Stack
    pg_set_entry(&((pde32_t*)u1_pgd)[2], PG_USR | PG_RW, page_get_nr(0x613000));
    pg_set_entry(&((pte32_t*)0x613000)[pt32_get_idx(0x800000)], PG_USR | PG_RW, page_get_nr(0x800000));
    
    pg_set_entry(&((pde32_t*)u1_pgd)[pd32_get_idx(VA_SHARED_U1)], PG_USR | PG_RW, page_get_nr(0x615000));
    pg_set_entry(&((pte32_t*)0x615000)[pt32_get_idx(VA_SHARED_U1)], PG_USR | PG_RW, page_get_nr(PA_SHARED_PHYS));

    task1.cr3 = u1_pgd;

    // Configuration Tâche 2 (similaire avec adresses spécifiques)
    create_base_pgd(u2_pgd, 0x621000, 0x624000);
    // ... (Répéter pour U2 avec VA_SHARED_U2 et pile 0x801000)
    task2.cr3 = u2_pgd;
}

/* --- Initialisation Système --- */

static inline void cli(void){asm volatile("cli");} 

void setup_hardware(uint32_t tick_hz) {
    cli();
    intr_init();
    pic_init();
    
    // PIT Init
    uint32_t d = 1193182 / tick_hz;
    out(0x36, 0x43);
    out(d & 0xFF, 0x40);
    out(d >> 8, 0x40);

    out(0xFE, PIC_IMR(PIC1)); // IRQ0 only
    out(0xFF, PIC_IMR(PIC2));
}

void tp() {
    setup_segmentation();
    setup_tss();
    setup_paging();
    
    // Activation Pagination
    set_cr3(task1.cr3);
    set_cr0(get_cr0() | 0x80000000);

    setup_hardware(50);

    /* --- Préparation du contexte Tâche 2 --- */
    uint32_t *kstack = (uint32_t*)&__kernel_stack_user2_end__;
    int_ctx_t *ctx = (int_ctx_t*)((uint8_t*)kstack - sizeof(int_ctx_t));
    
    memset(ctx, 0, sizeof(int_ctx_t));
    ctx->eip.raw = (uint32_t)user2;
    ctx->cs.raw  = (6 << 3) | 3;   // GDT 6, RPL 3
    ctx->ss.raw  = (7 << 3) | 3;   // GDT 7, RPL 3
    ctx->esp.raw = 0x00801000 + STACK_SZ;
    ctx->eflags.raw = 0x202;       // IF = 1
    task2.kstack_ptr = (uint32_t)ctx;

    /* --- Lancement Tâche 1 --- */
    current_task = &task1;
    system_tss.s0.esp = (uint32_t)&__kernel_stack_user1_end__;
    
    // Raz du compteur partagé
    *(volatile uint32_t*)PA_SHARED_PHYS = 0;

    uint32_t u_esp = 0x00800000 + STACK_SZ;
    uint32_t u_cs  = (4 << 3) | 3;
    uint32_t u_ss  = (5 << 3) | 3;

    debug("Système prêt. Switch vers User1...\n");

    asm volatile(
        "pushl %0; pushl %1; pushl $0x202; pushl %2; pushl %3; iret"
        : : "r"(u_ss), "r"(u_esp), "r"(u_cs), "r"(user1) : "memory"
    );
}