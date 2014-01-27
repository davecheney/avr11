#include <Arduino.h>
#include "avr11.h"
#include "cpu.h"
#include "mmu.h"

bool page::read() {
  return pdr & 2;
}

bool page::write() {
  return pdr & 6;
}
bool page::ed() {
  return pdr & 8;
}

uint16_t page::addr() {
  return par & 07777;
}

uint16_t page::len() {
  return (pdr >> 8) & 0x7f;
}

void pdp11::mmu::reset() {
  uint8_t i;
  for (i = 0; i < 16; i++) {
    pages[i].par = 0;
    pages[i].pdr = 0;
  }
}

void pdp11::mmu::dumppages() {
  uint8_t i;
  for (i = 0; i < 16; i++) {
    printf("%0x: %06o %06o\r\n", i, pages[i].par, pages[i].pdr);
  }
}

uint32_t pdp11::mmu::decode(uint16_t a, uint8_t w, uint8_t user) {
  if (((uint8_t)SR0 & 1) == 0) {
    return a > 0167777 ? ((uint32_t)a) + 0600000 : a;
  }
  uint8_t i = user ? ((a >> 13) + 8) : (a >> 13);
  if (w && !pages[i].write()) {
    SR0 = (1 << 13) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;

    printf("write to read-only page %06o\r\n", a);
    trap(INTFAULT);
  }
  if (!pages[i].read()) {
    SR0 = (1 << 15) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    printf("read from no-access page %06o\r\n", a);
    trap(INTFAULT);
  }
  uint8_t block = (a >> 6) & 0177;
  uint8_t disp = a & 077;
  // if ((p.ed() && (block < p.len())) || (!p.ed() && (block > p.len()))) {
  if (pages[i].ed() ? (block < pages[i].len()) : (block > pages[i].len())) {
    SR0 = (1 << 14) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    printf("page length exceeded, address %06o (block %03o) is beyond length %03o\r\n", a, block, pages[i].len());
    trap(INTFAULT);
  }
  if (w) {
    pages[i].pdr |= 1 << 6;
  }
  // danger, this can be cast to a uint16_t if you aren't careful
  //uint32_t aa = block + p.addr();
  //aa = aa << 6;
  //aa += disp;
  uint32_t aa = (((uint32_t)block) + ((uint32_t)(pages[i].addr())) << 6) + disp;
  if (DEBUG_MMU) {
    Serial.print("decode: slow "); Serial.print(a, OCT); Serial.print(" -> "); Serial.println(aa, OCT);
    //dumppages();
  }

  return aa;
}

uint16_t pdp11::mmu::read16(int32_t a) {
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
  printf("mmu::read16 invalid read from %06o\r\n", a);
  trap(INTBUS);
}

void pdp11::mmu::write16(int32_t a, uint16_t v) {
  uint8_t i = ((a & 017) >> 1);
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
  printf("mmu::write16 write to invalid address %06o\r\n", a);
  trap(INTBUS);
}

