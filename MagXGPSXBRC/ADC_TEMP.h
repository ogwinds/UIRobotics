/* --------------------- Guard Against Multiple Inclusions ------------------- */
#ifndef __ADC_TEMP_H__
	#define __ADC_TEMP_H__

	/* -----------------------------------------------------------------------
       This file is the header for the initialization of the Analog to Digital
       converters (ADC) that will be used for the temperature sensors.
       ----------------------------------------------------------------------- */

    /* ------------------ Define setup paramaters for ADC10 ------------------ */
    //     turn module on | output integer  | trigger mode auto | enable auto sample
    #define ADC_PARAM1  ADC_MODULE_ON | ADC_FORMAT_INTG | ADC_CLK_AUTO  | ADC_AUTO_SAMPLING_ON

    //      ADC ref external   | disable offset test    | disable scan mode | perform 2 samples | use dual buffers | use alternate mode
    #define ADC_PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF  | ADC_SAMPLES_PER_INT_2 | ADC_ALT_BUF_ON  | ADC_ALT_INPUT_ON

    //      use ADC internal clock | set sample time
    #define ADC_PARAM3  ADC_CONV_CLK_INTERNAL_RC | ADC_SAMPLE_TIME_15
 
    // Define AN16 and AN19 as analog inputs
    #define ADC_PARAM4  ENABLE_AN16_ANA | ENABLE_AN19_ANA

    // Do not assign channels to scan
    #define ADC_PARAM5  SKIP_SCAN_ALL
    
    #define ADCMAX    1023
    #define OpenADC10G(config1, config2, config3, configport, configscan) (mPORTGSetPinsAnalogIn(configport), AD1CSSL = ~(configscan), AD1CON3 = (config3), AD1CON2 = (config2), AD1CON1 = (config1) )

    // Temperature Parameters - Temporary
    #define SLOW_TEMP     125  // Degrees F where we start throttling motor
    #define STOP_TEMP     150  // Degrees F where we STOP the motor

	// Function Prototypes
    void init_temperature(void); 
    void read_temperature(int *t1, int *t2);
    void check_thresh(float *motor1_scale, float *motor2_scale);
    int GetTemp1();
    int GetTemp2();
    void read_temperature_store();
#endif
