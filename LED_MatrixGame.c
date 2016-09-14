#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.c"
#include <stdlib.h>


	unsigned char ALED=0x01;
	unsigned char BLEDmove=0xF7;
	unsigned char tmpC;
	unsigned short count=0;
	unsigned int updownmove=0;
	unsigned char start_game=0;
	int randomval=0;
	int countdisplay=0;
	int count_ticks=0;
	unsigned char score=0;
	unsigned char score10=0;
	unsigned char deathup=0;
	unsigned char deathdown=0;

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;    // Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } //stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		
		// prevents OCR3A from underflowing, using prescaler 64                    // 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }
		
		// set OCR3A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT3 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM3A0);
	// COM3A0: Toggle PB6 on compare match between counter and OCR3A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
	// WGM32: When counter (TCNT3) matches OCR3A, reset counter
	// CS31 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}


void transmit_data_A(unsigned char data) {
	int i;
	for (i = 0; i < 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTA = 0x08;
		// set SER = next bit of data to be sent.
		PORTA |= ((data >> i) & 0x01);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTA |= 0x02;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTA |= 0x04;
	// clears all lines in preparation of a new transmission
	PORTA = 0x00;
}

void transmit_data_B(unsigned char data) {
	int i;
	for (i = 0; i < 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTB = 0x08;
		// set SER = next bit of data to be sent.
		PORTB |= ((data >> i) & 0x01);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTB |= 0x02;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTB |= 0x04;
	// clears all lines in preparation of a new transmission
	PORTB = 0x00;
}


void store_character(unsigned char storage_loc, unsigned char charac[])  //storage location start at 64, increment by 8
{
	LCD_WriteCommand(storage_loc);
	unsigned int i = 0;
	for ( i = 0; i< 8; i++)
	LCD_WriteData(charac[i]);
}
void display_character(unsigned char display_loc, unsigned char stored_char_order) // display location starts at 0x80 ( upper left), increment by 1 ; stored_char_order start by 0 increment by 1
{
	LCD_WriteCommand(display_loc);
	LCD_WriteData(stored_char_order);

}

unsigned char question_mark[] = { 0, 14, 17, 2, 4, 4, 0, 4};
unsigned char sad_face[] = {0, 0, 10, 0, 4, 0, 14, 17};
unsigned char music_sign[] = { 1, 3, 5, 9, 9, 11, 27, 24};
unsigned char block_full[]= {31,31,31,31,31,31,31,31};
unsigned char eye_one[]= {16, 24, 24,28, 28,30,31,31} ;
unsigned char eye_two[]= {1,3,3,7,7,15,31,31};



enum LEDStates { zero,one, two, three, four,five,six,seven} LEDState;
unsigned char tmpA = 0x00;
void LED_Matrix(unsigned char move,unsigned char trap)
{
	switch(LEDState)
	{
		case zero:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one;
		case one:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two;
		case two:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three;
		case three:
			transmit_data_B(0xF7);
			transmit_data_A(trap);
			LEDState=four;
		case four:
			transmit_data_B(0xEF);
			transmit_data_A(trap);
			LEDState=five;
		case five:
			transmit_data_B(0xDF);
			transmit_data_A(trap);
			LEDState=six;
		case six:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=seven;
		case seven:
			transmit_data_B(0xBF);
			transmit_data_A(trap);
			LEDState=zero;
		default:
			LEDState=zero;
	}
}


enum LEDStates1 { zero1,one1, two1, three1, four1,five1,six1,seven1} LEDState1;
void LED_Matrix1(unsigned char move,unsigned char trap)
{
	switch(LEDState1)
	{
		case zero1:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
		LEDState = one1;
		case one1:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
		LEDState = two1;
		case two1:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three1;
		case three1:
			transmit_data_B(0xBF);
			transmit_data_A(trap);
		LEDState=four1;
		case four1:
			transmit_data_B(0xF7);
			transmit_data_A(trap);
			LEDState=five1;
		case five1:
			transmit_data_B(0xFB);
			transmit_data_A(trap);
			LEDState=six1;
		case six1:
			transmit_data_B(0xFD);
			transmit_data_A(trap);
			LEDState=seven1;
		case seven1:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=zero1;
		default:
			LEDState=zero1;
	}
}


enum LEDStates2 { zero2,one2, two2, three2, four2,five2,six2,seven2} LEDState2;
void LED_Matrix2(unsigned char move,unsigned char trap)
{
	switch(LEDState2)
	{
		case zero2:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one2;
		case one2:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two2;
		case two2:
			transmit_data_B(0xDF);
			transmit_data_A(trap);
			LEDState= three2;
		case three2:
			transmit_data_B(0xEF);
			transmit_data_A(trap);
			LEDState=four2;
		case four2:
			transmit_data_B(0xF7);
			transmit_data_A(trap);
			LEDState=five2;
		case five2:
			transmit_data_B(0xFB);
			transmit_data_A(trap);
			LEDState=six2;
		case six2:
			transmit_data_B(0xFD);
			transmit_data_A(trap);
			LEDState=seven2;
		case seven2:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=zero2;
		default:
		LEDState=zero2;
	}
}

enum LEDStates3 { zero3,one3, two3, three3, four3,five3,six3,seven3} LEDState3;
void LED_Matrix3(unsigned char move,unsigned char trap)
{
	switch(LEDState3)
	{
		case zero3:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one3;
		case one3:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two3;
		case two3:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three3;
		case three3:
			transmit_data_B(0xEF);
			transmit_data_A(trap);
			LEDState=four3;
		case four3:
			transmit_data_B(0xF7);
			transmit_data_A(trap);
			LEDState=five3;
		case five3:
			transmit_data_B(0xFB);
			transmit_data_A(trap);
			LEDState=six3;
		case six3:
			transmit_data_B(0xFD);
			transmit_data_A(trap);
			LEDState=seven3;
		case seven3:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=zero3;
		default:
			LEDState=zero3;
	}
}

enum LEDStates4 { zero4,one4, two4, three4, four4,five4,six4,seven4} LEDState4;
void LED_Matrix4(unsigned char move,unsigned char trap)
{
	switch(LEDState4)
	{
		case zero4:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one4;
		case one4:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two4;
		case two4:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three4;
		case three4:
			transmit_data_B(0xBF);
			transmit_data_A(trap);
			LEDState=four4;
		case four4:
			transmit_data_B(0xDF);
			transmit_data_A(trap);
			LEDState=five4;
		case five4:
			transmit_data_B(0xFB);
			transmit_data_A(trap);
			LEDState=six4;
		case six4:
			transmit_data_B(0xFD);
			transmit_data_A(trap);
			LEDState=seven4;
		case seven4:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=zero4;
		default:
			LEDState=zero4;
	}
}

enum LEDStates5 { zero5,one5, two5, three5, four5,five5,six5,seven5} LEDState5;
void LED_Matrix5(unsigned char move,unsigned char trap)
{
	switch(LEDState5)
	{
		case zero5:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one5;
		case one5:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two5;
		case two5:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three5;
		case three5:
			transmit_data_B(0xBF);
			transmit_data_A(trap);
			LEDState=four5;
		case four5:
			transmit_data_B(0xDF);
			transmit_data_A(trap);
			LEDState=five5;
		case five5:
			transmit_data_B(0xEF);
			transmit_data_A(trap);
			LEDState=six5;
		case six5:
			transmit_data_B(0xFD);
			transmit_data_A(trap);
			LEDState=seven5;
		case seven5:
			transmit_data_B(0xFE);
			transmit_data_A(trap);
			LEDState=zero5;
		default:
			LEDState=zero5;
	}
}


enum LEDStates6 { zero6,one6, two6, three6, four6,five6,six6,seven6} LEDState6;
void LED_Matrix6(unsigned char move,unsigned char trap)
{
	switch(LEDState6)
	{
		case zero6:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			LEDState = one6;
		case one6:
			transmit_data_B(0xFF);
			transmit_data_A(0x00);
			transmit_data_A(0x80);
			transmit_data_B(move);
			transmit_data_A(0x00);
			LEDState = two6;
		case two6:
			transmit_data_B(0x7F);
			transmit_data_A(trap);
			LEDState= three6;
		case three6:
			transmit_data_B(0xBF);
			transmit_data_A(trap);
			LEDState=four6;
		case four6:
			transmit_data_B(0xDF);
			transmit_data_A(trap);
			LEDState=five6;
		case five6:
			transmit_data_B(0xEF);
			transmit_data_A(trap);
			LEDState=six6;
		case six6:
			transmit_data_B(0xF7);
			transmit_data_A(trap);
			LEDState=seven6;
		case seven6:
			transmit_data_B(0xFB);
			transmit_data_A(trap);
			LEDState=zero6;
		default:
			LEDState=zero6;
	}
}




void music_play()
{
	LCD_ClearScreen();
	display_character(0x80, 3);							//Display Notes on LCD screen
	display_character(0x82, 3);
	display_character(0x84, 3);
	display_character(0x86, 3);
	display_character(0x88, 3);
	display_character(0x8A, 3);
	display_character(0x8C, 3);
	display_character(0x8E, 3);
	display_character(0xC1, 3);
	display_character(0xC3, 3);
	display_character(0xC5, 3);
	display_character(0xC7, 3);
	display_character(0xC9, 3);
	display_character(0xCB, 3);
	display_character(0xCD, 3);
	display_character(0xCF, 3);
	int notecount=0;
	for (notecount=0;notecount<10000;notecount++)		//plays mario theme variation
		set_PWM(493.88);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(349.23);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(0);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(349.23);
	set_PWM(0);
	for (notecount=0;notecount<20000;notecount++)
		set_PWM(349.23);
	for (notecount=0;notecount<20000;notecount++)
		set_PWM(329.63);
	for (notecount=0;notecount<20000;notecount++)
		set_PWM(293.66);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(523.25);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(329.63);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(0);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(329.63);
	for (notecount=0;notecount<10000;notecount++)
		set_PWM(261.63);
	set_PWM(0);
}

void game_start (unsigned short speed)		//starts up game according to needed speeds
{
	count++;
	updownmove++;
	if (updownmove>=150){
		if((tmpC& 0x10)== 0x10)
		{
			start_game=0;					//resets
			ALED=0x01;
			countdisplay=0;
			count_ticks=0;
		}
		if((tmpC& 0x80)== 0x80)
		{
			if (BLEDmove<0xFE)				//move character up
			BLEDmove=(BLEDmove>>1)|0x80;
			updownmove=0;
		}
		if((tmpC& 0x40)== 0x40)
		{
			if (BLEDmove>0x7F)				//move character down
			BLEDmove=(BLEDmove<<1)|0x01;
			updownmove=0;
		}
	}
	if (count>=speed)
	{
		ALED=ALED<<1;						//shifts lines left
		count=0;
	}
	countdisplay++;
	if(countdisplay>=speed*8)
	{
		set_PWM(0);
		count_ticks++;
		countdisplay--;
		if (count_ticks<=1){						//get random line out of 7 options
			ALED=0x01;
			randomval= random()%7;
		}
		if (randomval==0){
			LED_Matrix(BLEDmove,ALED);				//each lights up 6/8 lights and leaves 2 for player
			deathdown=0xFB;
			deathup=0xFD;
		}
		if (randomval==1){
			LED_Matrix1(BLEDmove,ALED);
			deathdown=0xDF;
			deathup=0xEF;
		}
		if(randomval==2){
			LED_Matrix2(BLEDmove,ALED);
			deathdown=0x7F;
			deathup=0xBF;
		}
		
		if(randomval==3){
			LED_Matrix3(BLEDmove,ALED);
			deathdown=0xBF;
			deathup=0xDF;
		}
		if(randomval==4){
			LED_Matrix4(BLEDmove,ALED);
			deathup=0xF7;
			deathdown=0xEF;
		}
		if(randomval==5){
			LED_Matrix5(BLEDmove,ALED);
			deathup=0xFB;
		deathdown=0xF7;}
		if(randomval==6){
			LED_Matrix6(BLEDmove,ALED);
			deathup=0xFE;
		deathdown=0xFD;}
		if (count_ticks>=speed*8){
			set_PWM(261.63);										//plays noise everytime score++
			score++;
			if(BLEDmove>deathup || BLEDmove<deathdown){				//death
				start_game=0;
				set_PWM(0);
				music_play();
				score--;
			}
			if (score>=10)
			{
				score=0;
				score10++;
			}
			LCD_DisplayString(1, "Score:");
			if (score10>0){											//displays score also fixes when score>10
				LCD_WriteData( score10 + '0');
				LCD_WriteData(score + '0');
			}
			else{
			LCD_WriteData(score + '0');}
			count_ticks=0;
		}
	}
	
}
int main()
{
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0x0F; PORTC = 0xF0;
	DDRD = 0xFF; PORTD = 0x00;

	LCD_init();
	PWM_on();

	store_character(64, block_full);
	store_character(72, eye_one);
	store_character(80, eye_two);
	store_character(88, music_sign);

	display_character(0x80, 3);
	display_character(0x84, 0);
	display_character(0x86, 1);
	display_character(0x88, 2);
	display_character(0x8A, 0);
	
	display_character(0xC5, 0);
	display_character(0xC6, 0);
	display_character(0xC7, 0);
	display_character(0xC8, 0);
	display_character(0xC9, 0);
	display_character(0xCF, 3);
	unsigned char load_lvl=0;
	unsigned char level_pick=1;

	TimerSet(1);
	TimerOn();
	while(1)
	{
		tmpC= ~(PINC) ;
		if (start_game==0)
		{
			load_lvl++;
			transmit_data_A(0x7E);					//initial start of game
			transmit_data_B(0x81);
			score=0;
			score10=0;
			if (load_lvl>=200){						
				if((tmpC& 0x80)== 0x80)			//choose difficulty up and down button
				{
					if (level_pick<4){
						level_pick++;}
						load_lvl=0;
				}
				if((tmpC& 0x40)== 0x40)	{
					if (level_pick>1){
						level_pick--;}
						load_lvl=0;
					}
					if(level_pick==1)				//chooses difficulty(lights of LED).
						PORTA= 0x10;
					if(level_pick==2)
						PORTA= 0x20;
					if(level_pick==3)
						PORTA= 0x30;
					if(level_pick==4){
						PORTA= 0x40;
					}
						
				}
		}
		if((tmpC & 0x20)==0x20)						//game begins after button pressed
		{
			start_game=1;
		}
		if (start_game==1)
		{
			
			if(level_pick==1)
			game_start(175);
			if(level_pick==2)
			game_start(150);
			if(level_pick==3)
			game_start(125);
			if(level_pick==4)
			game_start(90);

		}
		while(!TimerFlag);
		TimerFlag = 0;
		}	
	return 0 ;
}