/*

 * main.c
 *
 *  Created on: 20. 11. 2017
 *      Author: Milan Hruban (xhruba08)
 *      Original
 */


#include "MK60D10.h"
#include "stdio.h"
#include "ctype.h"
#include "string.h"
#include "time.h"

#include "starWarsFont.c";

// Mapping of LEDs and buttons to specific port pins:
#define LED_D9  0x20 // Port B, bit 5
#define LED_D10 0x10 // Port B, bit 4
#define LED_D11 0x8 // Port B, bit 3
#define LED_D12 0x4 // Port B, bit 2

#define BTN_SW2 0x400 // Port E, bit 10
#define BTN_SW3 0x1000 // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800 // Port E, bit 11

const int secondsInDay = 86400;
// type of alarm
unsigned int lightType = 1;
unsigned int soundType = 1;
unsigned int lightTypeCount = 3;
unsigned int soundTypeCount = 3;

unsigned int fancyClock = 0;

unsigned int alarmOn = 0;
unsigned int alarmRinging = 0;
unsigned int alarmRepetitions = 0;
unsigned int alarmRepetitionsPerformed = 0;
unsigned int alarmRepetitionDelay = 300;
unsigned int alarmTimeSeconds = 0;

// index of item that user's arrows points at
unsigned int menuItemArrow = 0;
unsigned int menuItemCount = 8;

// user input
unsigned int maxInputLength = 4;
unsigned char userInput[4];

// A delay function
void delay(long long bound) {
  long long i;
  for(i=0;i<bound;i++);
}
// beep
void beep(void) {
int q;
	for (q=0; q<500; q++) {
    	GPIOA->PDOR ^= 0x10;
    	delay(500);
    	GPIOA->PDOR ^= 0x10;
    	delay(500);
    }
}
// play alarm sound that is set
void playAlarmSound(void){
	SendString("Alarm sound playing");
}
// player alarm light notification that is set
void playAlarmLight(void){
	SendString("Alarm light playing");
}
// alarms
void performAlarm(void){
	SendString("Alarm happening\r\n");
}
// sends one character via UART
void SendChar(char ch)  {
    while(!(UART5->S1 & UART_S1_TDRE_MASK) && !(UART5->S1 & UART_S1_TC_MASK) );
    UART5->D = ch;
}
//sends string via UART
void SendString(char *s)  {
	int i = 0;
	while (s[i]!=0){
		SendChar(s[i++]);
	}
}
//sends integer
void SendInt(int n) {
	if( n > 9 ) {
		int a = n / 10;
		n -= 10 * a;
		SendInt(a);
	}
	SendChar('0'+n);
}
// receive char from UART
char ReceiveChar(void) {
	while(!(UART5->S1 & UART_S1_RDRF_MASK));
	return UART5->D;
}
// receive string from uart of expected size
void ReceiveString(int expectedSize){
	if(expectedSize > maxInputLength){
		return;
	}
	for(int i = 0; i < expectedSize; i++){
		char c = ReceiveChar();
		SendChar(c);
		userInput[i] = c;
	}
}
// Initialize the MCU - basic clock settings, turning the watchdog off
void MCUInit(void)  {
    MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
    WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
}

// Initialize UART
void UART5Init(void) {
	UART5->C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
	UART5->BDH = 0;
	UART5->BDL = 26;
	UART5->C4 = 15;
	UART5->C1 = 0;
	UART5->C3 = 0;
	UART5->MA1 = 0;
	UART5->MA2 = 0;
	UART5->C2 |= ( UART_C2_TE_MASK | UART_C2_RE_MASK );
}
// Initialize pins ( function from lecture 06-imp-demo-FITkit3.pdf) [edited]
void PinInit(void)
{
    /* Turn on all port clocks */
	SIM->SCGC1 |= SIM_SCGC1_UART5_MASK;
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK;
	SIM->SCGC6 |= SIM_SCGC6_RTC_MASK;

    /* Set corresponding PTB pins (connected to LED's) for GPIO functionality */
    PORTA->PCR[4] = PORT_PCR_MUX(1);

    PORTB->PCR[5] = PORT_PCR_MUX(0x01); // D9
    PORTB->PCR[4] = PORT_PCR_MUX(0x01); // D10
    PORTB->PCR[3] = PORT_PCR_MUX(0x01); // D11
    PORTB->PCR[2] = PORT_PCR_MUX(0x01); // D12

    PORTE->PCR[10] = PORT_PCR_MUX(0x01); // SW2
    PORTE->PCR[12] = PORT_PCR_MUX(0x01); // SW3
    PORTE->PCR[27] = PORT_PCR_MUX(0x01); // SW4
    PORTE->PCR[26] = PORT_PCR_MUX(0x01); // SW5
    PORTE->PCR[11] = PORT_PCR_MUX(0x01); // SW6
    PORTE->PCR[8] = ( 0 | PORT_PCR_MUX(0x03) );
    PORTE->PCR[9] = ( 0 | PORT_PCR_MUX(0x03) );

    /* Change corresponding PTB port pins as outputs */
    PTA->PDDR = GPIO_PDDR_PDD(0x10);
    PTB->PDDR = GPIO_PDDR_PDD(0x3C);
    PTB->PDOR |= GPIO_PDOR_PDO(0x3C); // turn all LEDs OFF
}


