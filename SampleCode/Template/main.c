/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "misc_config.h"

#include "timer_service.h"
#include "hid_transfer.h"
#include "hid_joystick_api.h"
#include "hid_joystick_conf.h"
#include "button_sw_debounce.h"
/*_____ D E C L A R A T I O N S ____________________________________________*/

volatile struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS                 	(flag_PROJ_CTL.bit0)
#define FLAG_PROJ_ADC_DATA_READY               			(flag_PROJ_CTL.bit1)
#define FLAG_PROJ_REVERSE2                 				(flag_PROJ_CTL.bit2)
#define FLAG_PROJ_REVERSE3                              (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_REVERSE4                              (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_REVERSE5                              (flag_PROJ_CTL.bit5)
#define FLAG_PROJ_REVERSE6                              (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_REVERSE7                              (flag_PROJ_CTL.bit7)


/*_____ D E F I N I T I O N S ______________________________________________*/

volatile unsigned long counter_systick = 0;
volatile uint32_t counter_tick = 0;
// timer task
static int g_timer_id_task1 = -1;
static int g_timer_id_task2 = -1;
static int g_timer_id_task3 = -1;

#define CRYSTAL_LESS                                    1    /* CRYSTAL_LESS must be 1 if USB clock source is HIRC */
#define TRIM_INIT                                       (SYS_BASE+0x118)
#if CRYSTAL_LESS
uint32_t u32TrimInit;
#endif


enum
{
	ADC0_CH0 = 0 ,
	ADC0_CH1 , 	
	ADC0_CH2 , 
	ADC0_CH3 , 
	ADC0_CH4 , 
	
	ADC0_CH5 , 
	ADC0_CH6 , 
	ADC0_CH7 , 
	ADC0_CH8 , 
	ADC0_CH9 , 
	
	ADC0_CH10 , 
	ADC0_CH11 , 
	ADC0_CH12 ,
	ADC0_CH13 , 
	ADC0_CH14 , 
	ADC0_CH15 , 
	
	ADC_CH_DEFAULT 	
}ADC_CH_TypeDef;

typedef struct
{	
	uint8_t ch; 
	uint16_t data;
}ADC_struct;

enum{
	State_avergage = 0 ,
	State_moving ,		
	
	State_DEFAULT	
}ADC_State;

volatile ADC_struct adc_measure[] =
{
	{ADC0_CH7,0},
	{ADC0_CH6,0},
	{ADC0_CH9,0},
	{ADC0_CH8,0},

	{ADC_CH_DEFAULT,0},	
};


// #define ADC_RESOLUTION							((uint16_t)(4096u))
// #define ADC_REF_VOLTAGE							((uint16_t)(3300u))	//(float)(3.3f)

// #define ABS(X)  								((X) > 0 ? (X) : -(X))

// #define ADC_DIGITAL_SCALE(void) 				(0xFFFU >> ((0) >> (3U - 1U)))		//0: 12 BIT 
// #define ADC_CALC_DATA_TO_VOLTAGE(DATA,VREF) 	((DATA) * (VREF) / ADC_DIGITAL_SCALE())

#define ADCextendSampling 						(10)

#define ADC_AVG_TRAGET 						    (8)
#define ADC_AVG_POW	 					        (3)
#define ADC_CH_NUM	 						    (4)
#define ADC_WAIT_READY_TIMEOUT_COUNT             (0xFFFFU)

volatile uint16_t adc_temporay_ch = 0;
volatile uint16_t adc_temporay_raw = 0;
uint32_t AVdd = 0;
volatile uint32_t g_u32AdcTimeoutTotal = 0U;
volatile uint32_t g_u32AdcTimeoutAverage = 0U;
volatile uint32_t g_u32AdcTimeoutMoving = 0U;


enum
{
    AXIS_X,
    AXIS_Y,
    AXIS_Z,
    AXIS_Rz,
};

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

unsigned long get_systick(void)
{
	return (counter_systick);
}

void set_systick(unsigned long t)
{
	counter_systick = t;
}

void systick_counter(void)
{
	counter_systick++;
}

