/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>
#include <segmem.h>
#include <intr.h>
#include <string.h>
#include <cr.h>

#define USER1_PHYS 0x400000
#define USER2_PHYS 0x800000
#define USER1_STACK_PHYS 0x401000
#define USER2_STACK_PHYS 0x801000
#define KERN1_STACK_PHYS 0x402000
#define KERN2_STACK_PHYS 0x802000
#define SHARED_PHYS 0x900000
#define SHARED_VIRT_T1 0x500000
#define SHARED_VIRT_T2 0xA00000

#define PGD1_PHYS 0xA100000
#define PGD2_PHYS 0xA200000

#define IRQ0 32
#define SYSCALL_INT 0x80


/*
Une erreur est générée. 
Ca compile avec make, mais quand je fais make qemu, ca donne ca :
ERROR:system/cpus.c:504:qemu_mutex_lock_iothread_impl: assertion failed: (!qemu)
Bail out! ERROR:system/cpus.c:504:qemu_mutex_lock_iothread_impl: assertion fail)
make: *** [../utils/rules.mk:58: qemu] Abandon (core dump créé)
*/




//Besoin ou pas ? Selon les fichiers fournis
typedef struct task {
    uint32_t *pgd;         // Page directory
    uint32_t *esp0;        // Pile noyau (haut de pile)
    uint32_t *stack_user;  // Pile utilisateur
    uint32_t *shared_vaddr;// Adresse virtuelle de la zone partagée
    uint32_t eip;          // Adresse de départ (user1 ou user2)
} task_t;

//Définir les taches 1 et 2
task_t task1, task2;
task_t *current_task;

//Interface utilisateur pour l'appel système
void sys_counter(uint32_t *c) { //L'argument est une adresse virtuelle ring 3
   asm volatile (
	  "mov %0, %%esi \n" //mettre l'argument dans ESI
	  "int $80        \n" //appel système 80
	  :
	  : "r"(c)
	  : "%esi"
   );
}


__attribute__((section(".user")))
void user1() {
    uint32_t *counter = (uint32_t*)SHARED_VIRT_T1;
    while (1) {
        (*counter)++;
    }
}

__attribute__((section(".user")))
void user2() {
    uint32_t *counter = (uint32_t*)SHARED_VIRT_T2;
    while (1) {
        sys_counter(counter);
    }
}

//gestion de changement de tache
void switch_to(task_t *task) {
    set_cr3((uint32_t)task->pgd);
    asm volatile (
        "mov %0, %%esp\n"
        "popa; add $8, %%esp\n"
        "iret\n"
        :
        : "r"(task->esp0)
    );
}

void save_context(task_t *task){
    asm volatile ("mov %%esp, %0" : "=r"(task->esp0));
}

//Gestionnaire d'interruption
void irq0_handler() {
	//Definir la fonction save_context selon ce dont on a besoin dans ce tp
    save_context(current_task); // sauvegarde esp0
    current_task = (current_task == &task1) ? &task2 : &task1;
    switch_to(current_task);    // restaure esp0 et fait iret
}

void irq0_isr() {
    asm volatile (
        "cli; call irq0_handler\n"
    );
}




//interface noyau pour l'appel système
void syscall_handler(int_ctx_t *ctx) {
    uint32_t *counter = (uint32_t*)ctx->gpr.esi.raw;
    if (*counter >= 0x400000 && *counter < 0xBFFFFFFF) { //&counter ou (void*)counter ??
		(*counter)++;
        debug("Counter = %d\n", *counter);
    } else {
        debug("Invalid pointer: %p\n", counter);
    }
}

//Voir a quoi ca sert
void syscall_isr() {
    asm volatile (
        "leave; pusha\n"
        "mov %esp, %eax\n"
        "call syscall_handler\n"
        "popa; iret\n"
    );
}

