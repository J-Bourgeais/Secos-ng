/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>
#include <segmem.h>
#include <intr.h>
#include <string.h>
#include <cr.h>
#include <types.h> 

extern void pic_remap(void) __attribute__((weak));
extern void pit_init(void) __attribute__((weak));

#define USER1_STACK_PHYS 0x401000 
#define USER2_STACK_PHYS 0x801000 
#define KERN1_STACK_PHYS 0x402000 
#define KERN2_STACK_PHYS 0x802000 
#define SHARED_PHYS      0x900000 
#define SHARED_VIRT_T1   0x500000 
#define SHARED_VIRT_T2   0xA00000 

#define KRN_CS_SEL 0x08
#define KRN_DS_SEL 0x10
#define USR_CS_SEL 0x1B
#define USR_DS_SEL 0x23
#define TSS_SEL    0x28
#define TSS_IDX    5

/* Tables statiques dans le binaire pour garantir leur existence physique */
static uint32_t my_pgd1[1024] __attribute__((aligned(4096)));
static uint32_t my_pgd2[1024] __attribute__((aligned(4096)));
static uint32_t my_pt_low[1024] __attribute__((aligned(4096)));   /* 0 -> 4MB */
static uint32_t my_pt_high[1024] __attribute__((aligned(4096)));  /* 4 -> 8MB (Contient vos piles !) */
static uint32_t my_pt_shared[1024] __attribute__((aligned(4096)));

typedef struct task {
    uint32_t *pgd;         
    uint32_t *esp0;       
    uint32_t eip;          
} task_t;

task_t task1, task2;
task_t *current_task;
static tss_t TSS;

void irq0_isr(); 
void syscall_isr();

void irq0_handler(void *ctx) {
    current_task->esp0 = (uint32_t*)ctx; 
    current_task = (current_task == &task1) ? &task2 : &task1;
    set_cr3((uint32_t)(uint32_t)current_task->pgd);
    TSS.s0.esp = (uint32_t)(uint32_t)current_task->esp0 + sizeof(int_ctx_t);
}

void syscall_handler(int_ctx_t *ctx) {
    uint32_t *counter = (uint32_t*)ctx->gpr.esi.raw;
    if (counter) (*counter)++;
}

__attribute__((naked)) void irq0_isr() {
    asm volatile ("pushl %ds; pushl %es; pushl %fs; pushl %gs; pusha;"
                  "mov $0x10, %ax; mov %ax, %ds; mov %ax, %es;"
                  "push %esp; call irq0_handler; add $4, %esp;"
                  "popa; popl %gs; popl %fs; popl %es; popl %ds; iret;");
}

__attribute__((naked)) void syscall_isr() {
    asm volatile ("pushl %ds; pushl %es; pushl %fs; pushl %gs; pusha;"
                  "mov $0x10, %ax; mov %ax, %ds; mov %ax, %es;"
                  "push %esp; call syscall_handler; add $4, %esp;"
                  "popa; popl %gs; popl %fs; popl %es; popl %ds; iret;");
}

__attribute__((section(".user1"))) void user1() {
    uint32_t *c = (uint32_t*)SHARED_VIRT_T1;
    while(1) { (*c)++; }
}

__attribute__((section(".user2"))) void user2() {
    uint32_t *c = (uint32_t*)SHARED_VIRT_T2;
    while(1) { asm volatile("mov %0, %%esi; int $0x80"::"r"(c):"esi"); }
}