void SysTick_Handler(void)
{

    systick_counter();

    // if ((get_systick() % 1000) == 0)
    // {
       
    // }

    #if defined (ENABLE_TICK_EVENT)
    TickCheckTickEvent();
    #endif    
}

void SysTick_delay(unsigned long delay)
{  
    
    unsigned long tickstart = get_systick(); 
    unsigned long wait = delay; 

    while((get_systick() - tickstart) < wait) 
    { 
    } 

}

void SysTick_enable(unsigned long ticks_per_second)
{
    set_systick(0);
    if (SysTick_Config(SystemCoreClock / ticks_per_second))
    {
        /* Setup SysTick Timer for 1 second interrupts  */
        printf("Set system tick error!!\n");
        while (1);
    }

    #if defined (ENABLE_TICK_EVENT)
    TickInitTickEvent();
    #endif
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void tick_counter(void)
{
	counter_tick++;
}

void delay_ms(uint16_t ms)
{
	#if 1
	uint32_t start = get_tick();
    while ((uint32_t)(get_tick() - start) < (uint32_t)ms) 
	{
		
	}	
	#else
	TIMER_Delay(TIMER0, 1000*ms);
	#endif
}


__STATIC_INLINE uint32_t FMC_ReadBandGap(void)
{
    FMC->ISPCMD = FMC_ISPCMD_READ_UID;            /* Set ISP Command Code */
    FMC->ISPADDR = 0x70u;                         /* Must keep 0x70 when read Band-Gap */
    FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;           /* Trigger to start ISP procedure */
#if ISBEN
    __ISB();
#endif                                            /* To make sure ISP/CPU be Synchronized */
    while(FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk) {}  /* Waiting for ISP Done */

    return FMC->ISPDAT & 0xFFF;
}

// void convertDecToBin(int n)
// {
//     int k = 0;
//     unsigned char *p = (unsigned char*)&n;
//     int val2 = 0;
//     int i = 0;
//     for(k = 0; k <= 1; k++)
//     {
//         val2 = *(p+k);
//         for (i = 7; i >= 0; i--)
//         {
//             if(val2 & (1 << i))
//                 printf("1");
//             else
//                 printf("0");
//         }
//         printf(" ");
//     }
// }


void ADC_IRQHandler(void)
{
	FLAG_PROJ_ADC_DATA_READY = 1;
	adc_temporay_raw = ADC_GET_CONVERSION_DATA(ADC, adc_temporay_ch);	
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT); /* Clear the A/D interrupt flag */
}

void ADC_ReadAVdd(void)
{
    int32_t  i32ConversionData;
    int32_t  i32BuiltInData;

    ADC_POWER_ON(ADC);
    CLK_SysTickDelay(10000);

	
    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE, BIT29);
    ADC_SetExtendSampleTime(ADC, 0, 71);
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
    ADC_ENABLE_INT(ADC, ADC_ADF_INT);
    NVIC_EnableIRQ(ADC_IRQn);
    ADC_START_CONV(ADC);

    ADC_DISABLE_INT(ADC, ADC_ADF_INT);
		
    i32ConversionData = ADC_GET_CONVERSION_DATA(ADC, 29);
    SYS_UnlockReg();
    FMC_Open();
    i32BuiltInData = FMC_ReadBandGap();	

	AVdd = 3072*i32BuiltInData/i32ConversionData;

//	printf("%s : %d,%d,%d\r\n",__FUNCTION__,AVdd, i32ConversionData,i32BuiltInData);

    NVIC_DisableIRQ(ADC_IRQn);
	
}

