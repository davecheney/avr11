#include <Arduino.h>
#include <SpiRAM.h>
#include <SdFat.h>
#include "avr11.h"
#include "cpu.h"
#include "cons.h"
#include "mmu.h"
#include "unibus.h"
#include "rk05.h"
#include "xmem.h"

namespace unibus {

// memory as words
int *intptr = reinterpret_cast<int *>(0x2200);
// memory as bytes
char *charptr = reinterpret_cast<char *>(0x2200);

uint16_t read8(const uint32_t a) {
  if (a & 1) {
    return read16(a & ~1) >> 8;
  }
  return read16(a & ~1) & 0xFF;
}

void write8(const uint32_t a, const uint16_t v) {
  if (a < 0760000) {
    char * aa = (char *)&a;
    uint8_t bank = ((aa[2] & 3)<<2) | (((aa)[1] & (1<<7))>>7);
    xmem::setMemoryBank(bank, false);
    charptr[(a & 0x7fff)] = v & 0xff;
    return;
  }
  if (a & 1) {
    write16(a&~1, (read16(a) & 0xFF) | (v & 0xFF) << 8);
  } else {
    write16(a&~1, (read16(a) & 0xFF00) | (v & 0xFF));
  }
}

void write16(uint32_t a, uint16_t v) {
  if (a % 1) {
  Serial.print(F("unibus: write16 to odd address ")); Serial.println(a, OCT);
  longjmp(trapbuf, INTBUS);
  }
  if (a < 0760000) {
    char * aa = (char *)&a;
    uint8_t bank = ((aa[2] & 3)<<2) | (((aa)[1] & (1<<7))>>7);
    xmem::setMemoryBank(bank, false);
    intptr[(a & 0x7fff) >> 1] = v;
    return;
  }
  switch (a) {
    case 0777776:
      switch (v >> 14) {
        case 0:
          cpu::switchmode(false);
          break;
        case 3:
          cpu::switchmode(true);
          break;
        default:
          Serial.println(F("invalid mode"));
          panic();
      }
      switch ((v >> 12) & 3) {
        case 0:
          cpu::prevuser = false;
          break;
        case 3:
          cpu::prevuser = true;
          break;
        default:
          Serial.println(F("invalid mode"));
          panic();
      }
      cpu::PS = v;
      return;
    case 0777546:
      cpu::LKS = v;
      return;
    case 0777572:
      mmu::SR0 = v;
      return;
  }
  if ((a & 0777770) == 0777560) {
    cons::write16(a, v);
    return;
  }
  if ((a & 0777700) == 0777400) {
    rk11::write16(a, v);
    return;
  }
  if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
    mmu::write16(a, v);
    return;
  }
  Serial.print(F("unibus: write to invalid address ")); Serial.println(a, OCT);
  longjmp(trapbuf, INTBUS);
}

uint16_t read16(uint32_t a) {
  if (a & 1) {
    Serial.print(F("unibus: read16 from odd address ")); Serial.println(a, OCT);
    longjmp(trapbuf, INTBUS);
  }
  if (a < 0760000 ) {
    // bank = a >> 15 costs nearly 5 usec !!
    uint8_t bank = a >> 15;
//    char * aa = (char *)&a;
//    uint8_t bank = ((aa[2] & 3)<<2) | (((aa)[1] & (1<<7))>>7);
    xmem::setMemoryBank(bank, false);
    return intptr[(a & 0x7fff) >> 1];
  }
  if (a == 0777546) {
    return cpu::LKS;
  }

  if (a == 0777570) {
    return 0173030;
  }

  if (a == 0777572) {
    return mmu::SR0;
  }

  if (a == 0777576) {
    return mmu::SR2;
  }

  if (a == 0777776) {
    return cpu::PS;
  }

  if ((a & 0777770) == 0777560) {
    return cons::read16(a);
  }

  if ((a & 0777760) == 0777400) {
    return rk11::read16(a);
  }

  if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
    return mmu::read16(a);
  }

  Serial.print(F("unibus: read from invalid address ")); Serial.println(a, OCT);
  longjmp(trapbuf, INTBUS);
}

};
