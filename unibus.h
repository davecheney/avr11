#include "cons.h"
#include "mmu.h"

namespace pdp11 {

class unibus {

  public:
    pdp11::cons cons;

  
    uint16_t read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    void write8(uint32_t a, uint16_t v);
    void write16(uint32_t a, uint16_t v);
};

};
    pdp11::mmu mmu;

extern pdp11::unibus unibus;