void ADC_InitChannel(uint8_t ch)
{
	FLAG_PROJ_ADC_DATA_READY = 0;

//	ADC_ReadAVdd();

    /* Enable ADC converter */
//    ADC_POWER_ON(ADC);

    /*Wait for ADC internal power ready*/
//    CLK_SysTickDelay(10000);

    /* Set input mode as single-end, and Single mode*/
    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE,(uint32_t) 0x1 << ch);

    /* To sample band-gap precisely, the ADC capacitor must be charged at least 3 us for charging the ADC capacitor ( Cin )*/
    /* Sampling time = extended sampling time + 1 */
    /* 1/24000000 * (Sampling time) = 3 us */
	/*
	    printf("+----------------------------------------------------------------------+\n");
	    printf("|   ADC clock source -> PCLK1  = 48 MHz                                |\n");
	    printf("|   ADC clock divider          = 2                                     |\n");
	    printf("|   ADC clock                  = 48 MHz / 2 = 24 MHz                   |\n");
	    printf("|   ADC extended sampling time = 71                                    |\n");
	    printf("|   ADC conversion time = 17 + ADC extended sampling time = 88         |\n");
	    printf("|   ADC conversion rate = 24 MHz / 88 = 272.7 ksps                     |\n");
	    printf("+----------------------------------------------------------------------+\n");
	*/

    /* Set extend sampling time based on external resistor value.*/
    ADC_SetExtendSampleTime(ADC,(uint32_t) NULL, ADCextendSampling);

    /* Select ADC input channel */
    ADC_SET_INPUT_CHANNEL(ADC, 0x1 << ch);

	ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
	ADC_ENABLE_INT(ADC, ADC_ADF_INT);
	NVIC_EnableIRQ(ADC_IRQn);

    /* Start ADC conversion */
    ADC_START_CONV(ADC);
	
}

__STATIC_INLINE uint8_t ADC_WaitReadyWithTimeout(uint16_t u16TimeoutCount)
{
    while (!FLAG_PROJ_ADC_DATA_READY)
    {
        if (u16TimeoutCount == 0U)
        {
            return 0U;
        }
        u16TimeoutCount--;
    }

    return 1U;
}

void ADC_Process(uint8_t state)
{
	uint8_t idx = 0;
    uint8_t u8Timeout = 0U;
	volatile uint32_t sum = 0;
	uint16_t tmp = 0;
	
	switch(state)
	{
		case State_avergage:	
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
                u8Timeout = 0U;
                adc_temporay_ch = adc_measure[idx].ch;
				for ( tmp = 0 ; tmp < ADC_AVG_TRAGET ; tmp++)
				{
					ADC_InitChannel(adc_temporay_ch);
                    if (ADC_WaitReadyWithTimeout(ADC_WAIT_READY_TIMEOUT_COUNT) == 0U)
                    {
                        g_u32AdcTimeoutTotal++;
                        g_u32AdcTimeoutAverage++;
                        u8Timeout = 1U;
                        ADC_DISABLE_INT(ADC, ADC_ADF_INT);
                        break;
                    }
					ADC_DISABLE_INT(ADC, ADC_ADF_INT);

					sum += adc_temporay_raw;									    //sum the first 8 ADC data
				}
                if (u8Timeout == 0U)
                {
				    adc_measure[idx].data = (uint16_t) (sum >> ADC_AVG_POW);			//do average
                }
                sum = 0;
			}

			break;

		case State_moving:
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
                adc_temporay_ch = adc_measure[idx].ch;
                ADC_InitChannel(adc_temporay_ch);
                if (ADC_WaitReadyWithTimeout(ADC_WAIT_READY_TIMEOUT_COUNT) == 0U)
                {
                    g_u32AdcTimeoutTotal++;
                    g_u32AdcTimeoutMoving++;
                    ADC_DISABLE_INT(ADC, ADC_ADF_INT);
                    continue;
                }
				ADC_DISABLE_INT(ADC, ADC_ADF_INT);

				sum = adc_measure[idx].data << ADC_AVG_POW;					    //extend the original average data
				sum -= adc_measure[idx].data;									//subtract the old average data
				sum += adc_temporay_raw;									    //add the new adc data
				adc_measure[idx].data = (uint16_t) (sum >> ADC_AVG_POW);		//do average again
			}

			#if 0	// debug
			for ( idx = 0 ; idx < ADC_CH_NUM ; idx++)
			{
				tmp = adc_measure[idx].data;
//				convertDecToBin(tmp);//ADC_DataArray[idx]
//				printf("%d:%4dmv," , idx ,ADC_CALC_DATA_TO_VOLTAGE(adc_measure[idx].data,ADC_REF_VOLTAGE));
//				printf("%d:%3X,%4d ," , idx ,adc_measure[idx].data,ADC_CALC_DATA_TO_VOLTAGE(adc_measure[idx].data,ADC_REF_VOLTAGE));
//				printf("%d:0x%3X," , 4 , adc_measure[idx].data);
				printf("%3X:%4d ," , tmp ,ADC_CALC_DATA_TO_VOLTAGE(tmp,ADC_REF_VOLTAGE));
//				printf("%2X:%2X ," , adc_measure[idx].ch,adc_measure[idx].data);
				
				if (idx == (ADC_CH_NUM -1) )
				{
					printf("\r\n");
				}				
			}
			#endif	
			
			break;	
	}	
}





