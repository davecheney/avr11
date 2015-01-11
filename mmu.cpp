#include <stdio.h>
#include <SdFat.h>
#include <setjmp.h>
#include "avr11.h"

namespace mmu {

struct {
  uint16_t par, pdr;
} pages[16];
uint16_t SR0, SR2;

#define READ(x) (x.pdr & 2)
#define WRITE(x) (x.pdr & 6)
#define ED(x) (x.pdr & 8)
#define LEN(x) ((x.pdr >> 8) & 0x7f)
#define ADDR(x) (uint32_t)(x.par & 07777)

uint32_t decode(const uint16_t a, const uint8_t w, const uint8_t user) {
  if ((SR0 & 1) == 0) {
    return a > 0167777 ? ((uint32_t)a) + 0600000 : a;
  }
  const uint8_t i = user ? ((a >> 13) + 8) : (a >> 13);
  if (w && !WRITE(pages[i])) {
    SR0 = (1 << 13) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = cpu::PC;

    printf("mmu::decode write to read-only page %06o\n", a);
    trap(INTFAULT);
  }
  if (!READ(pages[i])) {
    SR0 = (1 << 15) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = cpu::PC;
    printf("mmu::decode read from no-access page %06o\n", a);
    trap(INTFAULT);
  }
  const uint8_t block = (a >> 6) & 0177;
  const uint8_t disp = a & 077;
  // if ((p.ed() && (block < p.len())) || (!p.ed() && (block > p.len()))) {
  if (ED(pages[i]) ? (block < LEN(pages[i])) : (block > LEN(pages[i]))) {
    SR0 = (1 << 14) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = cpu::PC;
    printf("page length exceeded, address %06o (block %03o) is beyond length "
           "%03o\r\n",
           a, block, LEN(pages[i]));
    trap(INTFAULT);
  }
  if (w) {
    pages[i].pdr |= 1 << 6;
  }
  // danger, this can be cast to a uint16_t if you aren't careful
  uint32_t aa = ADDR(pages[i]);
  aa += block;
  aa <<= 6;
  aa += disp;
  if (DEBUG_MMU) {
    printf("decode: slow %06o -> %06o\n", a, aa);
  }

  return aa;
}

uint16_t read16(const uint32_t a) {
  if ((a >= 0772300) && (a < 0772320)) {
    return pages[((a & 017) >> 1)].pdr;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    return pages[((a & 017) >> 1)].par;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    return pages[((a & 017) >> 1) + 8].pdr;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    return pages[((a & 017) >> 1) + 8].par;
  }
  printf("mmu::read16 invalid read from %06o\n", a);
  return trap(INTBUS);
}

void write16(const uint32_t a, const uint16_t v) {
  const uint8_t i = ((a & 017) >> 1);
  if ((a >= 0772300) && (a < 0772320)) {
    pages[i].pdr = v;
    return;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    pages[i].par = v;
    return;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    pages[i + 8].pdr = v;
    return;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    pages[i + 8].par = v;
    return;
  }
  printf("mmu::write16 write to invalid address %06o\n", a);
  trap(INTBUS);
}
};
