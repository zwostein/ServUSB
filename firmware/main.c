#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/power.h>

#include <util/delay.h>
#include <util/atomic.h>

#include "usbdrv/usbdrv.h"
#include "servo.h"


#define SERVUSB_REPORT_ID_CONTROL 0x01
#define SERVUSB_REPORT_ID_DATA    0x02

#define SERVUSB_CONTROL_ENABLE_BIT 0x01


PROGMEM const char usbHidReportDescriptor[40] =
{
	0x06, 0x00, 0xff,                // USAGE_PAGE (Generic Desktop)
	0x09, 0x01,                      // USAGE (Vendor Usage 1)
	0xa1, 0x01,                      // COLLECTION (Application)
	0x15, 0x00,                      //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,                //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                      //   REPORT_SIZE (8)
	0x85, SERVUSB_REPORT_ID_CONTROL, //   REPORT_ID (SERVUSB_REPORT_ID_CONTROL)
	0x95, 0x01,                      //   REPORT_COUNT (1)
	0x09, 0x00,                      //   USAGE (Undefined)
	0xb2, 0x02, 0x01,                //   FEATURE (Data,Var,Abs,Buf)
	0x15, 0x00,                      //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,                //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                      //   REPORT_SIZE (8)
	0x85, SERVUSB_REPORT_ID_DATA,    //   REPORT_ID (SERVUSB_REPORT_ID_DATA)
	0x95, 0x01,                      //   REPORT_COUNT (1)
	0x09, 0x00,                      //   USAGE (Undefined)
	0xb2, 0x02, 0x01,                //   FEATURE (Data,Var,Abs,Buf)
	0xc0                             // END_COLLECTION
};


static uint8_t currentReportID = 0;

// called when the host requests a chunk of data from the device
uint8_t usbFunctionRead( uint8_t * data, uint8_t len )
{
	data[0] = currentReportID;
	switch( currentReportID )
	{
	case SERVUSB_REPORT_ID_CONTROL:
		data[1] = 0x00;
		if( servo_isEnabled() )
			data[1] |= SERVUSB_CONTROL_ENABLE_BIT;
		return 2;
	case SERVUSB_REPORT_ID_DATA:
		data[1] = servo_getPosition();
		return 2;
	}
	return 0;
}


// called when the host sends a chunk of data to the device
uint8_t usbFunctionWrite( uint8_t * data, uint8_t len )
{
	if( len < 2 )
		return 0xff; // stall
	switch( currentReportID )
	{
	case SERVUSB_REPORT_ID_CONTROL:
		servo_setEnabled( data[1] & SERVUSB_CONTROL_ENABLE_BIT );
		return 1; // end of transfer
	case SERVUSB_REPORT_ID_DATA:
		servo_setPosition( data[1] );
		return 1; // end of transfer
	}
	return 1; // end of transfer
}


usbMsgLen_t usbFunctionSetup( uint8_t data[8] )
{
	usbRequest_t * rq = (void*)data;
	if( (rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS )
	{ // HID class request
		switch( rq->bRequest )
		{
		case USBRQ_HID_GET_REPORT:
		case USBRQ_HID_SET_REPORT:
			currentReportID = rq->wValue.bytes[0];
			return USB_NO_MSG; // calls usbFunctionRead() on USBRQ_HID_GET_REPORT or usbFunctionWrite() on USBRQ_HID_SET_REPORT
		}
	} else {
		// ignore vendor type requests, we don't use any
	}
	return 0;
}


int main( void )
{
	wdt_disable();
	servo_init();
	usbInit();

	usbDeviceDisconnect(); // enforce re-enumeration, do this while interrupts are disabled!
	uint8_t i = 0;
	while( --i ) // fake USB disconnect for > 250 ms
		_delay_ms(1);
	usbDeviceConnect();

	sei();

	while( 1 )
	{
		usbPoll();
	}

	return 0;
}
