/*
   avrTWI.c - I2C for atmega
   Copyright (C) 2017  W Mead <ironapp-pennAuto@yahoo.com>

  LICENCE
  -------
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#include "avrTWI.h"

static volatile uint8_t twi_buffer[128];
static volatile uint8_t twi_pointer;
static volatile uint8_t twi_length;

#define  MAX_TWI_TRY    0x14

/* I2C clock in Hz */
//#define SCL_CLOCK       100000L
#define SCL_CLOCK       400000L

/* I2C timer max delay */
#define TWI_TIMER_DELAY 0xFF

//
void twi_init(void)
{
	// SCL freq = (CPU Clock) / (16 + 2(TWBR) * (prescale value))
	// 400 kHz = (16MHz) / (16 + 2(12) * (1))

	TWSR = 0;                         /* no prescaler */
    TWBR = ((F_CPU/SCL_CLOCK)-16)/2;  /* must be > 10 for stable operation */
/*
    TWCR - Control register
	  TWEN           // TWI enable interface and release TWI pins.
	  TWIE           // TWI interrupt.
      TWINT          // TWI interrupt flag
	  TWEA           // TWI Enable acknowledge
	  TWSTA          // TWI Start condition
	  TWSTO          // TWI Stop condition
	  TWWC           // TWI Write condition
*/
}

// From atmega 328 datasheet (page 216)
unsigned char twi_start(unsigned char sAddress, uint8_t twiRW)
{
	uint32_t twi_timer = 0 ;

	// Send START condition
	TWCR = (1 << TWEN) |        // Enable interface and release TWI pins.
	       (1 << TWINT) |       // Clear the interrupt flag.
	       (1 << TWSTA) ;       // Enable start condition.

	// Wait for TWINT flag set
	twi_timer = TWI_TIMER_DELAY ;
	while(!(TWCR & (1<<TWINT)) && twi_timer--) ;
	  if(twi_timer==0) return 9 ;

    // Check TWI status (Start and repeat start)
	if(((TWSR & TW_NO_INFO) != TW_START) &&
	   ((TWSR & TW_NO_INFO) != TW_REP_START)) return 8 ;

	//Slave address (Value on datasheet shifted to the left)
	TWDR = sAddress<<1 ;
	//+Read or +Write
	TWDR |= twiRW ;
	TWCR = (1<<TWINT) |
	       (1<<TWEN) ;

    // Wait for TWINT flag set
	twi_timer = TWI_TIMER_DELAY ;
	while(!(TWCR & (1<<TWINT)) && twi_timer--) ;
	  if(twi_timer==0) return 7 ;

    // Check TWI status (Write and Read acknowledge)
	if(((TWSR & TW_NO_INFO) != TW_MT_SLA_ACK) &&
	   ((TWSR & TW_NO_INFO) != TW_MR_SLA_ACK)) return 6 ;

    //Ready to transmit data
	return 0 ;

}


void twi_stop(unsigned char sAddress)
{
	uint32_t twi_timer = 0 ;

	TWCR = (1 << TWEN) |        // Enable interface and release TWI pins.
	       (1 << TWINT) |       // Clear the interrupt flag.
	       (1 << TWSTO) ;       // Enable stop condition.

	twi_timer = TWI_TIMER_DELAY ;
	while((TWCR & (1<<TWSTO)) && twi_timer--) ;

}

/*************************************************************************
 Read one byte from the I2C device, read is followed by a stop condition

 Return:  byte read from I2C device
*************************************************************************/
unsigned char twi_readNak(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN);
	while(!(TWCR & (1<<TWINT)));

    return TWDR;

}/* i2c_readNak */


/*************************************************************************
  Send one byte to I2C device

  Input:    byte to be transfered
  Return:   0 write successful
            1 write failed
*************************************************************************/
unsigned char twi_write_byte(unsigned char data)
{
    uint8_t   twst;

	// send data to the previously addressed device
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);

	// wait until transmission completed
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits
	twst = TW_STATUS & 0xF8;
	if( twst != TW_MT_DATA_ACK) return 1;
	return 0;

}/* i2c_write */


