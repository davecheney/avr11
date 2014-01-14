#include <stdint.h>
#include <SD.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "mmu.h"
#include "cpu.h"
#include "unibus.h"

void setup(void)
{
  // Start the UART
  Serial.begin(9600) ;
  Serial.println("setting up..."); 
  cpureset();
  Serial.println("setup done.");
  rkinit();
}

void loop() {
  cpustep();
  rkstep();
}

void printstate() {
  uint32_t ia;
  uint16_t inst;

  Serial.print("R0 "); 
  Serial.print(R[0], OCT);
  Serial.print(" R1 "); 
  Serial.print(R[1], OCT);
  Serial.print(" R2 "); 
  Serial.print(R[2], OCT);
  Serial.print(" R3 "); 
  Serial.print(R[3], OCT);
  Serial.print(" R4 "); 
  Serial.print(R[4], OCT);
  Serial.print(" R5 "); 
  Serial.print(R[5], OCT);
  Serial.print(" R6 "); 
  Serial.print(R[6], OCT);
  Serial.print(" R7 "); 
  Serial.print(R[7], OCT);
  Serial.print("\n [");

  if (prevuser) {
    Serial.print("u");
  } 
  else {
    Serial.print("k");
  }
  if (curuser) {
    Serial.print("U");
  } 
  else {
    Serial.print("K");
  }
  if (PS&FLAGN) {
    Serial.print("N");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGZ) {
    Serial.print("Z");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGV) {
    Serial.print("V");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGC) {
    Serial.print("C");
  } 
  else {
    Serial.print(" ");
  }
  ia = decode(PC, false, curuser);
  inst = physread16(ia);
  Serial.print("]  instr ");
  Serial.print(PC, OCT);
  Serial.print(": ");
  Serial.print(inst, OCT);
  Serial.print("\n"); // + "   " + disasm(ia) + "\n")
}

void panic() {
  printstate();
  Serial.println("panic");
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
 