//start RTC
void RTCStart(void){
	RTC->SR |= RTC_SR_TCE_MASK;
}
// stop RTC
void RTCStop(void){
	RTC->SR &= ~(RTC_SR_TCE_MASK);
}
// Initializes RTC
void RTCInit(void){
	RTC->CR = RTC_CR_SWR_MASK;
	RTC->CR &= ~RTC_CR_SWR_MASK;
	RTC->CR |= RTC_CR_OSCE_MASK;
	delay(0x600000);
	RTC->SR &= ~(RTC_SR_TCE_MASK);
	RTC->TCR = 0;
	RTC->TAR = 0;
	RTC->TSR = 1;
	NVIC_EnableIRQ(RTC_IRQn);
	RTCStart();
}
// Overwrites TSR register of RTC
void RTCSet(int seconds){
	RTCStop();
	RTC->TSR = seconds;
	RTCStart();
}
// Converts string from hhmm format to seconds
int ConvertToSeconds(char *hhmm){
	int hhmmToInt = strtol(hhmm, NULL, 10);
	int seconds = (hhmmToInt / 100) * 3600;
	hhmmToInt = hhmmToInt%100;
	seconds += hhmmToInt * 60;
	return seconds;
}
// converts seconds to hhmmss format
void ConvertToHhmmss(char *convertedString, int seconds){
	int hours = seconds / 3600;
	seconds -= 3600 * hours;
	int minutes = seconds / 60;
	seconds -= 60 * minutes;
	// now build the resulting string in HH:MM:SS format
	char separator[1];
	strcpy(separator, ":");
	char digitsToString[2];
	strcpy(convertedString, "");

	if(hours < 10) strcat(convertedString, "0");
	sprintf(digitsToString, "%d", hours);
	strcat(convertedString, digitsToString);
	strcat(convertedString, separator);
	if(minutes < 10) strcat(convertedString, "0");
	sprintf(digitsToString, "%d", minutes);
	strcat(convertedString, digitsToString);
	strcat(convertedString, separator);
	if(seconds < 10) strcat(convertedString, "0");
	sprintf(digitsToString, "%d", seconds);
	strcat(convertedString, digitsToString);

}
// takes one parameter - seconds
//and sends it to UART
void SendTimeSeconds(unsigned int seconds){
	char timeValue[8];
	ConvertToHhmmss(timeValue, seconds);
	SendString(timeValue);
}
// Initializes everything we need
void Init(void){
	MCUInit();
	PinInit();
	UART5Init();
	RTCInit();

}
//sends welcome message
void SendWelcome(){
	SendString("\r\n");
	SendString("ALARM CLOCK\r\n");
	SendString("This project was created by Milan Hruban at BUT VUT\r\n\r\n");
}
// draws menu on the screen
void drawMenu(void){
	SendString("\r\n\r\n\r\n\r\n----------MENU----------\r\n\r\n");
	SendString("- change time ");SendTimeSeconds(RTC->TSR); if(menuItemArrow==0) {SendString(" <--");} SendString("\r\n");
	SendString("- set alarm "); alarmOn ? SendTimeSeconds(alarmTimeSeconds) : SendString("(no alarm set)"); if(menuItemArrow==1) {SendString(" <--");} SendString("\r\n");
	SendString("- alarm repetitions: "); SendInt(alarmRepetitions); if(menuItemArrow==2) {SendString(" <--");} SendString("\r\n");
	SendString("- repetition delay: "); SendTimeSeconds(alarmRepetitionDelay); if(menuItemArrow==3) {SendString(" <--");} SendString("\r\n");
	SendString("- Alarm tone: "); SendInt(soundType); if(menuItemArrow==4) {SendString(" <--");} SendString("\r\n");
	SendString("- Light notification: "); SendInt(lightType); if(menuItemArrow==5) {SendString(" <--");} SendString("\r\n");
	SendString("- Fancy clock: "); fancyClock ? SendString("ON") : SendString("OFF"); if(menuItemArrow==6) {SendString(" <--");} SendString("\r\n");
	SendString("- Exit menu"); if(menuItemArrow==7) {SendString(" <--");} SendString("\r\n");
}
// draws time from RTC register on screen
void drawTime(void){
	if(fancyClock == 0){
		char timeValue[8];
		ConvertToHhmmss(timeValue, RTC->TSR);
		SendString(timeValue);
		SendString("\r\n");
	}else{
		int seconds = RTC->TSR;
		int hours = seconds / 3600;
		seconds -= 3600 * hours;
		int minutes = seconds / 60;
		seconds -= 60 * minutes;

		int timeDigits[8];
		if(hours < 10) timeDigits[0] = 0;
		else timeDigits[0] = hours / 10;
		timeDigits[1] = hours % 10;
		timeDigits[2] = 10;

		if(minutes < 10) timeDigits[3] = 0;
		else timeDigits[3] = minutes / 10;
		timeDigits[4] = minutes % 10;
		timeDigits[5] = 10;

		if(seconds < 10) timeDigits[6] = 0;
		else timeDigits[6] = seconds / 10;
		timeDigits[7] = seconds % 10;

		for(int i = 0; i < 6; i++){
			for(int j = 0; j < 8; j++){
				if(timeDigits[j] == 10) continue;
				SendString(asciiSWFont[timeDigits[j]][i]);
				SendString("   ");
			}
			SendString("\r\n");
		}
	}
}
// handles pressing left button in menu
void PressedLeft(){
	int time = RTC->TSR;
	switch(menuItemArrow){
		case 0:
			time -= 60;
			if(time <= 0) time = 1;
			RTCSet(time);
			break;
		case 1:
			if(alarmTimeSeconds < 1800) alarmTimeSeconds = secondsInDay;
			alarmTimeSeconds -= 1800;
			alarmOn = 1;
			break;
		case 2:
			if(alarmRepetitions == 0) {
				beep();
				break;
			}
			alarmRepetitions--;
			break;
		case 3:
			if(alarmRepetitionDelay <= 60){
				beep();
			}else{
				alarmRepetitionDelay -= 60;
			}
			break;
		case 4:
			if(soundType == 0){
				soundType = soundTypeCount;
			}else{
				soundType--;
			}
			break;
		case 5:
			if(lightType == 0){
				lightType = lightTypeCount;
			}else{
				lightType--;
			}
			break;
		case 6:
			fancyClock = fancyClock ? 0 : 1;
			break;
		default:
			beep();
			break;
	}
}
// handles pressing right button in menu
void PressedRight(){
	int time = RTC->TSR;
	switch(menuItemArrow){
		case 0:
			time += 60;
			if(time >= secondsInDay) time = 1;
			RTCSet(time);
			break;
		case 1:
			if(alarmTimeSeconds >= secondsInDay-1800) alarmTimeSeconds = 0;
			alarmTimeSeconds += 1800;
			alarmOn = 1;
			break;
		case 2:
			alarmRepetitions++;
			break;
		case 3:
			if(alarmRepetitionDelay > secondsInDay){
				beep();
			}else{
				alarmRepetitionDelay += 60;
			}
			break;
		case 4:
			if(soundType == soundTypeCount){
				soundType = 0;
			}else{
				soundType++;
			}
			break;
		case 5:
			if(lightType == lightTypeCount){
				lightType = 0;
			}else{
				lightType++;
			}
			break;
		case 6:
			fancyClock = fancyClock ? 0 : 1;
			break;
		default:
			beep();
			break;
	}
}
// handles pressing action button in menu
int PressedAction(){
	int time = RTC->TSR;
	int setTime = 0;
	switch(menuItemArrow){
		case 0:
			while(setTime == 0){
				SendString("Set current time(HHMM): ");
				setTime = GetTime();
				RTCSet(setTime);
				SendString("\r\n");
				if(setTime == 0){
					beep();
					SendString("Invalid time set, please try again. Make sure to use correct format.\r\n");
				}
			}
			break;
		case 1:
			if(alarmOn){
				alarmOn = 0;
			}else{
				while(setTime == 0){
					SendString("Set alarm time(HHMM): ");
					setTime = GetTime();
					alarmTimeSeconds = setTime;
					SendString("\r\n");
					if(setTime == 0){
						beep();
						SendString("Invalid time set, please try again. Make sure to use correct format.\r\n");
					}
				}
				alarmOn = 1;
			}
			break;
		case 2:
			alarmRepetitions++;
			break;
		case 3:
			while(setTime == 0){
				SendString("Set alarm delay(HHMM): ");
				setTime = GetTime();
				alarmRepetitionDelay = setTime;
				SendString("\r\n");
				if(setTime == 0){
					beep();
					SendString("Invalid time set, please try again. Make sure to use correct format.\r\n");
				}
			}
			break;
		case 4:
			playAlarmSound();
			break;
		case 5:
			playAlarmLight();
			break;
		case 6:
			fancyClock = fancyClock ? 0 : 1;
			break;
		case 7:
			menuItemArrow = 0;
			RTC->TAR = alarmTimeSeconds;
			return 0;
			break;
		default:
			beep();
			break;
	}
}