void map_identity(pde32_t *pgd, uint32_t phys_start, uint32_t virt_start) {
    pte32_t *ptb = (pte32_t*)(phys_start + 0x1000);
    pgd[pd32_get_idx(virt_start)].p = 1;
    pgd[pd32_get_idx(virt_start)].rw = 1;
    pgd[pd32_get_idx(virt_start)].addr = ((uint32_t)ptb) >> 12;

    for (int i = 0; i < 1024; i++) {
        ptb[i].p = 1;
        ptb[i].rw = 1;
        ptb[i].addr = i;
    }
}

void map_shared(pde32_t *pgd, uint32_t virt, uint32_t phys) {
    pte32_t *ptb = (pte32_t*)(phys + 0x1000);
    pgd[pd32_get_idx(virt)].p = 1;
    pgd[pd32_get_idx(virt)].rw = 1;
    pgd[pd32_get_idx(virt)].addr = ((uint32_t)ptb) >> 12;

    ptb[pt32_get_idx(virt)].p = 1;
    ptb[pt32_get_idx(virt)].rw = 1;
    ptb[pt32_get_idx(virt)].addr = phys >> 12;
}

void init_task(task_t *task, uint32_t pgd_phys, uint32_t code_phys, uint32_t user_stack_phys, uint32_t kern_stack_phys, uint32_t shared_virt, uint32_t eip) {
    task->pgd = (uint32_t*)pgd_phys;
    task->esp0 = (uint32_t*)(kern_stack_phys + PAGE_SIZE);
    task->stack_user = (uint32_t*)(user_stack_phys + PAGE_SIZE);
    task->shared_vaddr = (uint32_t*)shared_virt;
    task->eip = eip;

    map_identity((pde32_t*)task->pgd, pgd_phys, code_phys);
    map_shared((pde32_t*)task->pgd, shared_virt, SHARED_PHYS);
	uint32_t *stack = task->esp0;
    *(--stack) = gdt_usr_seg_sel(4); // SS
    *(--stack) = (uint32_t)task->stack_user;
    *(--stack) = 0x202;              // EFLAGS
    *(--stack) = gdt_usr_seg_sel(3); // CS
    *(--stack) = task->eip;
    for (int i = 0; i < 8; i++) *(--stack) = 0;
    task->esp0 = stack;
}

static inline void sti(){
    asm volatile ("sti");
}


void tp() {
	// TODO

	memset((void*)SHARED_PHYS, 0, PAGE_SIZE);

    init_task(&task1, PGD1_PHYS, USER1_PHYS, USER1_STACK_PHYS, KERN1_STACK_PHYS, SHARED_VIRT_T1, (uint32_t)&user1);
    init_task(&task2, PGD2_PHYS, USER2_PHYS, USER2_STACK_PHYS, KERN2_STACK_PHYS, SHARED_VIRT_T2, (uint32_t)&user2);

    idt_reg_t idtr;
    get_idtr(idtr);
    int_desc_t *syscall_dsc = &idtr.desc[SYSCALL_INT];
    syscall_dsc->offset_1 = (uint16_t)((uint32_t)syscall_isr);
    syscall_dsc->offset_2 = (uint16_t)(((uint32_t)syscall_isr) >> 16);
    syscall_dsc->p = 1;
    syscall_dsc->type = 0xE;
    syscall_dsc->dpl = 3;

	int_desc_t *irq0_dsc = &idtr.desc[IRQ0];
    irq0_dsc->offset_1 = (uint16_t)((uint32_t)irq0_isr);
    irq0_dsc->offset_2 = (uint16_t)(((uint32_t)irq0_isr) >> 16);
    irq0_dsc->p = 1;
    irq0_dsc->type = 0xE;
    irq0_dsc->dpl = 0;

    set_cr3((uint32_t)task1.pgd);
    uint32_t cr0 = get_cr0();
    set_cr0(cr0 | 0x80000000);

    sti(); //set interrupt flag --> a definir
    while (1);
	
}
