#define _BSD_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <getopt.h>
#include <libusb.h>


#define SERVUSB_VENDOR_ID  0x16c0
#define SERVUSB_PRODUCT_ID 0x05df

#define SERVUSB_CONFIGURATION 1
#define SERVUSB_INTERFACE 0

#define SERVUSB_REPORT_ID_CONTROL 0x01
#define SERVUSB_REPORT_ID_DATA    0x02

#define SERVUSB_CONTROL_ENABLE_BIT 0x01


#define USBRQ_HID_GET_REPORT    0x01
#define USBRQ_HID_SET_REPORT    0x09

#define USB_HID_REPORT_TYPE_INPUT   1
#define USB_HID_REPORT_TYPE_OUTPUT  2
#define USB_HID_REPORT_TYPE_FEATURE 3


static int usb_setFeature( libusb_device_handle * device, unsigned char * data, uint16_t length )
{
	int transferred = libusb_control_transfer( device,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, // request type
		USBRQ_HID_SET_REPORT,                                                        // request
		USB_HID_REPORT_TYPE_FEATURE << 8 | data[0],                                 // value report type|id
		0,                                                                         // index
		data, length,
		1000
		);
	if( transferred < 0 )
	{
		fprintf( stderr, "Error: Transfer failed: %s (%d)\n", libusb_strerror(transferred), transferred );
		return transferred;
	}
	if( transferred != length )
	{
		fprintf( stderr, "Error: Incomplete transfer - sent %d bytes but expected %d\n", transferred, length );
		return LIBUSB_ERROR_IO;
	}
	return transferred;
}


struct arguments
{
	int bus;
	int dev;
	int enable;
	unsigned int position;
};


void print_usage( int argc, char ** argv )
{
	printf
	(
		"This is the ServUSB command line interface - ServUSB is a servo for the Universal Serial Bus.\n"
		"Usage: %s [-d] [--disable] [-e position] [--enable=position] [-s [[bus]:][devnum]] [--select=[[bus]:][devnum]]\n",
		argv[0]
	);
}

int main( int argc, char ** argv )
{
	// argument parsing
	struct arguments arguments = {0};
	arguments.bus = -1;
	arguments.dev = -1;
	arguments.enable = -1;
	arguments.position = 127;

	static struct option long_options[] =
	{
		{ "disable", no_argument,       0, 'd' },
		{ "enable",  required_argument, 0, 'e' },
		{ "select",  required_argument, 0, 's' },
		{ 0,         0,                 0, 0   }
	};

	int opt = 0;
	int option_index = 0;
	while( ( opt = getopt_long( argc, argv, "de:s:", long_options, &option_index ) ) != -1 )
	{
		switch( opt )
		{
		case 'd':
			arguments.enable = 0;
			break;
		case 'e':
			arguments.position = atoi( optarg );
			arguments.enable = 1;
			break;
		case 's':
		{ // shamelessly stolen from usbutil's lsusb.c ;)
			char * cp;
			cp = strchr( optarg, ':' );
			if( cp )
			{
				*cp++ = 0;
				if( *optarg )
					arguments.bus = strtoul( optarg, NULL, 10 );
				if( *cp )
					arguments.dev = strtoul( cp, NULL, 10 );
			} else {
				if( *optarg )
					arguments.dev = strtoul( optarg, NULL, 10 );
			}
			break;
		}
		default:
			print_usage( argc, argv );
			return EXIT_FAILURE;
		}
	}
	if( arguments.enable < 0 )
	{
		fprintf( stderr, "Need to either to enable or disable the servo!\n" );
		return EXIT_FAILURE;
	}
	if( arguments.position < 0 || arguments.position > 255 )
	{
		fprintf( stderr, "Servo position out of range (0-255)!\n" );
		return EXIT_FAILURE;
	}

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
			continue; // bus and/or device number given and it doesn't match - continue with next device

		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor( dev, &desc );
		if( (desc.idVendor != SERVUSB_VENDOR_ID) || (desc.idProduct != SERVUSB_PRODUCT_ID) )
			continue; // device is not ServUSB - continue with next device

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
		if( arguments.dev < 0 && arguments.bus < 0 )
			fprintf( stderr, "Error: Could not find ServUSB!\n" );
		else
			fprintf( stderr, "Error: Could not find ServUSB on bus %d device %d!\n", arguments.bus, arguments.dev );
		libusb_exit( ctx );
		return EXIT_FAILURE;
	}

	libusb_detach_kernel_driver( device, SERVUSB_INTERFACE );
	err = libusb_set_configuration( device, SERVUSB_CONFIGURATION );
	if( err )
	{
		fprintf( stderr, "Warning: Could not set configuration: %s (%d)\n", libusb_strerror(err), err );
	}
	err = libusb_claim_interface( device, SERVUSB_INTERFACE );
	if( err )
	{
		fprintf( stderr, "Warning: Could not claim interface: %s (%d)\n", libusb_strerror(err), err );
	}

	// execute command and exit
	int transferred;
	if( arguments.enable )
	{
		printf( "Enabling servo on bus %d, device %d and moving into position %d.\n", arguments.bus, arguments.dev, arguments.position );
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_DATA, arguments.position };
			transferred = usb_setFeature( device, data, sizeof(data) );
			if( transferred < 0 )
			{
				fprintf( stderr, "Error: Failed to set position!\n" );
				libusb_close( device );
				libusb_exit( ctx );
				return EXIT_FAILURE;
			}
		}
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_CONTROL, SERVUSB_CONTROL_ENABLE_BIT };
			transferred = usb_setFeature( device, data, sizeof(data) );
			if( transferred < 0 )
			{
				fprintf( stderr, "Error: Failed to enable servo!\n" );
				libusb_close( device );
				libusb_exit( ctx );
				return EXIT_FAILURE;
			}
		}
	} else {
		printf( "Disabling servo on bus %d, device %d.\n", arguments.bus, arguments.dev );
		{
			unsigned char data[2] = { SERVUSB_REPORT_ID_CONTROL, 0x00 };
			transferred = usb_setFeature( device, data, sizeof(data) );
			if( transferred < 0 )
			{
				fprintf( stderr, "Error: Failed to disable servo!\n" );
				libusb_close( device );
				libusb_exit( ctx );
				return EXIT_FAILURE;
			}
		}
	}

	libusb_close( device );
	libusb_exit( ctx );
	return EXIT_SUCCESS;
}