// handles user input from menu
void getMenuInput(void){
	// presed action set to 1 to prevent instant click when returning from main loop
	int pressed_up = 0, pressed_down = 0, pressed_left = 0, pressed_right = 0, pressed_action = 1;
	drawMenu();
	while(1){
		if(!pressed_up && !(GPIOE_PDIR & BTN_SW5)){
			pressed_up = 1;
			//handle UP
			if(menuItemArrow == 0)menuItemArrow = menuItemCount - 1;
			else menuItemArrow--;
			drawMenu();
		}else if (GPIOE_PDIR & BTN_SW5) pressed_up = 0;

		if(!pressed_down && !(GPIOE_PDIR & BTN_SW3)){
			pressed_down = 1;
			//handle  DOWN
			if(menuItemArrow == menuItemCount - 1)menuItemArrow = 0;
			else menuItemArrow++;
			drawMenu();
		}else if (GPIOE_PDIR & BTN_SW3) pressed_down = 0;

		if(!pressed_left && !(GPIOE_PDIR & BTN_SW4)){
			pressed_left = 1;
			//handle LEFT
			PressedLeft();
			drawMenu();
		}else if (GPIOE_PDIR & BTN_SW4) pressed_left = 0;

		if(!pressed_right && !(GPIOE_PDIR & BTN_SW2)){
			pressed_right = 1;
			//handle  RIGHT
			PressedRight();
			drawMenu();
		}else if (GPIOE_PDIR & BTN_SW2) pressed_right = 0;

		if(!pressed_action && !(GPIOE_PDIR & BTN_SW6)){
			pressed_action = 1;
			//handle ACTION
			if(PressedAction() == 0){
				break;
			}
			drawMenu();
		}else if (GPIOE_PDIR & BTN_SW6) pressed_action = 0;
	}
}
// gets user settings via UART
void GetSettings(){
	SendString("Please choose your settings\r\n\r\n");
	SendString("Set current time(HHMM): ");
	unsigned int secondsSet = GetTime();
	while(secondsSet == 0){
		SendString("\r\n");
		beep();
		SendString("Invalid time set, please try again. Make sure to use correct format.\r\n");
		SendString("Set current time(HHMM): ");
		secondsSet = GetTime();
	}
	SendString("\r\n");
	RTCSet(secondsSet);
	SendString("Time set to: ");
	char timeValue[8];
	ConvertToHhmmss(timeValue, RTC->TSR);
	SendString(timeValue);
	SendString("\r\n");
	// show menu controlled by arrow keys
	getMenuInput();
	SendString("You can open menu any time by holding down the Action button (SW6)\r\n");
}
// gets time in user input field returns zero on fail
int GetTime(){
	ReceiveString(4);
	int secondsSet = ConvertToSeconds(userInput);

	if(secondsSet < 0 || secondsSet >= secondsInDay){
		return 0;
	}
	return secondsSet;
}
void RTC_IRQHandler(void) {
	if(((RTC_SR & RTC_SR_TAF_MASK) == 0x04) && alarmOn){
		if(alarmRepetitions > 0 && alarmRepetitionsPerformed <= alarmRepetitions){
			alarmRepetitionsPerformed++;
			RTC->TAR += alarmRepetitionDelay;
			alarmTimeSeconds = RTC->TAR;
			alarmRinging = 1;
		}
		if(alarmRepetitions > 0 && alarmRepetitions < alarmRepetitionsPerformed){
			alarmRepetitionsPerformed = 0;
			alarmOn = 0;
			RTC->TAR = alarmTimeSeconds;
		}
		if(alarmRepetitions == 0){
			RTC->TAR = alarmTimeSeconds;
			alarmOn = 0;
			alarmRinging = 1;
		}

	}
}
int main(void)
{
	Init();
	SendWelcome();
	GetSettings();
	unsigned int delayPeriod = 2555555;
	unsigned int pressedAction = 1;
	while(1){
		drawTime();
		// check if user wants to open the menu
		if(!pressedAction && !(GPIOE_PDIR & BTN_SW6)){
			pressedAction = 1;
			getMenuInput();
		}else if (GPIOE_PDIR & BTN_SW6) pressedAction = 0;
		if(alarmRinging){
			performAlarm();
			if(!(GPIOE_PDIR & BTN_SW5)){
				alarmRinging = 0;
				alarmOn = 0;
				alarmRepetitionsPerformed = 0;
			}else if(!(GPIOE_PDIR & BTN_SW2) || !(GPIOE_PDIR & BTN_SW3) || !(GPIOE_PDIR & BTN_SW4)){
				alarmRinging = 0;
			}
		}

		//GPIOB_PDOR ^= LED_D9;
		delay(delayPeriod);
	}
}
////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
