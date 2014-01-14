#include <stdint.h>
#include <SD.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "mmu.h"
#include "cpu.h"
#include "unibus.h"

// we need fundamental FILE definitions and printf declarations
#include <stdio.h>

// create a FILE structure to reference our UART output function

static FILE uartout = {0} ;

// create a output function
// This works because Serial.write, although of
// type virtual, already exists.
static int uart_putchar (char c, FILE *stream)
{
    Serial.write(c) ;
    return 0 ;
}

void setup(void)
{
   // Start the UART
   Serial.begin(9600) ;

   // fill in the UART file descriptor with pointer to writer.
   fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);

   // The uart is the standard output device STDOUT.
   stdout = &uartout ;

  printf("setting up...\r\n"); 
  cpureset();
  printf("setup done.\r\n");
  rkinit();
}

void loop() {
  cpustep();
  rkstep();
}

int32_t R[8];

void printstate() {
  uint32_t ia;
  uint16_t inst;

  printf("R0 %06o R1 %06o R2 %06o R3 %06o R4 %06o R5 %06o R6 %06o R7 %06o\r\n[", R[0], R[1], R[2], R[3], R[4], R[5], R[6], R[7]); 

  if (prevuser) {
    printf("u");
  } 
  else {
    printf("k");
  }
  if (curuser) {
    printf("U");
  } 
  else {
    printf("K");
  }
  if (PS&FLAGN) {
    printf("N");
  } 
  else {
    printf(" ");
  }
  if (PS&FLAGZ) {
    printf("Z");
  } 
  else {
    printf(" ");
  }
  if (PS&FLAGV) {
    printf("V");
  } 
  else {
    printf(" ");
  }
  if (PS&FLAGC) {
    printf("C");
  } 
  else {
    printf(" ");
  }
  ia = decode(PC, false, curuser);
  inst = physread16(ia);
  printf("]\tinstr %06o: %06o\t ", PC, inst);
  disasm(ia);
  printf("\r\n");
}

void panic() {
  printstate();
  printf("panic\n");
  while (true) delay(1);
}

/**
 * 
 * var writedebug = fmt.Print
 * 
 * 
 * 
 * type trap struct {
 * 	num int
 * 	msg string
 * }
 * 
 * func (t trap) String() string {
 * 	return fmt.Sprintf("trap %06o occured: %s", t.num, t.msg)
 * }
 * 
 * func interrupt(vec, pri int) {
 * 	var i int
 * 	if vec&1 == 1 {
 * 		panic("Thou darst calling interrupt() with an odd vector number?")
 * 	}
 * 	for ; i < len(interrupts); i++ {
 * 		if interrupts[i].pri < pri {
 * 			break
 * 		}
 * 	}
 * 	for ; i < len(interrupts); i++ {
 * 		if interrupts[i].vec >= vec {
 * 			break
 * 		}
 * 	}
 * 	// interrupts.splice(i, 0, {vec: vec, pri: pri});
 * 	interrupts = append(interrupts[:i], append([]intr{{vec, pri}}, interrupts[i:]...)...)
 * }
 * 
 * func (k *KB11) handleinterrupt(vec int) {
 * 	defer func() {
 * 		trap = recover()
 * 		switch trap = trap.(type) {
 * 		case struct {
 * 			num int
 * 			msg string
 * 		}:
 * 			k.trapat(trap.num, trap.msg)
 * 		case nil:
 * 			break
 * 		default:
 * 			panic(trap)
 * 		}
 * 		k.R[7] = int(memory[vec>>1])
 * 		PS = memory[(vec>>1)+1]
 * 		if prevuser {
 * 			PS |= (1 << 13) | (1 << 12)
 * 		}
 * 		waiting = false
 * 	}()
 * 	prev = PS
 * 	k.switchmode(false)
 * 	k.push(prev)
 * 	k.push(uint16(k.R[7]))
 * }
 * 
 * func (k *KB11) trapat(vec int, msg string) {
 * 	var prev uint16
 * 	defer func() {
 * 		t = recover()
 * 		switch t = t.(type) {
 * 		case trap:
 * 			writedebug("red stack trap!\n")
 * 			memory[0] = uint16(k.R[7])
 * 			memory[1] = prev
 * 			vec = 4
 * 			panic("fatal")
 * 		case nil:
 * 			break
 * 		default:
 * 			panic(t)
 * 		}
 * 		k.R[7] = int(memory[vec>>1])
 * 		PS = memory[(vec>>1)+1]
 * 		if prevuser {
 * 			PS |= (1 << 13) | (1 << 12)
 * 		}
 * 		waiting = false
 * 	}()
 * 	if vec&1 == 1 {
 * 		panic("Thou darst calling trapat() with an odd vector number?")
 * 	}
 * 	writedebug("trap " + ostr(vec, 6) + " occured: " + msg + "\n")
 * 	k.printstate()
 * 
 * 	prev = PS
 * 	k.switchmode(false)
 * 	k.push(prev)
 * 	k.push(uint16(k.R[7]))
 * }
 * 
 * 
 * 
 * 
 * 
 * 	k.unibus.rk.Step()
 * 	k.unibus.cons.Step(k)
 * }
 * 
 * func (k *KB11) onestep() {
 * 	defer func() {
 * 		t = recover()
 * 		switch t = t.(type) {
 * 		case trap:
 * 			k.trapat(t.num, t.msg)
 * 		case nil:
 * 			// ignore
 * 		default:
 * 			panic(t)
 * 		}
 * 	}()
 * 
 * 	k.step()
 * 	if len(interrupts) > 0 && interrupts[0].pri >= ((int(PS)>>5)&7) {
 * 		//fmt.Printf("IRQ: %06o\n", interrupts[0].vec)
 * 		k.handleinterrupt(interrupts[0].vec)
 * 		interrupts = interrupts[1:]
 * 	}
 * 	clkcounter++
 * 	if clkcounter >= 40000 {
 * 		clkcounter = 0
 * 		k.unibus.LKS |= (1 << 7)
 * 		if k.unibus.LKS&(1<<6) != 0 {
 * 			interrupt(INTCLOCK, 6)
 * 		}
 * 	}
 * }
 * 
 

 * package pdp11
 * 
 * }
 * 
 */
 

