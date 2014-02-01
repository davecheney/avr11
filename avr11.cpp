#include <stdint.h>
#include <SdFat.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "unibus.h"
#include "cpu.h"
#include "xmem.h"

int serialWrite(char c, FILE *f) {
  Serial.write(c);
  return 0;
}

SdFat sd;

void setup(void)
{
  // setup all the SPI pins, ensure all the devices are deselected
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  pinMode(13, OUTPUT); digitalWrite(13, LOW);  // rk11
  pinMode(53, OUTPUT); digitalWrite(53, HIGH);

  // Start the UART
  Serial.begin(19200) ;
  fdevopen(serialWrite, NULL);

  Serial.println(F("Reset"));

  // Xmem test
  xmem::SelfTestResults results;

  xmem::begin(false);
  results = xmem::selfTest();
  if (!results.succeeded) {
    Serial.println(F("xram test failure"));
    panic();
  }

  // Initialize SdFat or print a detailed error message and halt
  // Use half speed like the native library.
  // change to SPI_FULL_SPEED for more performance.
  if (!sd.begin(4, SPI_FULL_SPEED)) sd.initErrorHalt();
  if (!rk11::rkdata.open("boot1.RK0", O_RDWR )) {
    sd.errorHalt("opening boot1.RK0 for write failed");
  }

  cpu::reset();
  Serial.println(F("Ready"));
}

union {
  struct {
    uint8_t low;
    uint8_t high;
  } bytes;
  uint16_t value;
} clkcounter;
uint16_t instcounter;

// On a 16Mhz atmega 2560 this loop costs 21usec per emulated instruction
// This cost is just the cost of the loop and fetching the instruction at the PC.
// Actual emulation of the instruction is another ~40 usec per instruction.
static void loop0() {
  for (;;) {
    //the itab check is very cheap
    if ((itab[0].vec) && (itab[0].pri >= ((cpu::PS >> 5) & 7))) {
      cpu::handleinterrupt();
      return; // exit from loop to reset trapbuf
    }
    cpu::step();
    if (INSTR_TIMING && (++instcounter == 0)) {
      Serial.println(millis());
    }
    if (ENABLE_LKS) {
      ++clkcounter.value;
      if (clkcounter.bytes.high == 1 << 6) {
        clkcounter.value = 0;
        cpu::LKS |= (1 << 7);
        if (cpu::LKS & (1 << 6)) {
          cpu::interrupt(INTCLOCK, 6);
        }
      }
    }
    // costs 3 usec
    cons::poll();
  }
}

jmp_buf trapbuf;

void loop() {
  uint16_t vec = setjmp(trapbuf);
  if (vec) {
    cpu::trapat(vec);
  }
  loop0();
}

void panic() {
  printstate();
  for (;;) delay(1);
}
