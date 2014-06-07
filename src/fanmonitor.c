#include "fanmonitor.h"


/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

const PROGMEM char usbHidReportDescriptor[22] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x80,                    //   REPORT_COUNT (128)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};
/* Since we define only one feature report, we don't use report-IDs (which
 * would be the first byte of the report). The entire report consists of 128
 * opaque data bytes.
 */


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// The following variables store the status of the current data transfer
static uchar    currentAddress	= 0;
static uchar    bytesRemaining	= 0;
static int		currentCpuTemp	= 0;
static int		previousCpuTemp	= 0;
static uchar	getTemp			= 0;


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// usbFunctionRead() is called when the host requests a chunk of data from
// the device. For more information see the documentation in usbdrv/usbdrv.h.
uchar usbFunctionRead(uchar *data, uchar len)
{
    if(len > bytesRemaining)
        len = bytesRemaining;
    eeprom_read_block(data, (uchar *)0 + currentAddress, len);
    currentAddress += len;
    bytesRemaining -= len;
    return len;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// usbFunctionWrite() is called when the host sends a chunk of data to the
// device. For more information see the documentation in usbdrv/usbdrv.h.
uchar usbFunctionWrite(uchar *data, uchar len)
{
	// end of transfer
    return 1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *)data;

    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
    {
    	// HID class request
        if (rq->bRequest == USBRQ_HID_GET_REPORT)
        {
        	// wValue: ReportType (highbyte), ReportID (lowbyte)
            //since we have only one report type, we can ignore the report-ID
            bytesRemaining = 128;
            currentAddress = 0;
            return USB_NO_MSG;  // use usbFunctionRead() to obtain data
        }
        else if (rq->bRequest == USBRQ_HID_SET_REPORT)
        {
            // since we have only one report type, we can ignore the report-ID
            bytesRemaining = 128;
            currentAddress = 0;
            return USB_NO_MSG; // use usbFunctionWrite() to receive data from host
    	}
    }
    else
    {
        // ignore vendor type requests, we don't use any
    }
    return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// get the current CPU temperature
int getCpuTemp(void)
{
	// start a conversion then sleep
	ADCSRA |= (1<<ADSC);

	sleep_cpu();

	getTemp = 0;

	return currentCpuTemp;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// USB_RESET_HOOK
void usbEventResetReady(void)
{
	//NO-OP
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// initialize the ADC
void initADC(void)
{
	//
	// Prepare sleep mode for ADC conversions
	// pg 
	sleep_enable();
	set_sleep_mode(SLEEP_MODE_ADC);

	//
	// Prepare ADC for temperature measurement (pg 262)
	// pg 263
	power_adc_enable();
	ADMUX	=	(1<<REFS1)	|	// REFS2:0 = 010 for Internal 1.1V Voltage Reference with external capacitor at AREF pin
				(1<<REFS0)	|
				(1<<ADLAR)	|	// left adjust so we can just read ADCH
				(0<<0)		|
				(1<<MUX3)	|	// MUX3:0 = 1000 for internal temp sensor
				(0<<MUX2)	|
				(0<<MUX1)	|
				(0<<MUX0);

	ADCSRA	=	(1<<ADEN)	|	// enable adc
				(1<<ADSC)	|	// start a conversion
				(0<<ADATE)	|	// no auto-trigger
				(1<<ADIF)	|	// clear interrupt flag
				(1<<ADIE)	|	// enable interrupts
				(1<<ADPS2)	|	// set ADC prescaler to a factor 128
				(1<<ADPS1)	|
				(1<<ADPS0);

	ADCSRB	=	(0<<0)		|	
				(0<<ACME)	|	// no auto-trigger
				(0<<0)		|
				(0<<0)		|
				(0<<0)		|
				(0<<ADTS2)	|
				(0<<ADTS1)	|
				(0<<ADTS0);

	/*
	From page 262:

	ADC = (Vin * 1024) / Vref

	Table 23-2. Temperature vs. Sensor Output Voltage (Typical Case)

	Temperature / °C		-45°C		+25°C		+85°C
	Voltage / mV			242 mV		314 mV		380 mV
	ADC Value (10bit)		225			292			354

	Values below 292 yield lowest fan speed
	PWM speed increases to max at 354
	*/
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// initialize the USB hardware
void initUSB(void)
{
    // enforce re-enumeration
    usbDeviceDisconnect();
	uchar i;
    for (i = 0; i<20; i++) // wait 300 ms
        _delay_ms(15);
    usbDeviceConnect();

    usbInit();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// prepare hardware
void init(void)
{
	// enable timer1
	power_timer1_enable();

	// prepare timer0 for PWM output on OC0A (PD6)
	TCCR0A	=	(1<<COM0A1)	|	// Clear OC0A on Compare Match, set OC0A at BOTTOM
				(0<<COM0A0)	|
				(0<<COM0B1)	|
				(0<<COM0B0)	|
				(0<<0)		|
				(0<<0)		|
				(1<<WGM01)	|	// FAST-PWM
				(1<<WGM00);

	TCCR0B	=	(0<<FOC0A)	|
				(0<<FOC0B)	|
				(0<<0)		|
				(0<<0)		|
				(0<<WGM02)	|
				(0<<CS02)	|	// clk/1/256 (20MHz/1/256 = 78.125KHz PWM carrier wave)
				(0<<CS01)	|
				(1<<CS00);

	OCR0A	= 0x50;

	// set PWM pin as output
	PWM_DDR	|= (1<<PWM_OUT);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// 
int main(void)
{
    // init main hardware
	init();

	// init USB interface
    initUSB();

	initADC();

    sei();

    // enable 1s watchdog timer
    wdt_enable(WDTO_1S);

	// enable debug led
    DBG_DDR	|=	(1<<DBG_LED);
    DBG_PRT	&=	~(1<<DBG_LED);

    while(1)
    {
		wdt_reset(); // keep the watchdog happy
        usbPoll();
    }

	return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ADC conversion ready interrupt handler
ISR(ADC_vect)
{
	previousCpuTemp = currentCpuTemp;
	currentCpuTemp = ADCH;
}

