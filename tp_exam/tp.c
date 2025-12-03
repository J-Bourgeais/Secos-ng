/* GPLv2 (c) Airbus */

#include <debug.h>
#include <pagemem.h>
#include <segmem.h>
#include <intr.h>
#include <string.h>
#include <cr.h>
#include <types.h> 

/* cf correction chatty */

//intr_init pour l'IDT, pic_remap pour le contrôleur d'interruptions, pit_init pour le timer
//Weak permet de compiler même si ces fonctions ne sont pas définies
extern void intr_init(void) __attribute__((weak));
extern void pic_remap(void) __attribute__((weak));
extern void pit_init(void) __attribute__((weak));

/* Adresses physiques des zones mémoire des tâches */
#define USER1_PHYS       0x400000 //adresse physique de base pour le code de la tâche utilisateur 1
#define USER2_PHYS       0x800000 //adresse physique de base pour le code de la tâche utilisateur 2
#define USER1_STACK_PHYS 0x401000 //adresse physique pour la pile utilisateur de la tâche 1
#define USER2_STACK_PHYS 0x801000 //adresse physique pour la pile utilisateur de la tâche 2
#define KERN1_STACK_PHYS 0x402000 //adresse physique pour la pile noyau de la tâche 1
#define KERN2_STACK_PHYS 0x802000 //adresse physique pour la pile noyau de la tâche 2
#define SHARED_PHYS      0x900000 //adresse physique de la page partagée entre les deux tâches
#define SHARED_VIRT_T1   0x500000 //adresse virtuelle de la page partagée dans la tâche 1
#define SHARED_VIRT_T2   0xA00000 //adresse virtuelle de la page partagée dans la tâche 2

#define PGD1_PHYS        0xA100000 //adresse physique du page directory de la tâche 1
#define PGD2_PHYS        0xA200000 //adresse physique du page directory de la tâche 2

#define IRQ0             32 //Le numéro dans l'IDT de l'interruption IRQ0 (timer)
#define SYSCALL_INT      0x80 //Le numéro dans l'IDT de l'interruption pour les appels système

/*
Une erreur est générée. 
Ca compile avec make, mais quand je fais make qemu, ca donne ca :
ERROR:system/cpus.c:504:qemu_mutex_lock_iothread_impl: assertion failed: (!qemu)
Bail out! ERROR:system/cpus.c:504:qemu_mutex_lock_iothread_impl: assertion fail)
make: *** [../utils/rules.mk:58: qemu] Abandon (core dump créé)

--> cette erreur revient de temps en temps, mais actuellement j'ai ca : 

secos-90d71e4-6fe44f8 (c) Airbus

IDT event
 . int    #13
 . error  0x28
 . cs:eip 0x8:0x304852
 . ss:esp 0x301fe4:0x304fbc
 . eflags 0x12

- GPR
eax     : 0x28
ecx     : 0x5f
edx     : 0x0
ebx     : 0x2be40
esp     : 0x301f9c
ebp     : 0x301fe8
esi     : 0x2bfda
edi     : 0x2bfdb

Exception: General protection
#GP details: ext:0 idt:0 ti:0 index:5
cr0 = 0x11
cr4 = 0x0

-= Stack Trace =-
0x30305a
0x303020
0x8c85
0x72bf0000
fatal exception !


--> On a une GP Fault à l'adresse 0x304852, qui correspond à l'instruction "mov %ds, %eax" dans la fonction syscall_handler.
De plus, l'index est 5, ce qui correspond à l'entrée TSS dans la GDT.
Donc on peut regarder ces éléments pour comprendre pourquoi on a cette erreur.
--> Possibles causes :
- Le descripteur TSS dans la GDT est mal configuré (base, limite, type, etc.)
- Le registre TR n'est pas correctement chargé avec le sélecteur TSS
- PGD non valide (au moment de l'IRQ/Syscall) : le noyau doit toujours pouvoir accéder à son code/ses données, et au TSS
*/



