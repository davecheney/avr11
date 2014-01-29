/*
 * xmem.cpp
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 *
 *  Modified by Rugged Circuits LLC (25 Oct 2011) for use with the QuadRAM 512k shield:
 *     http://ruggedcircuits.com/html/quadram.html
 *  
 *  Version 1.2:
 *  -----------
 *     * Fixed reference to 'bank_' in setMemoryBank().
 *     * Added include for Arduino.h/WProgram.h if using Arduino IDE. 
 *     (Contributed by Adam Watson - adam@adamlwatson.com)
 *
 *  Version 1.1:
 *  -----------
 *     * begin() function modified to also set __brkval to the RAM start so it doesn't grow into the stack
 *       (thanks to Gene Reeves).
 *
 */

// Select which shield you are using, Andy Brown's or the Rugged Circuits QuadRAM Shield.
// Only uncomment one of the two lines below.

#define QUADRAM_SHIELD
//#define ANDYBROWN_SHIELD

#include <avr/io.h>
#include "xmem.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif


namespace xmem {

	/*
	 * State for all 8 banks
	 */

	struct heapState bankHeapStates[8];

	/*
	 * The currently selected bank
	 */

	uint8_t currentBank;

	/*
	 * Initial setup. You must call this once
	 */

	void begin(bool heapInXmem_) {

		uint8_t bank;

		// set up the xmem registers

		XMCRB=0; // need all 64K. no pins released
		XMCRA=1<<SRE; // enable xmem, no wait states

#if defined(QUADRAM_SHIELD)
		// set up the bank selector pins (address lines A16..A18)
		// these are on pins 42,43,44 (PL7,PL6,PL5). Also, enable
        // the RAM by driving PD7 (pin 38) low.

        pinMode(38, OUTPUT); digitalWrite(38, LOW);
        pinMode(42, OUTPUT);
        pinMode(43, OUTPUT);
        pinMode(44, OUTPUT);
#elif defined(ANDYBROWN_SHIELD)
		// set up the bank selector pins (address lines A16..A18)
		// these are on pins 38,42,43 (PD7,PL7,PL6)

		DDRD|=_BV(PD7);
		DDRL|=(_BV(PL6)|_BV(PL7));
#endif

		// initialise the heap states

		if(heapInXmem_) {
			__malloc_heap_end=static_cast<char *>(XMEM_END);
			__malloc_heap_start=static_cast<char *>(XMEM_START);
			__brkval=static_cast<char *>(XMEM_START);
		}

		for(bank=0;bank<8;bank++)
			saveHeap(bank);

		// set the current bank to zero

		setMemoryBank(0,false);
	}

	/*
	 * Set the memory bank
	 */

	void setMemoryBank(uint8_t bank_,bool switchHeap_) {

		// check

		if(bank_==currentBank)
			return;

		// save heap state if requested

		if(switchHeap_)
			saveHeap(currentBank);

		// switch in the new bank

#if defined(QUADRAM_SHIELD)
        // Write lower 3 bits of 'bank' to upper 3 bits of Port L
		PORTL = (PORTL & 0x1F) | ((bank_ & 0x7) << 5);

#elif defined(ANDYBROWN_SHIELD)
		if((bank_&1)!=0)
			PORTD|=_BV(PD7);
		else
			PORTD&=~_BV(PD7);

		if((bank_&2)!=0)
			PORTL|=_BV(PL7);
		else
			PORTL&=~_BV(PL7);

		if((bank_&4)!=0)
			PORTL|=_BV(PL6);
		else
			PORTL&=~_BV(PL6);
#endif

		// save state and restore the malloc settings for this bank

		currentBank=bank_;

		if(switchHeap_)
			restoreHeap(currentBank);
	}

	/*
	 * Save the heap variables
	 */

	void saveHeap(uint8_t bank_) {
		bankHeapStates[bank_].__malloc_heap_start=__malloc_heap_start;
		bankHeapStates[bank_].__malloc_heap_end=__malloc_heap_end;
		bankHeapStates[bank_].__brkval=__brkval;
		bankHeapStates[bank_].__flp=__flp;
	}

	/*
	 * Restore the heap variables
	 */

	void restoreHeap(uint8_t bank_) {
		__malloc_heap_start=bankHeapStates[bank_].__malloc_heap_start;
		__malloc_heap_end=bankHeapStates[bank_].__malloc_heap_end;
		__brkval=bankHeapStates[bank_].__brkval;
		__flp=bankHeapStates[bank_].__flp;
	}

	/*
	 * Self test the memory. This will destroy the entire content of all
	 * memory banks so don't use it except in a test scenario.
	 */

	SelfTestResults selfTest() {

		volatile uint8_t *ptr;
		uint8_t bank,writeValue,readValue;
		SelfTestResults results;

		// write an ascending sequence of 1..237 running through
		// all memory banks

		writeValue=1;
		for(bank=0;bank<8;bank++) {

			setMemoryBank(bank);

			for(ptr=reinterpret_cast<uint8_t *> (0xFFFF);ptr>=reinterpret_cast<uint8_t *> (0x2200);ptr--) {
				*ptr=writeValue;

				if(writeValue++==237)
					writeValue=1;
			}
		}

		// verify the writes

		writeValue=1;
		for(bank=0;bank<8;bank++) {

			setMemoryBank(bank);

			for(ptr=reinterpret_cast<uint8_t *> (0xFFFF);ptr>=reinterpret_cast<uint8_t *> (0x2200);ptr--) {

				readValue=*ptr;

				if(readValue!=writeValue) {
					results.succeeded=false;
					results.failedAddress=ptr;
					results.failedBank=bank;
					return results;
				}

				if(writeValue++==237)
					writeValue=1;
			}
		}

		results.succeeded=true;
		return results;
	}
}
