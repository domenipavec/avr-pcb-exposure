/* File: main.cpp
 * Contains base main function and usually all the other stuff that avr does...
 */
/* Copyright (c) 2012-2013 Domen Ipavec (domen.ipavec@z-v.si)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
#define F_CPU 8000000UL  // 8 MHz
#include <util/delay.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h> 

#include <stdint.h>

#include "bitop.h"
#include "lcds.h"
#include "io.h"

using namespace avr_cpp_lib;

inline void startBeep() {
	SETBIT(DDRB, PB3);
}

inline void stopBeep() {
	CLEARBIT(DDRB, PB3);
}

OutputPin boards[10] = {
	OutputPin(&DDRA, &PORTA, PA0),
	OutputPin(&DDRA, &PORTA, PA1),
	OutputPin(&DDRA, &PORTA, PA2),
	OutputPin(&DDRA, &PORTA, PA3),
	OutputPin(&DDRA, &PORTA, PA4),
	OutputPin(&DDRC, &PORTC, PC7),
	OutputPin(&DDRC, &PORTC, PC6),
	OutputPin(&DDRC, &PORTC, PC5),
	OutputPin(&DDRC, &PORTC, PC4),
	OutputPin(&DDRC, &PORTC, PC3),
};

inline void turnAllOn() {
	for (uint8_t i = 0; i < 10; i++) {
		boards[i].set();
	}
}
inline void turnAllOff() {
	for (uint8_t i = 0; i < 10; i++) {
		boards[i].clear();
	}
}
uint8_t mode;
uint8_t pos;
uint8_t minutes;
uint8_t seconds;
uint8_t EEMEM eemem_minutes;
uint8_t EEMEM eemem_seconds;
uint8_t timer_min;
uint8_t timer_sec;
uint8_t calibration;
volatile bool update;
volatile bool forward;
volatile bool backward;
volatile bool button;

int main() {
	// INIT
	// soft power button
	SETBIT(DDRD, PD1);
	SETBIT(PORTD, PD1);
	SETBIT(PORTD, PD2);
	while (BITCLEAR(PIND, PD2));
	_delay_ms(5);
	
	// encoder
	SETBIT(PORTD, PD4);
	SETBIT(PORTD, PD3);

	// timer 0 with pwm
	TCCR0 = 0b00011100;
	OCR0 = 8;
	
	// timer 1 for 1s
	TCCR1A = 0;
	TCCR1B = 0b00001100;
	OCR1A = 31250;
	SETBIT(TIMSK, OCIE1A);
	
	// external interrupts (on falling edge)
	SETBIT(MCUCR, ISC11);
	SETBIT(MCUCR, ISC01);
	SETBIT(GICR, INT1);
	SETBIT(GICR, INT0);
	
	// lcd
	LCDS lcd(OutputPin(&DDRC, &PORTC, PC2),
		OutputPin(&DDRC, &PORTC, PC1),
		OutputPin(&DDRC, &PORTC, PC0),
		OutputPin(&DDRD, &PORTD, PD7),
		OutputPin(&DDRD, &PORTD, PD6),
		OutputPin(&DDRD, &PORTD, PD5));
	
	// vars
	mode = 0;
	pos = 0;
	update = true;
	
	// read seconds, minutes
	minutes = eeprom_read_byte(&eemem_minutes);
	seconds = eeprom_read_byte(&eemem_seconds);
	
	// beep for start
	startBeep();
	_delay_ms(200);
	stopBeep();
	
	// enable interrupts
	sei();

	for (;;) {
		if (forward) {
			switch (mode) {
				case 0:
					pos++;
					if (pos == 4) {
						pos = 0;
					}
					break;
				case 2:
					if (pos < 3) {
						pos++;
						if (pos == 3) {
							pos = 0;
						}
					} else if (pos == 3) {
						minutes++;
						if (minutes == 10) {
							minutes = 0;
						}
					} else if (pos == 4) {
						seconds += 5;
						if (seconds == 60) {
							seconds = 0;
						}
					}
					break;
				case 3:
					if (pos < 4) {
						pos++;
						if (pos == 4) {
							pos = 0;
						}
					} else if (pos == 4) {
						minutes++;
						if (minutes == 10) {
							minutes = 0;
						}
					} else if (pos == 5) {
						seconds += 5;
						if (seconds == 60) {
							seconds = 0;
						}
					}
					break;
			}
		}
		if (backward) {
			switch (mode) {
				case 0:
					if (pos == 0) {
						pos = 3;
					} else {
						pos--;
					}
					break;
				case 2:
					if (pos < 3) {
						if (pos == 0) {
							pos = 2;
						} else {
							pos--;
						}
					} else if (pos == 3) {
						if (minutes == 0) {
							minutes = 9;
						} else {
							minutes--;
						}
					} else if (pos == 4) {
						if (seconds < 5) {
							seconds = 55;
						} else {
							seconds -= 5;
						}
					}
					break;
				case 3:
					if (pos < 4) {
						if (pos == 0) {
							pos = 3;
						} else {
							pos--;
						}
					} else if (pos == 4) {
						if (minutes == 0) {
							minutes = 9;
						} else {
							minutes--;
						}
					} else if (pos == 5) {
						if (seconds < 5) {
							seconds = 55;
						} else {
							seconds -= 5;
						}
					}
					break;
			}
		
		}
		if (button) {
			switch (mode) {
				case 0:
					switch (pos) {
						case 0:
							// turn all on
							turnAllOn();
							// start timer
							TCNT1 = 0;
							timer_min = minutes;
							timer_sec = seconds;
							mode = 1;
							pos = 0;
							break;
						case 1:
							mode = 2;
							pos = 0;
							break;
						case 2:
							mode = 3;
							pos = 0;
							break;
						case 3:
							CLEARBIT(PORTD, PD1);
							break;
					}
					break;
				case 2:
					switch (pos) {
						case 0:
							pos = 3;
							break;
						case 1:
							pos = 4;
							break;
						case 2:
							// save minutes, seconds
							eeprom_write_byte(&eemem_minutes, minutes);
							eeprom_write_byte(&eemem_seconds, seconds);
							mode = 0;
							pos = 1;
							break;
						case 3:
							pos = 0;
							break;
						case 4:
							pos = 1;
							break;
					}
					break;
				case 3:
					switch (pos) {
						case 0:
							// turn all on
							turnAllOn();
							// start timer
							TCNT1 = 0;
							timer_min = minutes;
							timer_sec = seconds;
							mode = 5;
							break;
						case 1:
							pos = 0;
							mode = 0;
							minutes = eeprom_read_byte(&eemem_minutes);
							seconds = eeprom_read_byte(&eemem_seconds);
							break;
						case 2:
							pos = 4;
							break;
						case 3:
							pos = 5;
							break;
						case 4:
							pos = 2;
							break;
						case 5:
							pos = 3;
							break;
					}
					break;
			}
		}
		if (update or forward or backward or button) {
			lcd.command(LCDS::CLEAR);
			switch (mode) {
				case 0:
					lcd.writeFlash(PSTR(" Start   Set"));
					lcd.gotoXY(0,1);
					lcd.writeFlash(PSTR(" Cal     Off"));
					lcd.gotoXY((pos%2)*8, pos/2);
					lcd.character('>');
					break;
				case 1:
				case 5:
					lcd.write(timer_min,1);
					lcd.character(':');
					lcd.write(timer_sec,2);
					break;
				case 2:
					lcd.writeFlash(PSTR(" Min  Sec  Save"));
					lcd.gotoXY(1,1);
					lcd.write(minutes,3);
					lcd.gotoXY(6,1);
					lcd.write(seconds,3);
					lcd.gotoXY((pos%3)*5, pos/3);
					lcd.character('>');
					break;
				case 3:
					lcd.writeFlash(PSTR(" Start   Stop"));
					lcd.gotoXY(0,1);
					lcd.writeFlash(PSTR(" Min:    Sec:  "));
					lcd.gotoXY(6, 1);
					lcd.write(minutes, 1);
					lcd.gotoXY(14, 1);
					lcd.write(seconds, 2);
					lcd.gotoXY((pos%2)*8, pos/2);
					lcd.character('>');
					break;
				default:
					lcd.writeFlash(PSTR("Mode "));
					lcd.write(mode, 1);
					break;
			}
			update = false;
			forward = false;
			backward = false;
			button = false;
			_delay_ms(200);
		}
	}
}

ISR(INT0_vect) {
	button = true;
}

ISR(INT1_vect) {
	if (!forward and !backward) {
		if (BITSET(PIND, PD4)) {
			forward = true;
		} else {
			backward = true;
		}
	}
}

ISR(TIMER1_COMPA_vect) {
	if (mode == 1 || mode == 5) {
		timer_sec--;
		if (timer_sec == 1 && timer_min == 0) {
			startBeep();
		}
		if (timer_sec == 0 && timer_min == 0) {
			if (mode == 1) {
				mode = 0;
			}
			if (mode == 5) {
				mode = 3;
			}
			stopBeep();
			turnAllOff();
			// turn all off
		}
		if (timer_sec == 255) {
			timer_min--;
			timer_sec = 59;
		}
		update = true;
	}
}
