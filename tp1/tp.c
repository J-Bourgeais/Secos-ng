/* GPLv2 (c) Airbus */
#include <debug.h>
#include <segmem.h>
#include <string.h>

void userland() {
   asm volatile ("mov %eax, %cr0");
}

void print_gdt_content(gdt_reg_t gdtr_ptr) {
    seg_desc_t* gdt_ptr;
    gdt_ptr = (seg_desc_t*)(gdtr_ptr.addr);
    int i=0;
    while ((uint32_t)gdt_ptr < ((gdtr_ptr.addr) + gdtr_ptr.limit)) {
        uint32_t start = gdt_ptr->base_3<<24 | gdt_ptr->base_2<<16 | gdt_ptr->base_1;
        uint32_t end;
        if (gdt_ptr->g) {
            end = start + ( (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1) <<12) + 4095;
        } else {
            end = start + (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1);
        }
        debug("%d ", i);
        debug("[0x%x ", start);
        debug("- 0x%x] ", end);
        debug("seg_t: 0x%x ", gdt_ptr->type);
        debug("desc_t: %d ", gdt_ptr->s);
        debug("priv: %d ", gdt_ptr->dpl);
        debug("present: %d ", gdt_ptr->p);
        debug("avl: %d ", gdt_ptr->avl);
        debug("longmode: %d ", gdt_ptr->l);
        debug("default: %d ", gdt_ptr->d);
        debug("gran: %d ", gdt_ptr->g);
        debug("\n");
        gdt_ptr++;
        i++;
    }
}

 void set_gdt_entry(seg_desc_t* desc,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t type,
                   uint8_t s,
                   uint8_t dpl,
                   uint8_t p,
                   uint8_t avl,
                   uint8_t l,
                   uint8_t d,
                   uint8_t g)
    {
      desc->limit_1 = limit & 0xFFFF; //pour verifier que seuls les bons bits sont utilisés : un OR
      desc->base_1 = base & 0xFFFF;
      desc->base_2 = (base >> 16) & 0xFF;
      desc->type = type & 0xF;
      desc->s = s & 0x1;
      desc->dpl = dpl & 0x3;
      desc->p = p & 0x1;
      desc->limit_2 = (limit >> 16) & 0xF;
      desc->avl = avl & 0x1;
      desc->l = l & 0x1;
      desc->d = d & 0x1;
      desc->g = g & 0x1;
      desc->base_3 = (base >> 24) & 0xFF;
    }


//Pour Q6

#define KERNEL_CODE_SEG  (1 << 3) //index 1
#define KERNEL_DATA_SEG  (2 << 3) //index 2
void reload_segments()
{
  uint16_t data = 2<<3;
    set_ss(data);
    set_es(data);
    set_fs(data);
    set_ds(data);
    set_gs(data);

    // Far jump pour recharger cs avec le nouveau segment code
    // farjump(KERNEL_CODE_SEG);
    
}





void tp() {
	// TODO
  gdt_reg_t gdtr;
  get_gdtr(gdtr); //asm volatile ("sgdt %0" : "=m"(gdtr));
  print_gdt_content(gdtr);


  
  uint16_t ds = get_ds();
    uint16_t ss = get_ss();
    uint16_t es = get_es();
    uint16_t fs = get_fs();
    uint16_t gs = get_gs();
    uint16_t cs = get_cs();

    debug("-----------");

    
    

    debug("DS = 0x%x (index = %d)\n", ds, ds >> 3);
    debug("SS = 0x%x (index = %d)\n", ss, ss >> 3);
    debug("ES = 0x%x (index = %d)\n", es, es >> 3);
    debug("FS = 0x%x (index = %d)\n", fs, fs >> 3);
    debug("GS = 0x%x (index = %d)\n", gs, gs >> 3);
    debug("CS = 0x%x (index = %d)\n", cs, cs >> 3);

    debug("-----------");

    __attribute__((aligned(8))) seg_desc_t *gdt= (seg_desc_t*) 0x100000;   // 3 descripteurs : NULL, code, data
    //0x100000 car adresse available
    
    //gdt_reg_t gdtr;
   


   

    //init avec les segments qu'on veut
    //reload tout
    // 0) NULL descriptor (toujours à zéro)
    memset(&gdt[0], 0, sizeof(seg_desc_t));
    //1) Code
    set_gdt_entry(&gdt[1],
                  0x0,          // base flat = 0
                  0xFFFFF,      // limite = 4GB (avec granularité)
                  0xB,          // type code execute/read: voir code-and-data-segment types
                  1,            // s = code/data: 1 pour code et 0 pour data
                  0,            // dpl = ring 0
                  1,            // p = présent
                  0,            // avl
                  0,            // l = 64-bit mode désactivé
                  1,            // d = 32 bits segment
                  1);           // g = granularité 4Ko

    //2) Data

    set_gdt_entry(&gdt[2], 0x0, 0xFFFFF, 0x3, 1, 0, 1, 0, 0, 1, 1);
    //0x2 type data read/write

    debug("\n");
    //q6
    gdtr.limit = sizeof(seg_desc_t) * 3 - 1;
    gdtr.addr = (offset_t)gdt;
    printf("limit = 0x%08x, base = 0x%08lx\n", gdtr.limit, gdtr.addr);
    set_gdtr(gdtr);
    
    //mettre à jour les sélecteurs de segment (cs/ss/ds/...) afin qu'ils pointent vers les descripteurs précédemment définis.
    // reload_segments();
    uint16_t data = 0x10;
    set_fs(data);
    //asm volatile ("mov $0x10, %%ax\nmov %%ax, %%ds\n":::"ax");

    set_es(data);
    set_fs(data);
    set_ds(data);
    set_gs(data);
    //print_gdt_content(gdtr);

    //test pour cs
     uint16_t code = 0x8;
    farjump(code);
   
    print_gdt_content(gdtr);

    while(1);
    



}
