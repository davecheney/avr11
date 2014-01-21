#include <stdint.h>
#include <SPI.h>
#include <SD.h>
#include <SpiRAM.h>
#include "avr11.h"
#include "rk05.h"
#include "unibus.h"
#include "cpu.h"


int serialWrite(char c, FILE *f) {
    Serial.write(c);
    return 0;
}

pdp11::unibus unibus;

void setup(void)
{
  // setup all the SPI pins, ensure all the devices are deselected
    pinMode(4, OUTPUT); digitalWrite(4, HIGH);
  pinMode(6, OUTPUT); digitalWrite(6, HIGH);
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  pinMode(53, OUTPUT); digitalWrite(53, HIGH);
  // Start the UART
  Serial.begin(9600) ;
  fdevopen(serialWrite, NULL);

  Serial.println(F("Reset"));
  rkinit(); // must call rkinit first to setup sd card
  unibus.init();
  cpureset();
  Serial.println(F("Ready"));
}

void loop() {
  if (setjmp(trapbuf) == 0) {
            if ((itab[0].vec > 0) && (itab[0].pri >= ((PS)>>5)&7)) {
                handleinterrupt(itab[0].vec);
                uint8_t i;
                for (i = 0; i < ITABN-1; i++) {
                        itab[i] = itab[i+1];
                }
                itab[ITABN-1].vec = 0;
                itab[ITABN-1].pri = 0;
        }

    cpustep();
  } 
  else {
    trapat(setjmp(trapbuf));
  }
  unibus.cons.poll();
  rkstep();
}

void panic() {
  printstate();
  while(true) delay(1);
}