void USB_trim_process(void)
{

    /* Start USB trim if it is not enabled. */
    if((SYS->HIRCTRIMCTL & SYS_HIRCTRIMCTL_FREQSEL_Msk) != 1)
    {
        /* Start USB trim only when SOF */
        if(USBD->INTSTS & USBD_INTSTS_SOFIF_Msk)
        {
            /* Clear SOF */
            USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;

            /* Re-enable crystal-less */
            SYS->HIRCTRIMCTL = 0x01;
            SYS->HIRCTRIMCTL |= SYS_HIRCTRIMCTL_REFCKSEL_Msk;
        }
    }

    /* Disable USB Trim when error */
    if(SYS->HIRCTRIMSTS & (SYS_HIRCTRIMSTS_CLKERIF_Msk | SYS_HIRCTRIMSTS_TFAILIF_Msk))
    {
        /* Init TRIM */
        M32(TRIM_INIT) = u32TrimInit;

        /* Disable crystal-less */
        SYS->HIRCTRIMCTL = 0;

        /* Clear error flags */
        SYS->HIRCTRIMSTS = SYS_HIRCTRIMSTS_CLKERIF_Msk | SYS_HIRCTRIMSTS_TFAILIF_Msk;

        /* Clear SOF */
        USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
    }
}

void USB_HID_Init(void)
{
    /* Open USB controller */
    USBD_Open(&gsInfo, HID_ClassRequest, NULL);

    /* Endpoint configuration */
    HID_Init();

    /* Start USB device */
    USBD_Start();

    NVIC_EnableIRQ(USBD_IRQn);

#if CRYSTAL_LESS
    /* Backup default trim */
    u32TrimInit = M32(TRIM_INIT);
#endif

    /* Clear SOF */
    USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
}


void Task_1000ms_Callback(void *user_data)
{
	// static uint32_t LOG1 = 0;

    // printf("%s(timer) : %4d\r\n",__FUNCTION__,LOG1++);
    PB14 ^= 1;
}

void Task_100ms_Callback(void *user_data)
{
    /* Optional debug/log task; HID IN report TX path does not depend on this. */
    (void)user_data;
    HidTool_ProcessLogTask();
}

void Task_10ms_Callback(void *user_data)
{
    uint16_t u16X = 0;
    uint16_t u16Y = 0;
    uint16_t u16Z = 0;
    uint16_t u16Rz = 0;

    (void)user_data;
#if (HID_JOYSTICK_USE_BUTTON_SW_DEBOUNCE != 0U)
    /*
     * Button/GPIO routing is selected by profile in hid_joystick_conf.h:
     * - HID_JOYSTICK_MAP_PROFILE_LEFT
     * - HID_JOYSTICK_MAP_PROFILE_RIGHT
     * - HID_JOYSTICK_MAP_PROFILE_BOTH
     */
    ButtonSwDebounce_UpdateHidByProfile();
#endif

    /*
        L3      - X-AXIS    - PB7 (ADC0_CH7)
        L3      - Y-AXIS    - PB6 (ADC0_CH6)
        R3      - X-AXIS    - PB9 (ADC0_CH9)
        R3      - Y-AXIS    - PB8 (ADC0_CH8)        
    */
	ADC_Process(State_moving);
    // u16X = adc_measure[AXIS_X].data;
    u16X = 4095 - adc_measure[AXIS_X].data; // axis reverse
    u16Y = adc_measure[AXIS_Y].data;
    // u16Y = 4095 - adc_measure[AXIS_Y].data; // axis reverse
    #if 0   // attach adc value
    // u16Z = adc_measure[AXIS_Z].data;
    u16Z = 4095 - adc_measure[AXIS_X].data;
    u16Rz = adc_measure[AXIS_Rz].data;
    // u16Rz = 4095 - adc_measure[AXIS_Y].data;
    #else   // for test
    // u16Z = adc_measure[AXIS_X].data;
    u16Z = 4095 - adc_measure[AXIS_X].data; // axis reverse
    u16Rz = adc_measure[AXIS_Y].data;
    // u16Rz = 4095 - adc_measure[AXIS_Y].data; // axis reverse
    #endif

    HidTool_SetExternalAdc(u16X,u16Y,u16Z,u16Rz);

}

