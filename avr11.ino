#include <stdint.h>
#include <SPI.h>
#include <SD.h>
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
  // Start the UART
  Serial.begin(19200) ;
  fdevopen(serialWrite, NULL);

  Serial.println(F("Reset"));
  cpureset();
  rkinit();
  Serial.println(F("Ready"));
}

void loop() {
  if (setjmp(trapbuf) == 0) {
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

/**
 * 
 * var writedebug = fmt.Print
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
 * 
 * 
 * package pdp11
 * 
 * }
 * 
 */



