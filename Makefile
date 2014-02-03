ARDUINO_HOME=$(HOME)/bin/arduino-1.5.5

PROJECT=avr11
MCU=atmega2560 

CC=$(ARDUINO_HOME)/hardware/tools/avr/bin/avr-gcc
CXX=$(ARDUINO_HOME)/hardware/tools/avr/bin/avr-g++
AR=$(ARDUINO_HOME)/hardware/tools/avr/bin/avr-ar
OBJCOPY=$(ARDUINO_HOME)/hardware/tools/avr/bin/avr-objcopy

CFLAGS=-c -g -Os -w -Wall -ffunction-sections -fdata-sections -mmcu=$(MCU) -DF_CPU=16000000L -DARDUINO=155 -DARDUINO_AVR_MEGA2560 -DARDUINO_ARCH_AVR -I$(ARDUINO_HOME)/hardware/arduino/avr/cores/arduino -I$(ARDUINO_HOME)/hardware/arduino/avr/variants/mega -I./../libraries/SdFat
CPPFLAGS=-fno-exceptions 

SRC_FILES=avr11.cpp cons.cpp cpu.cpp unibus.cpp disasm.cpp mmu.cpp rk05.cpp xmem.cpp
OBJ_FILES=$(SRC_FILES:.cpp=.o)

CORE_FILES=malloc.o realloc.o hooks.o WInterrupts.o wiring.o wiring_analog.o wiring_digital.o wiring_pulse.o wiring_shift.o HardwareSerial.o HID.o main.o new.o Print.o Stream.o Tone.o USBCore.o WMath.o WString.o CDC.o

all: $(PROJECT).hex

clean:
	rm -f *.o *.elf *.eep

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) $< -o $@

%.o: ../libraries/SdFat/%.cpp 
	$(CXX) $(CFLAGS) $(CPPFLAGS) $< -o $@

%.o: $(ARDUINO_HOME)/hardware/arduino/avr/cores/arduino/%.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) $< -o $@

%.o: $(ARDUINO_HOME)/hardware/arduino/avr/cores/arduino/avr-libc/%.c
	$(CC) $(CFLAGS) -o $@ $^

%.o: $(ARDUINO_HOME)/hardware/arduino/avr/cores/arduino/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(PROJECT).elf: SdFatUtil.o SdFatErrorPrint.o SdBaseFile.o Sd2Card.o SdFat.o MinimumSerial.o $(OBJ_FILES) istream.o SdFile.o SdSpiArduino.o SdSpiAVR.o SdSpiMK20DX128.o SdSpiSAM3X.o SdStream.o SdVolume.o $(CORE_FILES) 
	$(CC) -Os -L. -Wl,--gc-sections,--relax -mmcu=$(MCU) -o $@ $^ -lm

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $^ $(PROJECT).eep 
	$(OBJCOPY) -O ihex -R .eeprom $^ $@