void tp() {
    asm volatile("cli");
    debug("TP Start - Kern @ 0x%x\n", (uint32_t)tp);

    /* 1. Reset des tables */
    memset(my_pgd1, 0, 4096); memset(my_pgd2, 0, 4096);
    memset(my_pt_low, 0, 4096); memset(my_pt_high, 0, 4096); memset(my_pt_shared, 0, 4096);

    /* 2. Identity Mapping (0 -> 8MB) */
    /* On mappe les deux premiers blocs de 4Mo du répertoire de pages */
    for (int i = 0; i < 1024; i++) {
        my_pt_low[i] = (i << 12) | 0x7;               /* Zone 0 à 4 Mo */
        my_pt_high[i] = (0x400000 + (i << 12)) | 0x7; /* Zone 4 à 8 Mo (Piles !) */
    }
    my_pgd1[0] = ((uint32_t)(uint32_t)my_pt_low) | 0x7;
    my_pgd1[1] = ((uint32_t)(uint32_t)my_pt_high) | 0x7; /* Liaison index 1 */
    my_pgd2[0] = ((uint32_t)(uint32_t)my_pt_low) | 0x7;
    my_pgd2[1] = ((uint32_t)(uint32_t)my_pt_high) | 0x7; /* Liaison index 1 */

    /* 3. Page Partagée */
    my_pt_shared[pt32_get_idx(SHARED_VIRT_T1)] = SHARED_PHYS | 0x7;
    my_pt_shared[pt32_get_idx(SHARED_VIRT_T2)] = SHARED_PHYS | 0x7;
    my_pgd1[pd32_get_idx(SHARED_VIRT_T1)] = ((uint32_t)(uint32_t)my_pt_shared) | 0x7;
    my_pgd2[pd32_get_idx(SHARED_VIRT_T2)] = ((uint32_t)(uint32_t)my_pt_shared) | 0x7;

    /* 4. Tâches */
    task1.pgd = my_pgd1;
    task1.eip = (uint32_t)user1;
    task1.esp0 = (uint32_t*)(KERN1_STACK_PHYS + 4096);
    uint32_t *sp = (uint32_t*)task1.esp0;
    *(--sp) = USR_DS_SEL; *(--sp) = USER1_STACK_PHYS + 4092;
    *(--sp) = 0x202; *(--sp) = USR_CS_SEL; *(--sp) = task1.eip;
    for(int i=0; i<8; i++) *(--sp) = 0;
    task1.esp0 = sp;

    /* 5. GDT / TSS */
    gdt_reg_t gdtr; get_gdtr(gdtr);
    seg_desc_t *gdt = (seg_desc_t*)gdtr.addr;
    gdtr.limit = (TSS_IDX + 1) * 8 - 1;
    set_gdtr(gdtr);

    raw32_t tss_p = {.raw = (uint32_t)&TSS};
    gdt[TSS_IDX].limit_1 = (sizeof(tss_t) - 1) & 0xFFFF;
    gdt[TSS_IDX].base_1 = tss_p.wlow; 
    gdt[TSS_IDX].base_2 = tss_p._whigh.blow; 
    gdt[TSS_IDX].base_3 = tss_p._whigh.bhigh;
    gdt[TSS_IDX].type = 0x9; gdt[TSS_IDX].p = 1; gdt[TSS_IDX].s = 0;

    TSS.s0.ss = KRN_DS_SEL;
    TSS.s0.esp = KERN1_STACK_PHYS + 4096;
    set_tr(TSS_SEL);

    /* 6. IDT */
    idt_reg_t idtr; get_idtr(idtr);
    build_int_desc(&idtr.desc[0x80], KRN_CS_SEL, (offset_t)syscall_isr);
    idtr.desc[0x80].dpl = 3;
    build_int_desc(&idtr.desc[32], KRN_CS_SEL, (offset_t)irq0_isr);

    current_task = &task1;
    if (pic_remap) pic_remap();
    if (pit_init) pit_init();

    /* 7. Activation */
    set_cr3((uint32_t)(uint32_t)my_pgd1);
    set_cr0(get_cr0() | 0x80000000);
    asm volatile("jmp 1f\n1:");

    debug("Paging OK - Starting IRET\n");

    asm volatile("mov %0, %%esp; popa; pop %%gs; pop %%fs; pop %%es; pop %%ds; iret"::"r"(task1.esp0));
}