void TimerService_CreateTask(void)
{
    /* Create task1 timer: 10 ms */
    g_timer_id_task1 = TimerService_CreateTimer(10U, Task_10ms_Callback, (void *)0);

    /* Start timers */
    if (g_timer_id_task1 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task1);
        printf("task1 id = %d\r\n", g_timer_id_task1);
    }

    /* Create task2 timer: 100 ms */
    g_timer_id_task2 = TimerService_CreateTimer(100U, Task_100ms_Callback, (void *)0);

    /* Start timers */
    if (g_timer_id_task2 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task2);
        printf("task2 id = %d\r\n", g_timer_id_task2);
    }

    /* Create task3 timer: 1000 ms */
    g_timer_id_task3 = TimerService_CreateTimer(1000U, Task_1000ms_Callback, (void *)0);

    /* Start timers */
    if (g_timer_id_task3 >= 0)
    {
        TimerService_StartTimer((unsigned int)g_timer_id_task3);
        printf("task3 id = %d\r\n", g_timer_id_task3);
    }
}

//
// check_reset_source
//
uint8_t check_reset_source(void)
{
    uint32_t src = SYS_GetResetSrc();

    SYS->RSTSTS |= 0x1FF;
    printf("Reset Source <0x%08X>\r\n", src);

    #if 1   //DEBUG , list reset source
    if (src & BIT0)
    {
        printf("0)POR Reset Flag\r\n");       
    }
    if (src & BIT1)
    {
        printf("1)NRESET Pin Reset Flag\r\n");       
    }
    if (src & BIT2)
    {
        printf("2)WDT Reset Flag\r\n");       
    }
    if (src & BIT3)
    {
        printf("3)LVR Reset Flag\r\n");       
    }
    if (src & BIT4)
    {
        printf("4)BOD Reset Flag\r\n");       
    }
    if (src & BIT5)
    {
        printf("5)System Reset Flag \r\n");       
    }
    if (src & BIT6)
    {
        printf("6)Reserved.\r\n");       
    }
    if (src & BIT7)
    {
        printf("7)CPU Reset Flag\r\n");       
    }
    if (src & BIT8)
    {
        printf("8)CPU Lockup Reset Flag\r\n");       
    }
    #endif
    
    if (src & SYS_RSTSTS_PORF_Msk) {
        SYS_ClearResetSrc(SYS_RSTSTS_PORF_Msk);
        
        printf("power on from POR\r\n");
        return FALSE;
    }    
    else if (src & SYS_RSTSTS_PINRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_PINRF_Msk);
        
        printf("power on from nRESET pin\r\n");
        return FALSE;
    } 
    else if (src & SYS_RSTSTS_WDTRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_WDTRF_Msk);
        
        printf("power on from WDT Reset\r\n");
        return FALSE;
    }    
    else if (src & SYS_RSTSTS_LVRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_LVRF_Msk);
        
        printf("power on from LVR Reset\r\n");
        return FALSE;
    }    
    else if (src & SYS_RSTSTS_BODRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_BODRF_Msk);
        
        printf("power on from BOD Reset\r\n");
        return FALSE;
    }    
    else if (src & SYS_RSTSTS_SYSRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_SYSRF_Msk);
        
        printf("power on from System Reset\r\n");
        return FALSE;
    } 
    else if (src & SYS_RSTSTS_CPURF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_CPURF_Msk);

        printf("power on from CPU reset\r\n");
        return FALSE;         
    }    
    else if (src & SYS_RSTSTS_CPULKRF_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSTS_CPULKRF_Msk);
        
        printf("power on from CPU Lockup Reset\r\n");
        return FALSE;
    }   
    
    printf("power on from unhandle reset source\r\n");
    return FALSE;
}

