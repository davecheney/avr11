extern pdp11::mmu mmu;

uint16_t physread8(uint32_t addr);
uint16_t physread16(uint32_t addr);
void physwrite16(uint32_t a, uint16_t v);
void physwrite8(uint32_t a, uint16_t v);
