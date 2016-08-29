/*---------------------------------------------------------------------------------------
copyright 2011
Project: ECE353 lab D
Authors: Christopher Finn; Paulo Leal; Shuwen Cao

Description: General MIDI Explorer (GME) with Record/Playback
-----------------------------------------------------------------------------------------
Revision History  5.0
-----------------------------------------------------------------------------------------
Date              Person                     Description
-----------------------------------------------------------------------------------------
Nov/30/2011     Shuwen Cao 		           Version 1.0   (wired up the board)
Dec/02/2011     Chris Finn                 Version 2.0   (Wrote initial test code to test switches & LED output)
Dec/03/2011     Chris Finn, Paulo Leal     Version 3.0   (Wrote USART functions and logic)
Dec/04/2011     Chris Finn, Paulo Leal     Version 4.0   (Wrote EEPROM functions and logic)
Dec/05/2011     Chris F, Paulo L, Shuwen C Final Version (Implemented timing & finalized code)

---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 4000000UL
#include <util/delay.h>
#define BAUDRATE 31250
#define UBRRVAL (((F_CPU / (BAUDRATE * 16UL))) - 1)
#define NOTEON_START 0x90
#define NOTEOFF_START 0x80
#define NOTEON_END 0x64
#define NOTEOFF_END 0x40


/************************************
 ** Interrupt Function
 ** - Called when TIMER1 overflows
 ************************************/
ISR(TIMER1_OVF_vect) 
{
}

/********************
 ** Intializes USART
 ********************/
void USART_Init(unsigned int baud)
{
	////////////////
   	// Set Baud Rate
   	////////////////
	UBRRH = (unsigned char)(baud>>8);
	UBRRL = (unsigned char)baud;

	//////////////////////////////////////
	// Enable The receiver and transmitter
	//////////////////////////////////////
	UCSRB = (1<<RXEN)|(1<<TXEN);

	///////////////////
	// Set Frame Format
	///////////////////
   	// -Asynchronous mode
   	// -No Parity
   	// -1 StopBit
	// -Char size 8
	UCSRC = (1<<URSEL)|(0<<USBS)|(3<<UCSZ0);
}

/*********************************
 ** Waits for data & then reads it
 *********************************/
unsigned char USART_Receive()
{
	while (!(UCSRA & (1<<RXC)))
	{
		if((PINB & 0x02) == 0 || (PINB & 0x01) != 0)
		{
			return 0;
		}
	}
	return UDR;
}

/******************************
 ** Waits for transmitter to be
 ** ready & then transmits data
 ******************************/
void USART_Transmit( unsigned char data )
{
	while ( !( UCSRA & (1<<UDRE)) )
		;
	UDR = data;
}

/*********************************
 ** Flushes the USART
 *********************************/
void USART_Flush( void )
{
	unsigned char dummy;
	while ( UCSRA & (1<<RXC) ) dummy = UDR;
}

/************************************
 ** Waits for completion of previous
 ** write, sets up address & data register,
 ** writes a logical 1 to EEMWE & starts
 ** eeprom write by setting EEWE
 ************************************/
void EEPROM_write(unsigned int address, unsigned char data)
{
	while(EECR & (1<<EEWE))
	{}
	EEAR = address;
	EEDR = data;
	EECR |= (1<<EEMWE);
	EECR |= (1<<EEWE);
}

/************************************
 ** Waits for completion of previous
 ** write, sets up address register,
 ** starts eeprom read by writing
 ** EERE & returns data register data
 ************************************/
unsigned char EEPROM_read(unsigned int address)
{
	while(EECR & (1<<EEWE))
	{}
	EEAR = address;
	EECR |= (1<<EERE);
	return EEDR;
}


int main()
{
	USART_Init(UBRRVAL);
	int Record=0;
	int Playback=0;
	int dataStored = 0;
	int write_Enable = 0;
	unsigned int playbackAddress = 0;
	unsigned int address =0;
	///////////////////////////
    // 16 bit timer initialize
	///////////////////
	TIMSK |= (1<<TOIE1); 						// enabled global and timer overflow interrupt
	TCCR1A = 0x00; 								// normal operation (mode0)
	TCNT1=0x0000; 								// set timer to 0
	TCCR1B |= ((0<<CS11)|(1<<CS12)|(1<<CS10)); 	// set prescaler to 1024
	sei();
	//////////////////
    // Set Output Port
    ////////////
	PORTA=0x00;
 	DDRA=0xFF;
	//////////////////
    // Set Input Port
    ////////////
    PORTB=0x00;
    DDRB=0x00;


	unsigned char Midi[2];
	for(int i =0; i<2;i++)
	{
			Midi[i] = 0x00;
	}

	////////////////////
    // Main program loop
    //////////////
	while(1)
	{
	    //////////////////////
        // Get switch statuses
        //////////////
		Record = (PINB & 0x02);
		Playback = (PINB & 0x01);
		////////////
		// RECORD
		/////////
		if(Record != 0 && Playback == 0){
			for(int i =0; i<2;i++)
			{
				unsigned char temp = USART_Receive();
				if(temp != 0){
					Midi[i] = temp;
					write_Enable = 1;
				}
			}
			USART_Flush();
			if(write_Enable != 0)
			{
				if(address < 1020)
				{
					if(address != 0)
					{
						cli();
						unsigned int low = TCNT1L;
						unsigned int high = TCNT1H;
						sei();
						EEPROM_write(address, high);
						address++;
						EEPROM_write(address, low);
						address++;
					}
					///////////////////////////////////////////////////
					// Compress here by not writing last byte of packet
					///////////////////////////////////////////////
					for(int i=0;i<2;i++)
					{
						EEPROM_write(address, Midi[i]);
						address++;

					}
					dataStored++;
					TCNT1 = 0;
				}
				write_Enable = 0;
			}
		}
		//////////////
		// PLAYBACK
		//////////
		if(Record == 0 && Playback != 0){
			while(dataStored != 0){
				if(playbackAddress != 0)
				{
					///////////
					// DELAY
					///////
					unsigned int delayH = EEPROM_read(playbackAddress);
					playbackAddress++;
					address--;
					unsigned int delayL = EEPROM_read(playbackAddress);
					playbackAddress++;
					address--;
					unsigned int delay = (delayH << 8) + delayL;
					TCNT1 = 0;
					while(TCNT1 < delay)
					{}
				}
				unsigned char fb = EEPROM_read(playbackAddress);
				USART_Transmit(fb);
				playbackAddress++;
				address--;
				USART_Transmit(EEPROM_read(playbackAddress));
				playbackAddress++;
				address--;
				///////////////////////////////////////////////////
				// Decompress here by reconstructing the last byte
				// of the note packet by analyzing the first byte
				// and using the following pattern:
				// --------------
				// first | third
				// --------------
				//  0x90 | 0x64
				//  0x80 | 0x40
				///////////////////////////////////////////////
				if(fb == NOTEON_START)
					USART_Transmit(NOTEON_END);
				if(fb == NOTEOFF_START)
					USART_Transmit(NOTEOFF_END);
				dataStored--;
			}
			playbackAddress = 0;
		}
		USART_Flush();
	}
}