void TMR1_IRQHandler(void)
{
	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		// if ((get_tick() % 1000) == 0)
		// {
        //     FLAG_PROJ_TIMER_PERIOD_1000MS = 1;//set_flag(flag_timer_period_1000ms ,ENABLE);
		// }

		// if ((get_tick() % 50) == 0)
		// {

		// }	

        TimerService_Tick1ms();
#if (HID_JOYSTICK_USE_BUTTON_SW_DEBOUNCE != 0U)
        ButtonSwDebounce_Timer1msTick();
#endif
    }
}

void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void loop(void)
{
	// static uint32_t LOG2 = 0;

    // timer service
    TimerService_Dispatch();

    // application
    if ((get_systick() % 1000) == 0)
    {
        // printf("%s(systick) : %4d\r\n",__FUNCTION__,LOG2++);    
    }

    #if CRYSTAL_LESS
    USB_trim_process();
    #endif
    
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		printf("press : %c\r\n" , res);
		switch(res)
		{
			case '1':
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
                SYS_UnlockReg();
				// NVIC_SystemReset();	// Reset I/O and peripherals , only check BS(FMC_ISPCTL[1])
                // SYS_ResetCPU();     // Not reset I/O and peripherals
                SYS_ResetChip();    // Reset I/O and peripherals ,  BS(FMC_ISPCTL[1]) reload from CONFIG setting (CBS)	
				break;
		}
	}
}

void UART02_IRQHandler(void)
{
    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
			UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART02_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	dbg_printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	dbg_printf("CLK_GetHCLKFreq : %8d\r\n",CLK_GetHCLKFreq());
	dbg_printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	dbg_printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	dbg_printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	dbg_printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	
	#endif	

    #if 0
    dbg_printf("FLAG_PROJ_TIMER_PERIOD_1000MS : 0x%2X\r\n",FLAG_PROJ_TIMER_PERIOD_1000MS);
    dbg_printf("FLAG_PROJ_REVERSE1 : 0x%2X\r\n",FLAG_PROJ_REVERSE1);
    dbg_printf("FLAG_PROJ_REVERSE2 : 0x%2X\r\n",FLAG_PROJ_REVERSE2);
    dbg_printf("FLAG_PROJ_REVERSE3 : 0x%2X\r\n",FLAG_PROJ_REVERSE3);
    dbg_printf("FLAG_PROJ_REVERSE4 : 0x%2X\r\n",FLAG_PROJ_REVERSE4);
    dbg_printf("FLAG_PROJ_REVERSE5 : 0x%2X\r\n",FLAG_PROJ_REVERSE5);
    dbg_printf("FLAG_PROJ_REVERSE6 : 0x%2X\r\n",FLAG_PROJ_REVERSE6);
    dbg_printf("FLAG_PROJ_REVERSE7 : 0x%2X\r\n",FLAG_PROJ_REVERSE7);
    #endif

}

void GPIO_Init (void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB14MFP_Msk)) | (SYS_GPB_MFPH_PB14MFP_GPIO);
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB15MFP_Msk)) | (SYS_GPB_MFPH_PB15MFP_GPIO);

    GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PB, BIT15, GPIO_MODE_OUTPUT);	

}

void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) to input mode */
    PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);
    
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);	

