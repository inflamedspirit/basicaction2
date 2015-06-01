#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
// F_CPU in makefile

// Pin defintions
#define MOT1 PB0 // motor1
#define MOT2 PB1 // motor2
#define IRIN PB2 // ir detector
#define LED1 PB3 // led1
#define LED2 PB4 // led2
#define RSTB PB5 // reset pin

// IR Pulse definitions
#define THRESH_LONG   30
#define THRESH_HL     15
#define THRESH_SHORT  5
#define PULSE_LONG    0
#define PULSE_HIGH    1
#define PULSE_LOW     2
#define PULSE_SHORT   3
#define TEST_PULSE(x) (x>THRESH_SHORT?(x>THRESH_HL?(x>THRESH_LONG?PULSE_LONG:PULSE_HIGH):PULSE_LOW):PULSE_SHORT)

// remote command decoding (all should be in reverse order)
#define KEY_FORWARD (0b0010010001010111)
#define KEY_REWIND  (0b0010110001010111)
#define KEY_PLAY    (0b0000010001010111)
#define KEY_STOP    (0b0001010001010111)


// IR commands variables
uint16_t irdata[100];
volatile uint8_t irindex=0;
uint8_t irsaved=0;

// Timing variables
volatile uint16_t t=0;
uint16_t prevtime = 0;

// Command/State variables
uint16_t tempcommand = 0;
uint16_t command = 0;
uint8_t  marker = 0;


// INT0 interrupt, don't do anything if buffer is full
ISR(INT0_vect)
{
  if( irindex < 100 ){
    irdata[irindex++] = t;
  }
}

// TIMER1 interrupt, simply count.
ISR(TIMER1_COMPA_vect)
{
  t++;
}


int main () {

  // Setup registers
  GIMSK |= _BV(INT0);  // enable INT0 interrupt
  MCUCR |= _BV(ISC00); // Interrupt on logical change of INT0

  TCCR1 |= _BV(CS10);  // no prescaler on timer1
  TCCR1 |= _BV(CTC1);  // clear timer1 on OCR1C match

  TIMSK |= _BV(OCIE1A); // Enable compare register A interrupt
  OCR1C = 100;          // sets the max value which resets the counter
  OCR1A = 100;          // actually generates the interrupt

  DDRB = (_BV(MOT1)|_BV(MOT2)|_BV(LED1)|_BV(LED2)); // setups pins
  PORTB = 0;                                        // set LED pin LOW, and turn off pullups

  // start everything at 0 to make sure...
  irindex=0;
  irsaved=0;

  _delay_ms(1000);   // wait a second

  while (1) {

    sei();

    // If buffer is full, do commands parsing
    if( irindex == 100 ) {
      cli();
      while( irindex != irsaved ){

	// check current pulse
	switch(TEST_PULSE(irdata[irsaved]-prevtime)){
	case PULSE_SHORT:
	  // ignore short pulses
	  break;
	case PULSE_HIGH:
	  // write a 1 and increment
	  tempcommand |= (1<<marker++);
	  break;
	case PULSE_LOW:
	  // 'write' a 0 and increment
	  marker++;
	  break;
	case PULSE_LONG:
	  // reset command
	  tempcommand = 0;
	  marker = 0;
	}

	// command buffer full, execute command
	if( marker == 16 ){
	  command = tempcommand;
	  marker=0;
	  tempcommand=0;
	}

	prevtime = irdata[irsaved++];
      }

      // reset buffer
      irindex = 0;
      irsaved = 0;
      
      sei();
    }
 
    int i;
    // execute last command
    switch(command){
      case KEY_FORWARD:
	// move left
	PORTB = _BV(LED2);
	break;
      case KEY_REWIND:
	// move right
	PORTB = _BV(MOT1)|_BV(LED1)|_BV(MOT2);
	break;
      case KEY_PLAY:
	// move forward
	PORTB = _BV(MOT1)|_BV(LED1)|_BV(LED2);
	break;
      case KEY_STOP:
	// stop and blink
	for( i=0; i<4; i++){
	  PORTB = _BV(LED1)|_BV(LED2)|_BV(MOT2);
	  _delay_ms(50);
	  PORTB = _BV(MOT2);
	  _delay_ms(50);
	}
	break;
      default:
	// blink a couple until first command
	PORTB = _BV(LED1)|_BV(MOT2);
	_delay_ms(400);
	PORTB = _BV(MOT2);
	_delay_ms(400);
      };

  
  }

  return 0;
}
