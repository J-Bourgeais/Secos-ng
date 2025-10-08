

/* GPLv2 (c) Airbus */
#include <debug.h>
#include <info.h>
#include <types.h>

extern info_t   *info;
extern uint32_t __kernel_start__;
extern uint32_t __kernel_end__;

void tp() {
   debug("kernel mem [0x%p - 0x%p]\n", &__kernel_start__, &__kernel_end__);
   debug("MBI flags 0x%x\n", info->mbi->flags);

   /* adresse et fin de la table mmap fournies par le grub_mbi.h */
   uint32_t mmap_addr = (uint32_t)info->mbi->mmap_addr;
   uint32_t mmap_end  = mmap_addr + (uint32_t)info->mbi->mmap_length;

   /* castage pour avoir toutes les infos → ligne 243*/
   multiboot_memory_map_t* entry = (multiboot_memory_map_t*)mmap_addr;

   while ((uint32_t)entry < mmap_end) { //jusqu’à la fin
      if (entry->size == 0) {
         break;
      }

      uint64_t start = entry->addr;
      uint64_t len   = entry->len;
      uint64_t last  = (len > 0) ? (start + len - 1) : (start); 
      uint32_t type  = entry->type;

//on peut pas caster → j’avoue j’ai pas tout compris a comment ca a été defini struct

      const char *type_str;
      switch (type) {
         case MULTIBOOT_MEMORY_AVAILABLE:
            type_str = "MULTIBOOT_MEMORY_AVAILABLE";
            break;
         case MULTIBOOT_MEMORY_RESERVED:
            type_str = "MULTIBOOT_MEMORY_RESERVED";
            break;
         case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
            type_str = "MULTIBOOT_MEMORY_ACPI_RECLAIMABLE";
            break;
         case MULTIBOOT_MEMORY_NVS:
            type_str = "MULTIBOOT_MEMORY_NVS";
            break;
         default:
            type_str = "MULTIBOOT_MEMORY_UNKNOWN";
            break;
      }

      debug("[0x%llx - 0x%llx] %s\n",
            (unsigned long long)start,
            (unsigned long long)last,
            type_str);

      entry++; 
      /* ou avancer à l'entrée suivante : size (payload) + sizeof(size) (4 bytes) */
      //entry = (multiboot_memory_map_t*)
      // ((uint32_t)entry + (uint32_t)entry->size + sizeof(entry->size));
   }
}
