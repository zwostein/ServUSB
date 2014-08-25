#define _BSD_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <argp.h>

#include <libusb.h>


#define SERVUSB_VENDOR_ID  0x16c0
#define SERVUSB_PRODUCT_ID 0x05df

#define SERVUSB_REPORT_ID_CONTROL 0x01
#define SERVUSB_REPORT_ID_DATA    0x02

#define SERVUSB_CONTROL_ENABLE_BIT 0x01


#define USBRQ_HID_GET_REPORT    0x01
#define USBRQ_HID_SET_REPORT    0x09

#define USB_HID_REPORT_TYPE_INPUT   1
#define USB_HID_REPORT_TYPE_OUTPUT  2
#define USB_HID_REPORT_TYPE_FEATURE 3


static char doc[] = "This is the ServUSB command line interface - ServUSB is a servo for the Universal Serial Bus.";


static struct argp_option options[] =
{
	{ "select",  's', "[[bus]:][devnum]", 0, "Connect with specified device and/or bus numbers (in decimal) - Otherwise the first found ServUSB device is used." },
	{ "enable",  'e', "position",         0, "Enable the servo and move to the specified position (0 to 255)." },
	{ "disable", 'd', 0,                  0, "Disables the servo." },
	{ 0 }
};


struct arguments
{
	int bus;
	int dev;
	int enable;
	unsigned int position;
};


static error_t parser( int key, char * arg, struct argp_state * state )
{
	struct arguments * arguments = state->input;

	switch( key )
	{
	case 's':
	{ // shamelessly stolen from usbutil's lsusb.c ;)
		char * cp;
		cp = strchr( arg, ':' );
		if( cp )
		{
			*cp++ = 0;
			if( *arg )
				arguments->bus = strtoul( arg, NULL, 10 );
			if( *cp )
				arguments->dev = strtoul( cp, NULL, 10 );
		} else {
			if( *arg )
				arguments->dev = strtoul(arg, NULL, 10);
		}
		break;
	}
	case 'e':
		arguments->position = atoi( arg );
		arguments->enable = 1;
		break;
	case 'd':
		arguments->enable = 0;
		break;
	case ARGP_KEY_INIT:
		arguments->bus = -1;
		arguments->dev = -1;
		arguments->enable = -1;
		arguments->position = 127;
		break;
	case ARGP_KEY_END:
		if( arguments->enable < 0 )
			argp_failure( state, 1, 0, "Need to either enable or disable the servo!" );
		if( arguments->position < 0 || arguments->position > 255 )
			argp_failure( state, 1, 0, "Servo position out of range (0-255)!" );
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}


static struct argp argp = { options, parser, NULL, doc };


int main( int argc, char ** argv )
{
	// argument parsing
	struct arguments arguments = {0};
	argp_parse( &argp, argc, argv, 0, 0, &arguments );

	// init libusb
	int err;
	libusb_context * ctx;
	err = libusb_init( &ctx );
	if( err )
	{
		fprintf( stderr, "Error: Unable to initialize libusb: %s (%d)\n", libusb_strerror(err), err );
		return EXIT_FAILURE;
	}

	// get USB device
	libusb_device_handle * device = NULL;
	libusb_device ** list;
	ssize_t num_devs = libusb_get_device_list( ctx, &list );
	if( num_devs < 0 )
	{
		err = num_devs;
		fprintf( stderr, "Error: Could not get any devices: %s (%d)\n", libusb_strerror(err), err );
		libusb_exit( ctx );
		return EXIT_FAILURE;
	}
	for( int i = 0; i < num_devs; ++i )
	{
		libusb_device * dev = list[i];
		uint8_t bnum = libusb_get_bus_number( dev );
		uint8_t dnum = libusb_get_device_address( dev );

		if( (arguments.bus != -1 && arguments.bus != bnum) || (arguments.dev != -1 && arguments.dev != dnum))
			continue; // bus and/or device number given and it doesn't match - continue to next device

		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor( dev, &desc );
		if( (desc.idVendor != SERVUSB_VENDOR_ID) || (desc.idProduct != SERVUSB_PRODUCT_ID) )
			continue; // device is not ServUSB

		arguments.bus = bnum;
		arguments.dev = dnum;
		err = libusb_open( dev, &device );
		if( err )
		{
			fprintf( stderr, "Error: Unable to open usb device: %s (%d)\n", libusb_strerror(err), err );
			libusb_free_device_list( list, 0 );
			libusb_exit( ctx );
			return EXIT_FAILURE;
		}
		break;
	}
	libusb_free_device_list( list, 0 );
	if( !device )
	{
		fprintf( stderr, "Error: Could not find ServUSB!\n" );
		libusb_exit( ctx );
		return EXIT_FAILURE;
	}

	// execute command
	if( arguments.enable )
	{
		printf( "Enabling servo on bus %d, device %d and moving into position %d.\n", arguments.bus, arguments.dev, arguments.position );
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_DATA, arguments.position };
			libusb_control_transfer( device,
				LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, // request type
				USBRQ_HID_SET_REPORT,                                                        // request
				USB_HID_REPORT_TYPE_FEATURE << 8 | data[0],                                 // value report type|id
				0,                                                                         // index
				data, sizeof(data),
				1000
				);
		}
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_CONTROL, SERVUSB_CONTROL_ENABLE_BIT };
			libusb_control_transfer( device,
				LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, // request type
				USBRQ_HID_SET_REPORT,                                                        // request
				USB_HID_REPORT_TYPE_FEATURE << 8 | data[0],                                 // value report type|id
				0,                                                                         // index
				data, sizeof(data),
				1000
				);
		}
	} else {
		printf( "Disabling servo on bus %d, device %d.\n", arguments.bus, arguments.dev );
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_CONTROL, 0x00 };
			libusb_control_transfer( device,
				LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, // request type
				USBRQ_HID_SET_REPORT,                                                        // request
				USB_HID_REPORT_TYPE_FEATURE << 8 | data[0],                                 // value report type|id
				0,                                                                         // index
				data, sizeof(data),
				1000
				);
		}
	}

	libusb_close( device );
	libusb_exit( ctx );
	return EXIT_SUCCESS;
}
