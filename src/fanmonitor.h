#ifndef __FANMONITOR__
#define __FANMONITOR__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <avr/eeprom.h>
#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include <avr/sleep.h>

#include <usbdrv/usbdrv.h>



#define PWM_DDR		DDRD
#define PWM_PRT		PORTD
#define	PWM_OUT		PD6

#define DBG_DDR		DDRB
#define DBG_PRT		PORTB
#define DBG_LED		PB0
//#define USB_D+	PB0
//#define USB_D-	PB1


#endif
