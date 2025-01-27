// System included files
#include "hardware.h"
#include <plib.h>
#include "uart4.h"
#include "led7.h"
#include "LCDlib.h"
#include "RC.h"

unsigned int millisec = 0;
	
void Hardware_Setup(void) {
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Statement configure cache, wait states and peripheral bus clock
 * Configure the device for maximum performance but does not change the PBDIV
 * Given the options, this function will change the flash wait states, RAM
 * wait state and enable prefetch cache but will not change the PBDIV.
 * The PBDIV value is already set via the pragma FPBDIV option above..
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	SYSTEMConfig(GetSystemClock(), SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
	DDPCONbits.JTAGEN = 0;	// Statement is required to use Pin RA0 as IO
  
	ALL_DIGITAL_IO();		// Sets all LED, switches and push buttons for digital IO
	SET_MIC_ANALOG();		// Sets microphone input for analog
	SET_POT_ANALOG();		// Sets ANALOG INPUT CONTROL for analog input
	
	Set_All_LEDs_Output();	// Sets Basys MX3 LED0 through LED7 as output
	Set_All_LEDs_Off();		// Sets Basys MX3 LED0 through LED7 off
	SWcfg();				// Sets Basys MX3 SW0 through SW7 as input
	Set_All_PBs_Input();	// Sets Basys MX3 push buttons as input
	Set_RGB_Output();		// Sets Basys MX3 RGB LED as output
	Set_LED8_RGB(0);		// Sets Basys MX3 RGB LED off
	MCInit();
	initLCD();
	initRC();
	initTimer1();

	//EnableCNA15();
	//ConfigCNPullups();
	
}

/* -------------------------------- initTimer1 -------------------------------
  @ Summary
	 Initializes Timer 1 for a 10us interrupt.
  @ Parameters
	 This function takes no parameters.
  @ Returns
	 This function has no return value.
   --------------------------------------------------------------------------- */
static void initTimer1(void) {
	PORTSetPinsDigitalOut(IOPORT_B, BIT_8);
	LATBbits.LATB8 = 0;
	millisec = 0;	   // Global millisecond counter
	OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_1, TMR1_TICK);
	
	// Set Timer 1 interrupt with a priority of 2
	ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_2);
	mT1IntEnable(1);		   // Enable interrupts of T1   

	// Enable multi-vector interrupts
	INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);  // Do only once
	INTEnableInterrupts();   //Do as needed for global interrupt control
}

/* ------------------------------ Timer1Handler ------------------------------
  @ Summary
	 Processes the code associated with a Timer 1 interrupt. It is responsible
	 for generating the signals for each RC channel in two phases; the three 
	 state FSM controls the on period and the off period.
  @ Parameters
	 None, it is an ISR
  @ Returns
	 None
  ---------------------------------------------------------------------------- */
void __ISR(_TIMER_1_VECTOR, IPL2SOFT) Timer1Handler(void) {
	static ms = 100;			// Millisecond counter
	static int onesec = 1000;	// One second counter

	ms--;						// Increment the millisecond counter
	if (ms <= 0) {
		millisec++;		
		ms = 100;
		onesec--;
		if (onesec <= 0) 
		{
			invLED3();
			onesec = 1000;
		}
		updateLED7();
	}
	rcUpdateServos();		 	// This updates the RC outputs for the servos
	rcUpdateSpeedControllers();	// This updates the RC outputs for the speed controllers
	mT1ClearIntFlag();			// Clear the interrupt flag	
}