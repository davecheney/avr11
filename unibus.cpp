#include <Arduino.h>
#include "avr11.h"
#include "cpu.h"
#include "unibus.h"
#include "rk05.h"

//uint16_t memory[MEMSIZE];

void pdp11::unibus::init() {
  SD.remove("core");
  core = SD.open("core", FILE_WRITE);
  if (!core) {
    Serial.println("failed to open core file"); 
    abort();
  }
  Serial.print("zeroing core file, ");
  uint8_t buf[1024];
  uint16_t i;
  for (i = 0; i < 128 * 2; i++) {
    core.write(buf, 1024);
  }
  Serial.println("done");
}

uint16_t pdp11::unibus::read8(uint32_t a) {
  if (a & 1) {
    return read16(a & ~1) >> 8;
  }
  return read16(a & ~1) & 0xFF;
}

void pdp11::unibus::write8(uint32_t a, uint16_t v) {
  if (a < 0760000) {
    if (a & 1) {
      core.seek(a);
      core.write(v & 0xff);
      //memory[a >> 1] &= 0xFF;
      //memory[a >> 1] |= v & 0xFF << 8;
    }
    else {
      core.seek(a);
      core.write(v&0xff);
      //memory[a >> 1] &= 0xFF00;
      //memory[a >> 1] |= v & 0xFF;
    }
  }
  else {
    if (a & 1) {
      write16(a&~1, (read16(a) & 0xFF) | (v & 0xFF) << 8);
    }
    else {
      write16(a&~1, (read16(a) & 0xFF00) | (v & 0xFF));
    }
  }
}

void pdp11::unibus::write16(uint32_t a, uint16_t v) {
  //printf("unibus::write16: %06o\t", a); printf("%06o\r\n", v);
  if (a % 1) {
    //panic(trap{INTBUS, "write to odd address " + ostr(a, 6)})
    trap(INTBUS);
  }
  if (a < 0760000) {
    core.seek(a);
    core.write(v & 0xff) ;
    if (core.write((v >> 8) & 0xff) == 0) {
      printf("failed to write to core file");
      panic(); }
    //memory[a >> 1] = v;
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
    //panic(trap{INTBUS, "write to invalid address " + ostr(a, 6)})
    trap(INTBUS);
  }
}

uint16_t pdp11::unibus::read16(uint32_t a) {
  //printf("unibus::read16: %06o\n", a);
  if (a & 1) {
    // panic(trap{INTBUS, "read from odd address " + ostr(a, 6)})
    trap(INTBUS);
  }
  else if (a < 0760000 ) {
    core.seek(a);
    return core.read() | (core.read()<<8);
    //return memory[a >> 1];
  }
  else if (a == 0777546) {
    return LKS;
  }
  else if (a == 0777570) {
    return 0173030;
  }
  else if (a == 0777572) {
    return SR0;
  }
  else if (a == 0777576) {
    return SR2;
  }
  else if (a == 0777776) {
    return PS;
  }
  else if ((a & 0777770) == 0777560) {
    return cons.read16(a);
  }
  else if ((a & 0777760) == 0777400) {
    return rkread16(a);
  }
  else if (((a & 0777600) == 0772200) || ((a & 0777600) == 0777600)) {
    mmu.read16(a);
  }
  //panic(trap{INTBUS, "read from invalid address " + ostr(a, 6)})
  trap(INTBUS);

}
