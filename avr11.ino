#include <stdio.h>
#include <SdFat.h>
#include <setjmp.h>
#ifdef __ATMEGA2560__
#include <avr/io.h>
#define BAUD 9600
#include <util/setbaud.h>
#endif
#include "avr.h"

SdFat sd;

#ifdef __ATMEGA2560__ 
int uart_putchar(char c, FILE *stream) {
  if (c == '\n') {
    uart_putchar('\r', stream);
  }
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE uartout = { 0 };

void uart_init(void) {
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;

#if USE_2X
  UCSR0A |= _BV(U2X0);
#else
  UCSR0A &= ~(_BV(U2X0));
#endif

  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8-bit data */
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);   /* Enable RX and TX */
  fdev_setup_stream(&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uartout;
}
#endif

void setup() {
  Serial.begin(9600);
  // setup all the SPI pins, ensure all the devices are deselected
  pinMode(6, OUTPUT); digitalWrite(6, HIGH);
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(13, OUTPUT); digitalWrite(13, LOW);  // rk11
  pinMode(18, OUTPUT); digitalWrite(18, LOW); // timing interrupt, high while CPU is stepping

  printf("Reset\n");
  
  #ifdef __ATMEGA2560__
  // QuadRAM test
  xmem::SelfTestResults results;

  xmem::begin(false);
  results=xmem::selfTest();
  if(!results.succeeded) {
    printf("xram test failure\n");
    panic();
  }
  #endif

  // Initialize SdFat or print a detailed error message and halt
  if (!sd.begin(0x8, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!rk11::rkdata.open("boot1.RK0", O_RDWR )) {
    printf("opening boot1.RK0 for write failed\n");
  }

  cpu::reset();
  printf("Ready\n");
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
       
    digitalWrite(18, HIGH);//sbi(PORTD, 3);
    cpu::step();
    digitalWrite(18, LOW);//cbi(PORTD, 3);
    
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

uint16_t trap(uint16_t vec) {
  longjmp(trapbuf, INTBUS);
  return vec; // not reached
}

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