/* Contexte des tâches */
typedef struct task {
    uint32_t *pgd;         // Page directory
    uint32_t *esp0;        // Pile noyau (haut de pile)
    uint32_t *stack_user;  // Pile utilisateur
    uint32_t *shared_vaddr;// Adresse virtuelle de la zone partagée
    uint32_t eip;          // Adresse de départ (user1 ou user2)
} task_t;

task_t task1, task2;
task_t *current_task;

/* Sélecteurs GDT*/
#define KRN_CS_SEL gdt_krn_seg_sel(1) //sélecteur de segment de code noyau (Ring 0)
#define KRN_DS_SEL gdt_krn_seg_sel(2) //sélecteur de segment de données noyau (Ring 0)
#define USR_CS_SEL gdt_usr_seg_sel(3) //sélecteur de segment de code utilisateur (Ring 3)
#define USR_DS_SEL gdt_usr_seg_sel(4) //sélecteur de segment de données utilisateur (Ring 3)

/* Indice TSS ; on suppose que l’entrée TSS est index 5 */
#define TSS_IDX    5
#define TSS_SEL    gdt_krn_seg_sel(TSS_IDX)

/* TSS global (pour basculer pile noyau lors des IRQ depuis ring 3) */
static tss_t TSS;

/* Helpers: mise à jour de l'adresse de la pile noyau de la tache courante dans le TSS 
--> essentiel pour gérer les interruptions depuis la ring 3*/
static inline void tss_set_kernel_stack(uint32_t esp0) {
    TSS.s0.esp = esp0;
    TSS.s0.ss  = KRN_DS_SEL;
}

/* Helper: construire un descripteur TSS dans la GDT TODO --> Voir si présents autres TP*/
static inline void tss_dsc(seg_desc_t *dsc, offset_t tss_addr) {
    memset(dsc, 0, sizeof(*dsc));
    raw32_t addr = {.raw = (uint32_t)tss_addr};
    uint32_t limit = sizeof(tss_t) - 1;

    dsc->limit_1 = (uint16_t)(limit & 0xFFFF);
    dsc->limit_2 = (uint8_t)((limit >> 16) & 0x0F);

    dsc->base_1 = addr.wlow;
    dsc->base_2 = addr._whigh.blow;
    dsc->base_3 = addr._whigh.bhigh;

    dsc->type   = SEG_DESC_SYS_TSS_AVL_32;  // 0x9
    dsc->s      = 0;                        // descripteur système
    dsc->dpl    = 0;
    dsc->p      = 1;

    dsc->avl    = 0;
    dsc->l      = 0;
    dsc->d      = 0;                        // non applicable ici
    dsc->g      = 0;                        // limite en octets
}

/* Interface utilisateur pour l'appel système */
void sys_counter(uint32_t *c) {
   asm volatile (
      "mov %0, %%esi \n"
      "int $80        \n"
      :
      : "r"(c)
      : "%esi"
   );
}

/* Code user (section .user)*/

__attribute__((section(".user")))
void user1() {
    uint32_t *counter = (uint32_t*)SHARED_VIRT_T1;
    while (1) {
        (*counter)++; //Incrémente directement la valeur du compteur dans la zone de mémoire partagée --> passe pas par le noyau
    }
}

__attribute__((section(".user")))
void user2() {
    uint32_t *counter = (uint32_t*)SHARED_VIRT_T2;
    while (1) {
        sys_counter(counter); //incrémente via le noyau grâce à un appel système
    }
}

/*  Handlers  */
//Déclarations des Interrupt Service Routines (ISR) qui sont les points d'entrée dans le noyau pour les interruptions matérielles (IRQ0) et logicielles (SYSCALL_INT
void irq0_isr(); 
void syscall_isr();

//Les mêmes, mais en C, recoivent le contexte de l'interruption en paramètre
void irq0_handler(int_ctx_t *ctx);
void syscall_handler(int_ctx_t *ctx);

