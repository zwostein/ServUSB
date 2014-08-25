#include "servo.h"

#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>


#define CPU_CYCLE_S             ( 1.0 / (F_CPU) )               // The time for one CPU cycle in seconds
#define SERVO_CYCLE_S           ( 0.02 )                        // Time between servo updates in seconds (nominal 20ms)
#define SERVO_CPU_CYCLES        ( SERVO_CYCLE_S / CPU_CYCLE_S ) // The number of CPU cycles passing between two servo updates
#define SERVO_MIN_S             ( 0.0008 )                      // Minimum pulse length in seconds
#define SERVO_MAX_S             ( 0.00216 )                     // Maximum pulse length in seconds
#define SERVO_MIN_CPU_CYCLES    ( SERVO_MIN_S / CPU_CYCLE_S )   // Minimum pulse length in CPU cycles
#define SERVO_MAX_CPU_CYCLES    ( SERVO_MAX_S / CPU_CYCLE_S )   // Maximum pulse length in CPU cycles

#define SERVO_CPU_CYCLES_2048   ( SERVO_CPU_CYCLES / 2048 )     // Using a prescaler of 2048
#define SERVO_MIN_CPU_CYCLES_64 ( SERVO_MIN_CPU_CYCLES / 64 )   // Using a prescaler of 64
#define SERVO_MAX_CPU_CYCLES_64 ( SERVO_MAX_CPU_CYCLES / 64 )   // Using a prescaler of 64


static volatile uint8_t scaledPosition = 0;


void servo_init( void )
{
	DDRB |= _BV(DDB0); // servo pin as output (OC0A)

	// Timer/Counter1 - Generates servo update interrupts (every SERVO_CYCLE_S)
	TCCR1 = 0
	      | _BV(CTC1)              // Clear Timer/Counter on Compare Match (with OCR1C)
	      | _BV(CS13) | _BV(CS12)  // Prescaler: CK/2048
	      ;
	OCR1A = SERVO_CPU_CYCLES_2048; // Compare match on SERVO_CPU_CYCLES (match triggers interrupt)
	OCR1C = SERVO_CPU_CYCLES_2048; // Compare match on SERVO_CPU_CYCLES (match causes reset)
//	TIMSK |= _BV(OCIE1A);          // enable interrupt

	// Timer/Counter0 - Generates a pulse on the servo pin (between SERVO_MIN_CPU_CYCLES and SERVO_MAX_CPU_CYCLES)
	TCCR0A = 0
	       | _BV(WGM01)            // CTC (TOP = OCRA)
	       ;
	TCCR0B = 0                     // initially disabled (no clock source)
	       ;
	TIMSK |= _BV(OCIE0A);          // enable interrupt
}


void servo_setPosition( uint8_t position )
{
	scaledPosition = ( (uint16_t)(SERVO_MAX_CPU_CYCLES_64 - SERVO_MIN_CPU_CYCLES_64) * (uint16_t)position ) / 255;
}


uint8_t servo_getPosition( void )
{
	return (scaledPosition * 255) / (uint16_t)(SERVO_MAX_CPU_CYCLES_64 - SERVO_MIN_CPU_CYCLES_64);
}


// Timer/Counter1 Compare Match A interrupt - called each servo update
ISR( TIM1_COMPA_vect )
{
	// start pulse
	PORTB  |= _BV(PB0);              // set servo pin - timer0 interrupt will clear it
	TCCR0B |= _BV(CS01) | _BV(CS00); // enable timer0 by setting prescaler to CK/64
}


// Timer/Counter0 Compare Match A interrupt - servo pulse generator
ISR( TIM0_COMPA_vect )
{
	#define MODE_BEGIN 0 // wait SERVO_MIN_CPU_CYCLES_64
	#define MODE_PULSE 1 // wait additional time to encode position (scaledPosition)

	static uint8_t mode = MODE_BEGIN;

	// switch to next mode
	switch( mode )
	{
	case MODE_BEGIN: // waited for SERVO_MIN_CPU_CYCLES_64 - now wait for scaledPosition
		mode = MODE_PULSE;
		OCR0A = scaledPosition;
		break;
	case MODE_PULSE: // pulse completed - clear servo pin, stop timer and prepare for next pulse
		PORTB &= ~_BV(PB0);
		mode = MODE_BEGIN;
		// prepare for next interrupt
		TCCR0B &= ~( _BV(CS01) | _BV(CS00) | _BV(CS02) ); // disable timer0 (no clock source)
		OCR0A = SERVO_MIN_CPU_CYCLES_64;                  // delay for SERVO_MIN_CPU_CYCLES_64 when timer reenables
		TCNT0 = 0;                                        // start counting from zero again
		break;
	}

	#undef MODE_BEGIN
	#undef MODE_PULSE
}
