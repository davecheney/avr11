#include <stdint.h>
#include <SPI.h>
#include <SdFat.h>
#include <SpiRAM.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "unibus.h"
#include "cpu.h"

int serialWrite(char c, FILE *f) {
  Serial.write(c);
  return 0;
}

SdFat sd;

void setup(void)
{
  // setup all the SPI pins, ensure all the devices are deselected
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);
  pinMode(6, OUTPUT); digitalWrite(6, HIGH);
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  pinMode(53, OUTPUT); digitalWrite(53, HIGH);
  // Start the UART
  Serial.begin(19200) ;
  fdevopen(serialWrite, NULL);

  //   SPI.begin();
  //   SPI.setClockDivider(SPI_CLOCK_DIV2);

  Serial.println(F("Reset"));
    // Initialize SdFat or print a detailed error message and halt
  // Use half speed like the native library.
  // change to SPI_FULL_SPEED for more performance.
  if (!sd.begin(4, SPI_FULL_SPEED)) sd.initErrorHalt();
    if (!rk11::rkdata.open("boot1.RK0", O_RDWR )) {
    sd.errorHalt("opening boot1.RK0 for write failed");
  }

  unibus::init();
  cpu::reset();
  Serial.println(F("Ready"));
}

uint16_t clkcounter;
uint16_t instcounter;

jmp_buf trapbuf;

void loop() {
  uint16_t vec = setjmp(trapbuf);
  if (vec == 0) {
    loop0();
  }  else {
    cpu::trapat(vec);
  }
}

void loop0() {
  while (true) {
    if ((itab[0].vec > 0) && (itab[0].pri >= ((cpu::PS >> 5) & 7))) {
      cpu::handleinterrupt(itab[0].vec);
      uint8_t i;
      for (i = 0; i < ITABN - 1; i++) {
        itab[i] = itab[i + 1];
      }
      itab[ITABN - 1].vec = 0;
      itab[ITABN - 1].pri = 0;
      return; // exit from loop to reset trapbuf
    }
    cpu::step();
    if (INSTR_TIMING) {
      if (++instcounter == 0) {
        Serial.println(millis());
      }
    }
    if (++clkcounter > 39999) {
      clkcounter = 0;
      cpu::LKS |= (1 << 7);
      if (cpu::LKS & (1 << 6)) {
        cpu::interrupt(INTCLOCK, 6);
      }
    }
    cons::poll();
  }
}

void panic() {
  printstate();
  while (true) delay(1);
}