//code de sauvegarde du contexte du CPU pour l'interruption du timer (IRQ0)
//naked : rajoute rien au code --> pas de prologue/épilogue
__attribute__((naked)) void irq0_isr() {
    asm volatile (
        "cli               \n" //Désactive les aurtes interruptions pendant le traitement
        "pushl %ds         \n" //sauvegarde les registres et segments
        "pushl %es         \n"
        "pushl %fs         \n"
        "pushl %gs         \n"
        "pusha             \n"
        "movl %esp, %eax   \n"
        "pushl %eax        \n"
        "call irq0_handler \n" //call le handler
        "addl $4, %esp     \n"
        "popa              \n" //restaure les registres et segments
        "popl %gs          \n"
        "popl %fs          \n"
        "popl %es          \n"
        "popl %ds          \n"
        "sti               \n" //réactive les interruptions
        "iret              \n" //retour de l'interruption
    );
}

//Le code de sauvegarde similaire pour l'appel système (int 0x80)
__attribute__((naked)) void syscall_isr() {
    asm volatile (
        "cli                 \n"
        "pushl %ds           \n"
        "pushl %es           \n"
        "pushl %fs           \n"
        "pushl %gs           \n"
        "pusha               \n"
        "movl %esp, %eax     \n"
        "pushl %eax          \n"
        "call syscall_handler\n"
        "addl $4, %esp       \n"
        "popa                \n"
        "popl %gs            \n"
        "popl %fs            \n"
        "popl %es            \n"
        "popl %ds            \n"
        "sti                 \n"
        "iret                \n"
    );
}

//Pour traiter l'appel système de la tache user2
void syscall_handler(int_ctx_t *ctx) {
    uint32_t *counter = (uint32_t*)ctx->gpr.esi.raw;
    uint32_t val = *counter;
    val++;
    *counter = val;
    debug("Counter = %u\n", val); //Est sensé l'afficher !!!
}

/*  Mapping  */

#define PT1_CODE_PHYS   (PGD1_PHYS + 0x001000)
#define PT1_SHRD_PHYS   (PGD1_PHYS + 0x002000)
#define PT1_KERN_BASE   (PGD1_PHYS + 0x003000)

#define PT2_CODE_PHYS   (PGD2_PHYS + 0x001000)
#define PT2_SHRD_PHYS   (PGD2_PHYS + 0x002000)
#define PT2_KERN_BASE   (PGD2_PHYS + 0x003000)

/* Mappe une page de 4 MiB en identity mapping en user --> Crée une zone d'adressage utilisateur de 4 Mo où l'adresse virtuelle est la même que l'adresse physique */
//TODO --> Voir les autres TP si pareil
static void map_4MB_identity_user(pde32_t *pgd, uint32_t virt_base, uint32_t pt_phys) {
    pte32_t *pt = (pte32_t*)pt_phys;
    __clear_page(pt);
    pg_set_entry(&pgd[pd32_get_idx(virt_base)], PG_USR | PG_RW, page_get_nr(pt_phys));
    uint32_t base4m = virt_base & ~((1u << PG_4M_SHIFT) - 1);
    for (int i = 0; i < 1024; i++) {
        uint32_t phys = base4m + (i << PAGE_SHIFT);
        pg_set_entry(&pt[i], PG_USR | PG_RW, page_get_nr(phys));
    }
}

/* Mappe une page partagée en user --> Crée une seule page utilisateur (4 KiB) pour la mémoire partagée, en la rendant accessible à une adresse virtuelle spécifique */
static void map_shared_page_user(pde32_t *pgd, uint32_t virt, uint32_t pt_phys, uint32_t shared_phys) {
    pte32_t *pt = (pte32_t*)pt_phys;
    __clear_page(pt);
    pg_set_entry(&pgd[pd32_get_idx(virt)], PG_USR | PG_RW, page_get_nr(pt_phys));
    pg_set_entry(&pt[pt32_get_idx(virt)], PG_USR | PG_RW, page_get_nr(shared_phys));
}

/* Identity map kernel (U/S=0) pour au moins 16 MiB --> Crée une grande zone (16 Mo) où l'adresse virtuelle est la même que l'adresse physique, mais seul le noyau peut y accéder*/
static void map_16MB_identity_kernel(pde32_t *pgd, uint32_t pt_phys_base) {
    for (int pd = 0; pd < 4; pd++) {
        uint32_t pt_phys = pt_phys_base + pd * PAGE_SIZE;
        pte32_t *pt = (pte32_t*)pt_phys;
        __clear_page(pt);
        pg_set_entry(&pgd[pd], PG_RW, page_get_nr(pt_phys)); /* U/S=0 */
        for (int i = 0; i < 1024; i++) {
            uint32_t phys = (pd << 22) | (i << 12);
            pg_set_entry(&pt[i], PG_RW, page_get_nr(phys));   /* U/S=0 */
        }
    }
}

