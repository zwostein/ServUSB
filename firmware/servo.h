#ifndef _SERVO_H_
#define _SERVO_H_


#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>


void servo_init( void );

void servo_setPosition( uint8_t position );
uint8_t servo_getPosition( void );


static inline void servo_enable( void )
{
	TIMSK |= _BV(OCIE1A);
}


static inline void servo_disable( void )
{
	TIMSK &= ~_BV(OCIE1A);
}


static inline bool servo_isEnabled( void )
{
	return TIMSK & _BV(OCIE1A);
}


static inline void servo_setEnabled( bool enabled )
{
	if( enabled )
		servo_enable();
	else
		servo_disable();
}


#endif
