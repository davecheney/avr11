#include <Arduino.h>
#include <SpiRAM.h>
#include "avr11.h"
#include "cpu.h"
#include "unibus.h"
#include "rk05.h"
#include "xmem.h"

// memory as words
int *intptr = reinterpret_cast<int *>(0x2200);
// memory as bytes
char *charptr = reinterpret_cast<char *>(0x2200);

uint16_t pdp11::unibus::read8(uint32_t a) {
  if (a & 1) {
    return read16(a & ~1) >> 8;
  }
  return read16(a & ~1) & 0xFF;
}

void pdp11::unibus::write8(uint32_t a, uint16_t v) {
  if (a < 0760000) {
    xmem::setMemoryBank((a >> 15) & 0xf, false);
    charptr[(a & 0x7fff)] = v & 0xff;
    return;
  }
  if (a & 1) {
    write16(a&~1, (read16(a) & 0xFF) | (v & 0xFF) << 8);
  } else {
    write16(a&~1, (read16(a) & 0xFF00) | (v & 0xFF));
  }

}

void pdp11::unibus::write16(uint32_t a, uint16_t v) {
  if (a % 1) {
    Serial.print(F("unibus: write to odd address ")); Serial.println(a, OCT);
    trap(INTBUS);
  }
  if (a < 0760000) {
    xmem::setMemoryBank((a >> 15) & 0xf, false);
    intptr[(a & 0x7fff) >> 1] = v;
    return;
  }
  else if (a == 0777776) {
    switch (v >> 14) {
      case 0:
        switchmode(false);
        break;
      case 3:
        switchmode(true);
        break;
      default:
        Serial.println(F("invalid mode"));
        panic();
    }
    switch ((v >> 12) & 3) {
      case 0:
        prevuser = false;
        break;
      case 3:
        prevuser = true;
        break;
      default:
        Serial.println(F("invalid mode"));
        panic();
    }
    PS = v;
  }
  else if (a == 0777546) {
    LKS = v;
  }
  else if (a == 0777572) {
    SR0 = v;
  }
  else if ((a & 0777770) == 0777560) {
    cons.write16(a, v);
  }
  else if ((a & 0777700) == 0777400) {
    rkwrite16(a, v);
  }
  else if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
    mmu.write16(a, v);
  }
  else {
    Serial.print(F("unibus: write to invalid address ")); Serial.println(a, OCT);
    trap(INTBUS);
  }
}

uint16_t pdp11::unibus::read16(uint32_t a) {
  if (a & 1) {
    Serial.print(F("unibus: read from odd address ")); Serial.println(a, OCT);
    trap(INTBUS);
  }

  if (a < 0760000 ) {
    xmem::setMemoryBank((a >> 15) & 0xf, false);
    return intptr[(a & 0x7fff) >> 1];
  }

  if (a == 0777546) {
    return LKS;
  }

  if (a == 0777570) {
    return 0173030;
  }  

  if (a == 0777572) {
    return SR0;
  }

  if (a == 0777576) {
    return SR2;
  }

  if (a == 0777776) {
    return PS;
  }

  if ((a & 0777770) == 0777560) {
    return cons.read16(a);
  }

  if ((a & 0777760) == 0777400) {
    return rkread16(a);
  }

  if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
    return mmu.read16(a);
  }
  Serial.print(F("unibus: read from invalid address ")); Serial.println(a, OCT);
  trap(INTBUS);
}