/*  Contexte initial (cadre iret)  */
//Prépare la pile noyau de la tâche pour faire croire au CPU qu'il revient d'une interruption. 
//permet de démarrer la tâche utilisateur en mode Ring 3.
static void build_initial_iret_frame(task_t *t) {
    uint32_t *sp = t->esp0;
    *(--sp) = USR_DS_SEL;                 /* SS user */
    *(--sp) = (uint32_t)t->stack_user;    /* ESP user (haut de pile) */
    *(--sp) = 0x202;                      /* EFLAGS (IF=1) --> interruption activée*/
    *(--sp) = USR_CS_SEL;                 /* CS user */
    *(--sp) = t->eip;                     /* EIP user */
    for (int i = 0; i < 8; i++) *(--sp) = 0; //place des 0 pour simuler la sauvegarde des registres généraux
    t->esp0 = sp;
}

/*  Initialisation d'une tâche  */
// Fonction d'initialisation complète de la structure task_t et de son espace d'adressage virtuel.
void init_task(task_t *task,
               uint32_t pgd_phys,
               uint32_t code_base_virt, uint32_t code_pt_phys,
               uint32_t user_stack_phys,
               uint32_t kern_stack_phys,
               uint32_t shared_virt, uint32_t shared_pt_phys,
               uint32_t eip,
               uint32_t kern_pt_base) {

    task->pgd        = (uint32_t*)pgd_phys;
    task->esp0       = (uint32_t*)(kern_stack_phys + PAGE_SIZE);
    task->stack_user = (uint32_t*)(user_stack_phys + PAGE_SIZE);
    task->shared_vaddr = (uint32_t*)shared_virt;
    task->eip        = eip;

    __clear_page((void*)pgd_phys);

    map_16MB_identity_kernel((pde32_t*)pgd_phys, kern_pt_base);
    map_4MB_identity_user((pde32_t*)pgd_phys, code_base_virt, code_pt_phys);
    map_shared_page_user((pde32_t*)pgd_phys, shared_virt, shared_pt_phys, SHARED_PHYS);

    build_initial_iret_frame(task);
}

/*  Scheduler simple */
//La fonction qui s'exécute à chaque tic-tac de l'horloge pour changer de programme
void irq0_handler(int_ctx_t *ctx) {
    int from_user = ((ctx->cs.raw & 3) == 3); //vérifie si l'interruption vient du mode utilisateur (Ring 3)

    setptr(current_task->esp0, (offset_t)ctx); //sauvegarde le contexte actuel de la tâche courante dans sa pile noyau

    task_t *next = (current_task == &task1) ? &task2 : &task1; //si une tache, on prend l'autre
    current_task = next;

    set_cr3((uint32_t)next->pgd); //change l'espace d'adressage virtuel pour celui de la tâche suivante
    tss_set_kernel_stack((uint32_t)next->esp0); //met à jour la pile noyau dans le TSS pour la tâche suivante

    if (!from_user) {
        asm volatile ("mov %0, %%esp" :: "r"(next->esp0)); 
    }
}

/*  Activation des interruptions  */
static inline void sti(){
    asm volatile ("sti");
}

/* Entrée explicite en ring 3: charge DS/ES/FS/GS en user avant iret */
//initialise les registres de segments et empile manuellement un cadre iret (Stack Frame) pour forcer le passage initial au mode Ring 3 et le démarrage de la première tache utilisateur
static void enter_userland_initial(task_t *t) {
    asm volatile (
        "mov %0, %%ds      \n"
        "mov %0, %%es      \n"
        "mov %0, %%fs      \n"
        "mov %0, %%gs      \n"
        "push %0           \n"  /* SS = USR_DS_SEL */
        "push %1           \n"  /* ESP = t->stack_user (top) */
        "pushf             \n"  /* EFLAGS */
        "pop %%eax         \n"
        "or $0x200, %%eax  \n"  /* IF=1 */
        "push %%eax        \n"
        "push %2           \n"  /* CS = USR_CS_SEL */
        "push %3           \n"  /* EIP = t->eip */
        "iret              \n"
        :
        : "r"(USR_DS_SEL),
          "r"(t->stack_user),
          "r"(USR_CS_SEL),
          "r"(t->eip)
        : "eax", "memory"
    );
}

