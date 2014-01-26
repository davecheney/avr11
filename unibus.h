#include <SpiRAM.h>
#include "cons.h"
#include "mmu.h"
#include "SD.h"

namespace pdp11 {

class unibus {
  public:
    pdp11::cons cons;
    pdp11::mmu mmu;

    uint16_t read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    void write8(uint32_t a, uint16_t v);
    void write16(uint32_t a, uint16_t v);
};

};

extern pdp11::unibus unibus;
