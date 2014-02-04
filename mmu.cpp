#include <Arduino.h>
#include "avr11.h"
#include "cpu.h"
#include "mmu.h"

namespace mmu {

struct page {
    uint16_t par;
    union {
     struct {
      uint8_t low;
      uint8_t high;
     } bytes;
     uint16_t word;
    } pdr;
};

page pages[16];
uint16_t SR0, SR2;

uint32_t decode(const uint16_t a, const bool w, const bool user) {
  if (SR0 & 1) {
    // mmu enabled
    const uint8_t i = user ? ((a >> 13) + 8) : (a >> 13);
    if (w && !pages[i].pdr.bytes.low & 6) {
      SR0 = (1 << 13) | 1;
      SR0 |= (a >> 12) & ~1;
      if (user) {
        SR0 |= (1 << 5) | (1 << 6);
      }
      SR2 = cpu::PC;

      Serial.print(F("mmu::decode write to read-only page ")); Serial.println(a, OCT);
      longjmp(trapbuf, INTFAULT);
    }
    if (!pages[i].pdr.bytes.low & 2) {
      SR0 = (1 << 15) | 1;
      SR0 |= (a >> 12) & ~1;
      if (user) {
        SR0 |= (1 << 5) | (1 << 6);
      }
      SR2 = cpu::PC;
      Serial.print(F("mmu::decode read from no-access page ")); Serial.println(a, OCT);
      longjmp(trapbuf, INTFAULT);
    }
    const uint8_t block = (a >> 6) & 0177;
    const uint8_t disp = a & 077;
    // if ((p.ed() && (block < p.len())) || (!p.ed() && (block > p.len()))) {
    if ((pages[i].pdr.bytes.low & 8) ? (block < (pages[i].pdr.bytes.high & 0x7f)) : (block > (pages[i].pdr.bytes.high & 0x7f))) {
      SR0 = (1 << 14) | 1;
      SR0 |= (a >> 12) & ~1;
      if (user) {
        SR0 |= (1 << 5) | (1 << 6);
      }
      SR2 = cpu::PC;
      printf("page length exceeded, address %06o (block %03o) is beyond length %03o\r\n", a, block, (pages[i].pdr.bytes.high & 0x7f));
      longjmp(trapbuf, INTFAULT);
    }
    if (w) {
      pages[i].pdr.bytes.low |= 1 << 6;
    }
    // danger, this can be cast to a uint16_t if you aren't careful
    uint32_t aa = pages[i].par & 07777;
    aa += block;
    aa <<= 6;
    aa += disp;
    if (DEBUG_MMU) {
      Serial.print("decode: slow "); Serial.print(a, OCT); Serial.print(" -> "); Serial.println(aa, OCT);
    }
    return aa;
  }
  // mmu disabled, fast path
  return a > 0167777 ? ((uint32_t)a) + 0600000 : a;
}

uint16_t read16(const uint32_t a) {
  if ((a >= 0772300) && (a < 0772320)) {
    return pages[((a & 017) >> 1)].pdr.word;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    return pages[((a & 017) >> 1)].par;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    return pages[((a & 017) >> 1) + 8].pdr.word;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    return pages[((a & 017) >> 1) + 8].par;
  }
  Serial.print(F("mmu::read16 invalid read from ")); Serial.println(a, OCT);
  longjmp(trapbuf, INTBUS);
}

void write16(const uint32_t a, const uint16_t v) {
  uint8_t i = ((a & 017) >> 1);
  if ((a >= 0772300) && (a < 0772320)) {
    pages[i].pdr.word = v;
    return;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    pages[i].par = v;
    return;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    pages[i + 8].pdr.word = v;
    return;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    pages[i + 8].par = v;
    return;
  }
  Serial.print(F("mmu::write16 write to invalid address ")); Serial.println(a, OCT);
  longjmp(trapbuf, INTBUS);
}

};