void tp() {
    // TODO

    //Initialise à zéro la page mémoire physique partagée
    memset((void*)SHARED_PHYS, 0, PAGE_SIZE);

    init_task(&task1,
              PGD1_PHYS,
              USER1_PHYS, PT1_CODE_PHYS,
              USER1_STACK_PHYS,
              KERN1_STACK_PHYS,
              SHARED_VIRT_T1, PT1_SHRD_PHYS,
              (uint32_t)&user1,
              PT1_KERN_BASE);

    init_task(&task2,
              PGD2_PHYS,
              USER2_PHYS, PT2_CODE_PHYS,
              USER2_STACK_PHYS,
              KERN2_STACK_PHYS,
              SHARED_VIRT_T2, PT2_SHRD_PHYS,
              (uint32_t)&user2,
              PT2_KERN_BASE);

    /* Init TSS mémoire + pile noyau initiale */
    memset(&TSS, 0, sizeof(TSS));
    TSS.s0.ss  = KRN_DS_SEL;
    TSS.s0.esp = (uint32_t)task1.esp0;

    /* Récupérer la GDT et y déposer l’entrée TSS, puis charger TR */
    gdt_reg_t gdtr;
    get_gdtr(gdtr);
    seg_desc_t *gdt = (seg_desc_t*)gdtr.addr;
    tss_dsc(&gdt[TSS_IDX], (offset_t)&TSS); 

        //TEST RESOLUTION
    uint16_t new_limit = ((TSS_IDX + 1) * sizeof(seg_desc_t)) - 1;
    if (gdtr.limit < new_limit) {
        gdtr.limit = new_limit;
    }
    set_gdtr(gdtr);

    set_tr(TSS_SEL); 
    //TEST RESOLUTION --> asm volatile("ltr %%ax" :: "a" (TSS_SEL)); --> essayer de le faire directement

    /* IDT: installer gates syscall 0x80 et IRQ0 avec selector = code noyau, type, présence, DPL */
    idt_reg_t idtr;
    get_idtr(idtr);

    int_desc_t *syscall_dsc = &idtr.desc[SYSCALL_INT];
    //Configure le point d'entrée 0x80 (syscall) dans la table d'interruption (IDT) et le rend accessible depuis Ring 3
    build_int_desc(syscall_dsc, KRN_CS_SEL, (offset_t)syscall_isr);
    syscall_dsc->dpl = 3; /* Accessible depuis ring 3 */

    int_desc_t *irq0_dsc = &idtr.desc[IRQ0];
    //Configure le point d'entrée 32 (IRQ0) pour le timer, accessible seulement depuis Ring 0
    build_int_desc(irq0_dsc, KRN_CS_SEL, (offset_t)irq0_isr);
    irq0_dsc->dpl = 0;

    /* Charger l’espace d’adressage de la tâche 1, activer paging */
    //Charge le plan d'adressage de Tâche 1 dans le CPU et active la pagination (paging) en modifiant le registre CR0
    set_cr3((uint32_t)task1.pgd);
    uint32_t cr0 = get_cr0();
    set_cr0(cr0 | 0x80000000);

    /* Initialisations des interruptions si disponibles */
    //Démarre les interruptions pour que le timer puisse fonctionner
    if (intr_init) intr_init();
    if (pic_remap) pic_remap();
    if (pit_init)  pit_init();

    /* Entrée explicite en ring 3, segments utilisateur valides avant iret */
    //Lance l'exécution de Tâche 1 en mode utilisateur (Ring 3)
    enter_userland_initial(&task1);

    /* On ne revient pas ici; boucle de sécurité si iret échoue */
    while (1) { }
}
