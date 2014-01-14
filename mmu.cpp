#include <Arduino.h>
#include "avr11.h"
#include "mmu.h"

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
    par, pdr };
  return p;
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