//    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);	

    /* Select HCLK clock source as HIRC and HCLK source divider as 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /***********************************/
    // CLK_EnableModuleClock(TMR0_MODULE);
  	// CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HIRC, 0);

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);
    
	/***********************************/
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));
	
    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk)) |
                    (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

	/***********************************/

    CLK_EnableModuleClock(ADC_MODULE);	
    CLK_SetModuleClock(ADC_MODULE, CLK_CLKSEL2_ADCSEL_PCLK1, CLK_CLKDIV0_ADC(3));

    SYS->GPB_MFPL = (SYS->GPB_MFPL &~(SYS_GPB_MFPL_PB6MFP_Msk | SYS_GPB_MFPL_PB7MFP_Msk)) \
                    | (SYS_GPB_MFPL_PB6MFP_ADC0_CH6 | SYS_GPB_MFPL_PB7MFP_ADC0_CH7) ;

    SYS->GPB_MFPH = (SYS->GPB_MFPH &~(SYS_GPB_MFPH_PB8MFP_Msk| SYS_GPB_MFPH_PB9MFP_Msk)) \
                    | (SYS_GPB_MFPH_PB8MFP_ADC0_CH8| SYS_GPB_MFPH_PB9MFP_ADC0_CH9) ;

    /* Set to input mode */
    GPIO_SetMode(PB, BIT6|BIT7|BIT8|BIT9, GPIO_MODE_INPUT);

    /* Disable the GPIO digital input path to avoid the leakage current. */
    GPIO_DISABLE_DIGITAL_PATH(PB, BIT6|BIT7|BIT8|BIT9);

	/***********************************/
	
    /* Switch USB clock source to HIRC & USB Clock = HIRC / 1 */
    CLK_SetModuleClock(USBD_MODULE, CLK_CLKSEL0_USBDSEL_HIRC, CLK_CLKDIV0_USB(1));

    /* Enable USB clock */
    CLK_EnableModuleClock(USBD_MODULE);		

	/***********************************/
		
   /* Update System Core Clock */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}

/*
 * This is a template project for M031 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 *
 * This template application uses external crystal as HCLK source and configures UART0 to print out
 * "Hello World", users may need to do extra system configuration based on their system design.
 */

int main()
{
    SYS_Init();

	GPIO_Init();
	UART0_Init();
	TIMER1_Init();
    check_reset_source();

    SysTick_enable(1000);
    #if defined (ENABLE_TICK_EVENT)
    TickSetTickEvent(1000, TickCallback_processA);  // 1000 ms
    TickSetTickEvent(5000, TickCallback_processB);  // 5000 ms
    #endif

    /* Enable ADC converter */
    ADC_POWER_ON(ADC);

    /*Wait for ADC internal power ready*/
    delay_ms(10);

	ADC_Process(State_avergage);

    TimerService_Init();    
#if (HID_JOYSTICK_USE_BUTTON_SW_DEBOUNCE != 0U)
    ButtonSwDebounce_Init();
#endif
    TimerService_CreateTask();

    USB_HID_Init();
#if (HID_JOYSTICK_USE_BUTTON_SW_DEBOUNCE != 0U)
    /*
     * Use external input source (ADC/buttons provided by app path).
     */
    HidTool_EnableExternalInput(1U);
#endif
    
    /*
        [ADC]
        A0 : PB7 (X-AXIS)
        A1 : PB6 (Y-AXIS)

        [GPIO]
        D8 : K  //PA5
        D7 : F  //PA6
        D6 : E  //PA7
        D5 : D  //PC2
        D4 : C  //PC3
        D3 : B  //PC5
        D2 : A  //PC4


        [MAPPING-L] note : change I/O base on PCB condition
        GUI     - GAMEPAD   - GPIO
        U       - C         - PC3
        D       - A         - PC4
        L       - B         - PC5
        R       - D         - PC2

        L1(B5)  - E         - PA7
        L2(B7)  - F         - PA6 
        L3(B11) - K         - PA5
        L3      - X-AXIS    - PB7 (ADC0_CH7)
        L3      - Y-AXIS    - PB6 (ADC0_CH6)

        SEL(B9) - X
        STA(B10)- X
        
        [MAPPING-R] note : change I/O base on PCB condition
        R1(B6)  - E         - PA7
        R2(B8)  - F         - PA6 
        R3(B12) - K         - PA5
        R3      - X-AXIS    - PB7 (ADC0_CH7)
        R3      - Y-AXIS    - PB6 (ADC0_CH6)

        Y(B4)   - C         - PC3
        A(B2)   - A         - PC4
        X(B1)   - B         - PC5
        B(B3)   - D         - PC2        

    */


    /* Got no where to go, just loop forever */
    while(1)
    {
        loop();

    }
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
