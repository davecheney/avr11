#include <Arduino.h>
#include <SdFat.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "unibus.h"
#include "cpu.h"

SdFat sd;



void setup(void)
{
  // setup all the SPI pins, ensure all the devices are deselected
  pinMode(6, OUTPUT); digitalWrite(6, HIGH);
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(13, OUTPUT); digitalWrite(13, LOW);  // rk11
  pinMode(18, OUTPUT); digitalWrite(18, LOW); // timing interrupt, high while CPU is stepping

  // Start the UART
  Serial.begin(115200) ;

  Serial.println(F("Reset"));

  // Initialize SdFat or print a detailed error message and halt
  if (!sd.begin(0x8, SPI_HALF_SPEED)) sd.initErrorHalt();
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
       
    digitalWrite(18, HIGH);
    cpu::step();
    digitalWrite(18, LOW);
    
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
