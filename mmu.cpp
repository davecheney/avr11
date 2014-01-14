#include <Arduino.h>
#include "avr11.h"
#include "mmu.h"

extern uint16_t SR0;
extern uint16_t SR2;
extern uint16_t PC;

page pages[16];

bool page::read() { 
  return pdr&2;
}

bool page::write() { 
  return pdr&6; 
}
bool page::ed() { 
  return pdr&8; 
}

uint16_t page::addr() {
  return par & 07777;
}

uint16_t page::len() {
  return (pdr >> 8) &0x7f;
}

void mmuinit() {
  uint8_t i;
  for (i = 0; i < 16; i++) {
    pages[i] = createpage(0, 0);
  }
}

page createpage(uint16_t par, uint16_t pdr) {
  page p = { 
    par, pdr   };
  return p;
}

uint32_t decode(uint16_t a, uint8_t w, uint8_t user) {
  page p;
  uint32_t aa, block, disp;
  if (!(SR0&1)) {
    aa = (uint32_t)a;
    if (aa >= 0170000) {
      aa += 0600000;
    } 
    return aa;
  }
  if (user) {
    p = pages[(a>>13)+8];
  } 
  else {
    p = pages[(a >> 13)];
  }

  if (w && !p.write()) {
    SR0 = (1 << 13) | 1;
    SR0 |= a >> 12 & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); //panic(trap{INTFAULT, "write to read-only page " + ostr(a, 6)})
  }
  if (!p.read()) {
    SR0 = (1 << 15) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); //panic(trap{INTFAULT, "read from no-access page " + ostr(a, 6)})
  }
  block = a >> 6 & 0177;
  disp = a & 077;
  if (((p.ed() && (block < p.len())) || !(p.ed() && (block > p.len())))) {
    //if(p.ed ? (block < p.len) : (block > p.len)) {
    SR0 = (1 << 14) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); // panic(trap{INTFAULT, "page length exceeded, address " + ostr(a, 6) + " (block " + ostr(block, 3) + ") is beyond length " + ostr(p.len, 3)})
  }
  if (w) {
    // watch out !
    p.pdr |= 1 << 6;
  }
  return ((block+p.addr()) << 6) + disp;
}

uint16_t mmuread16(int32_t a) {
  uint8_t i = ((a & 017) >> 1);
  if ((a >= 0772300) && (a < 0772320)) {
    return pages[i].pdr;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    return pages[i].par;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    return pages[i+8].pdr;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    return pages[i+8].par;
  }
  panic(); //trap{INTBUS, "invalid read from " + ostr(a, 6)})
}

void mmuwrite16(int32_t a, uint16_t v) {
  uint8_t i = ((a & 017) >> 1);
  if ((a >= 0772300) && (a < 0772320)) {
    pages[i] = createpage(pages[i].par, v);
    return;
  }
  if ((a >= 0772340) && (a < 0772360)) {
    pages[i] = createpage(v, pages[i].pdr);
    return;
  }
  if ((a >= 0777600) && (a < 0777620)) {
    pages[i+8] = createpage(pages[i+8].par, v);
    return;
  }
  if ((a >= 0777640) && (a < 0777660)) {
    pages[i+8] = createpage(v, pages[i+8].pdr);
    return;
  }
  panic(); //trap{INTBUS, "write to invalid address " + ostr(a, 6)  }
}
