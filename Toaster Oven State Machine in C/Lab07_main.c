// Included Libaries
#include <stdio.h>
#include <string.h>

// CMPE13 Support Library
#include "Adc.h"
#include "Ascii.h"
#include "BOARD.h"
#include "Buttons.h"
#include "Leds.h"
#include "Oled.h"
#include "OledDriver.h"

// Microchip libraries
#include <xc.h>
#include <sys/attribs.h>

// Marco Values
#define TRUE 1
#define FALSE 0

// State values
#define SETUP                   SETUP
#define SELECTOR_CHANGE_PENDING SELECTOR_CHANGE_PENDING  
#define COOKING                 COOKING
#define RESET_PENDING           RESET_PENDING

// Oven values
#define BAKE                    Bake
#define TOAST                   Toast
#define BROIL                   Broil

// Oven printing values
#define TOP_OVEN_OFF            "\x02\x02\x02\x02\x02"
#define TOP_OVEN_ON             "\x01\x01\x01\x01\x01"
#define BOTTOM_OVEN_OFF         "\x04\x04\x04\x04\x04"
#define BOTTOM_OVEN_ON          "\x03\x03\x03\x03\x03"
#define BREAK_LINE              "-----"
#define EIGHT_SPACE             "\x20\x20\x20\x20\x20"
#define ARROW                   ">"
#define SPACE_FREE              "\x20"

// Oven printing arrays
static char ovenPrintL1[100], 
            ovenPrintL2[100], 
            ovenPrintL3[100], 
            ovenPrintL4[100],
            ovenOption[5];

// Precision values
static uint8_t  ButtonAction = 0,
                PrevButtonAction = 0,
                CheckAdcValue = 0;
static uint16_t AdcValue = 0;

// Program Functionality values
static int  Adc_Min = 1,
            Adc_Sec = 15,
            AdcToMin = 0,
            AdcToSec = 0,
            AdcToTemp = 0,
            BAKE_SETUP_Init = 1,
            SETTINGS_SELECTOR = 1,
            TOAST_SETUP_Init = 0,
            BROIL_SETUP_Init = 0,
            TIMER_TICK = 0,
            PREV_TIMER_TICK = 0,
            B3_Init = 0,
            B4_Init = 0,
            B4_InitLED = 0,
            B4_Start = 0,
            B3_Start = 0,
            B3_Stop = 0,
            B4_Stop = 0,
            B3_Elapsed = 0,
            B4_Elapsed = 0,
            UPDATE_TIME_TEMP = 0,
            UPDATE_LED_FLAG = 0,
            LEDD = 1,
            perTime = 0,
            KeepLEDOn = 1;

// Flag values
static int  C7_FLAG = TRUE,
            C6_FLAG = TRUE,
            C5_FLAG = TRUE,
            C4_FLAG = TRUE,
            C3_FLAG = TRUE,
            C2_FLAG = TRUE,
            C1_FLAG = TRUE,
            C0_FLAG = TRUE;

// Set a macro for resetting the timer, makes the code a little clearer.
#define TIMER_2HZ_RESET() (TMR1 = 0)

// Establish typedef values
typedef enum
{
    SETUP, SELECTOR_CHANGE_PENDING, COOKING, RESET_PENDING
} OvenState;

typedef enum 
{
    BAKE, TOAST, BROIL
} cookSetting;

typedef struct ovenState 
{
    OvenState state;
    cookSetting mode;
    int cookTemp;
    int16_t cookTime;
    int16_t cookTimeRemaining;
    int16_t originalTime;
    int8_t Button_Event;
    int8_t Button3_Event;
    int8_t Button4_Event;
    int8_t Tick_Event;
    int8_t ADC_Event;
} OvenData;

// Establish datatypes
static OvenData ovenOven = {};

// Update OLED to reflect the state
void updateOvenOLED(OvenData ovenData)
{
    // Include the arrow sign here in order to indicate to the user what mode they have selected
    OledClear(0);

    // Custom Printing
    if ((ovenOven.state == COOKING || ovenOven.state == RESET_PENDING) && KeepLEDOn == 1) 
    {
        // Custom printing for BAKE
        if (ovenOven.mode == BAKE) 
        {
            char heatingPrintTOP[] = TOP_OVEN_ON;
            char heatingPrintBOTTOM[] = BOTTOM_OVEN_ON;
            sprintf(ovenPrintL1, "|%s|   Mode: %s", heatingPrintTOP, ovenOption);
            sprintf(ovenPrintL4, "\n\n\n|%s|", heatingPrintBOTTOM);
        }

        // Custom printing for TOAST
        if (ovenOven.mode == TOAST) 
        {
            char heatingPrintTOP[] = TOP_OVEN_OFF;
            char heatingPrintBOTTOM[] = BOTTOM_OVEN_ON;
            sprintf(ovenPrintL1, "|%s|   Mode: %s", heatingPrintTOP, ovenOption);
            sprintf(ovenPrintL4, "\n\n\n|%s|", heatingPrintBOTTOM);
        }

        // Custom printing for BROIL
        if (ovenOven.mode == BROIL) 
        {
            char heatingPrintTOP[] = TOP_OVEN_ON;
            char heatingPrintBOTTOM[] = BOTTOM_OVEN_OFF;
            sprintf(ovenPrintL1, "|%s|   Mode: %s", heatingPrintTOP, ovenOption);
            sprintf(ovenPrintL4, "\n\n\n|%s|", heatingPrintBOTTOM);
        }
    }

    // No custom printing
    else
    {
        char heatingPrintTOP[] = TOP_OVEN_OFF;
        char heatingPrintBOTTOM[] = BOTTOM_OVEN_OFF;
        sprintf(ovenPrintL1, "|%s|   Mode: %s", heatingPrintTOP, ovenOption);
        sprintf(ovenPrintL4, "\n\n\n|%s|", heatingPrintBOTTOM);
    }

    // Custom Printing for SETTINGS_SELECTOR
    if (ovenOven.mode == BAKE && SETTINGS_SELECTOR == 1)
    {
        char arrow[] = ARROW;
        sprintf(ovenPrintL2, "\n|%s|  %sTime: %d:%.2d", EIGHT_SPACE, arrow, Adc_Min, Adc_Sec);
        sprintf(ovenPrintL3, "\n\n|%s|   Temp: %d%sF", BREAK_LINE, ovenOven.cookTemp, DEGREE_SYMBOL);
    } 
    else if (ovenOven.mode == BAKE && SETTINGS_SELECTOR == -1) 
    {
        char arrow[] = ARROW;
        sprintf(ovenPrintL2, "\n|%s|   Time: %d:%.2d", EIGHT_SPACE, Adc_Min, Adc_Sec);
        sprintf(ovenPrintL3, "\n\n|%s|  %sTemp: %d%sF", BREAK_LINE, arrow, ovenOven.cookTemp, DEGREE_SYMBOL);
    }

    // CUSTOM Printing for modes that are not BAKE
    if (ovenOven.mode != BAKE)
    {
        sprintf(ovenPrintL2, "\n|%s|   Time: %d:%.2d", EIGHT_SPACE, Adc_Min, Adc_Sec);
        if (ovenOven.mode != TOAST)
        {
            sprintf(ovenPrintL3, "\n\n|%s|   Temp: %d%sF", BREAK_LINE, ovenOven.cookTemp, DEGREE_SYMBOL);
        } 
        else sprintf(ovenPrintL3, "\n\n|%s|", BREAK_LINE);
    }

    // Update OLED Screen
    OledDrawString(ovenPrintL1);
    OledDrawString(ovenPrintL2);
    OledDrawString(ovenPrintL3);
    OledDrawString(ovenPrintL4);
    OledUpdate();
}

// Event programmed state machine
void runOvenSM(void)
{
    // Machine States
    switch (ovenOven.state) 
    {
        case SETUP:
            // Initialize Flags
            if (UPDATE_LED_FLAG == TRUE)
            {
                C7_FLAG = TRUE;
                C6_FLAG = TRUE;
                C5_FLAG = TRUE;
                C4_FLAG = TRUE;
                C3_FLAG = TRUE;
                C2_FLAG = TRUE;
                C1_FLAG = TRUE;
                C0_FLAG = TRUE;
                PREV_TIMER_TICK = FALSE;
                UPDATE_LED_FLAG = FALSE;
            }
            
            // Initialize BAKE
            if (BAKE_SETUP_Init == 1) 
            {
                ovenOven.mode = BAKE;
                ovenOven.cookTime = 1;
                ovenOven.cookTimeRemaining = ovenOven.cookTime;
                ovenOven.ADC_Event = FALSE;
                ovenOven.Button_Event = FALSE;
                ovenOven.Button3_Event = FALSE;
                ovenOven.Button4_Event = FALSE;
                ovenOven.Tick_Event = FALSE;
                ovenOven.cookTemp = 350;
                BAKE_SETUP_Init = 0;
                sprintf(ovenOption, "BAKE");
            } 
            
            // Initialize Toast
            else if (TOAST_SETUP_Init == 1) 
            {
                ovenOven.mode = TOAST;
                ovenOven.cookTime = 1;
                ovenOven.cookTimeRemaining = ovenOven.cookTime;
                ovenOven.ADC_Event = FALSE;
                ovenOven.Button_Event = FALSE;
                ovenOven.Button3_Event = FALSE;
                ovenOven.Button4_Event = FALSE;
                ovenOven.Tick_Event = FALSE;
                SETTINGS_SELECTOR = 1;
                TOAST_SETUP_Init = 0;
                sprintf(ovenOption, "TOAST");
            }
            
            // Initialize Broil
            else if (BROIL_SETUP_Init == 1) 
            {
                ovenOven.mode = BROIL;
                ovenOven.cookTime = 1;
                ovenOven.cookTimeRemaining = ovenOven.cookTime;
                ovenOven.ADC_Event = FALSE;
                ovenOven.Button_Event = FALSE;
                ovenOven.Button3_Event = FALSE;
                ovenOven.Button4_Event = FALSE;
                ovenOven.Tick_Event = FALSE;
                ovenOven.cookTemp = 500;
                BROIL_SETUP_Init = 0;
                SETTINGS_SELECTOR = 1;
                sprintf(ovenOption, "BROIL");
            }

            // Update time
            else if (UPDATE_TIME_TEMP == TRUE) 
            {
                AdcToMin = ovenOven.cookTime / 60;
                Adc_Min = AdcToMin;
                AdcToSec = ovenOven.cookTime % 60;
                Adc_Sec = AdcToSec;
                UPDATE_TIME_TEMP = FALSE;
            }

            // Condition for Button3 Event Down
            if (ButtonAction & BUTTON_EVENT_3DOWN) ovenOven.Button3_Event = TRUE;
            else if (ButtonAction & BUTTON_EVENT_3UP) ovenOven.Button3_Event = FALSE;

            // Condition for Button4 Event Down
            if (ButtonAction & BUTTON_EVENT_4DOWN) 
            {
                ovenOven.Button4_Event = TRUE;
                PrevButtonAction = BUTTON_EVENT_4DOWN;
            } 
            else if (ButtonAction & BUTTON_EVENT_4UP && (PrevButtonAction == BUTTON_EVENT_4DOWN))
            {
                ovenOven.Button4_Event = FALSE;
                B4_Init = 1;
                B4_InitLED = 1;
                ovenOven.state = COOKING;
            }
            
            // Timer Update
            if (ovenOven.Button3_Event == TRUE && B3_Init == 0) 
            {
                B3_Start = TIMER_TICK;
                B3_Init = 1;
            } 
            else if (ovenOven.Button3_Event == FALSE && B3_Init == 1) 
            {
                B3_Stop = TIMER_TICK;
                B3_Elapsed = B3_Stop - B3_Start;
                B3_Init = 0;
                ovenOven.state = SELECTOR_CHANGE_PENDING;
            }
            
            // Potentiometer Mandatory COnversion
            if (CheckAdcValue == TRUE) 
            {
                AdcValue = AdcRead();
                AdcValue /= 4;
                AdcValue += 1;
            }
            
            // Potentiometer Control for TIME
            if (CheckAdcValue == TRUE && SETTINGS_SELECTOR == 1) 
            {
                ovenOven.cookTime = AdcValue;
                ovenOven.cookTimeRemaining = AdcValue;
                AdcToMin = AdcValue / 60;
                Adc_Min = AdcToMin;
                AdcToSec = AdcValue % 60;
                Adc_Sec = AdcToSec;
            }

            //  Potentiometer Control for TEMPERATURE
            else if (CheckAdcValue == TRUE && SETTINGS_SELECTOR == -1 && ovenOven.mode == BAKE) 
            {
                if (AdcValue != 0) AdcValue -= 1;
                AdcToTemp = AdcValue + 300;
                ovenOven.cookTemp = AdcToTemp;
            }
            break;
        case SELECTOR_CHANGE_PENDING:
            if (B3_Elapsed < 5) 
            {
                if (ovenOven.mode == BAKE)
                {
                    ovenOven.mode = TOAST;
                    TOAST_SETUP_Init = TRUE;
                } 
                else if (ovenOven.mode == TOAST)
                {
                    ovenOven.mode = BROIL;
                    BROIL_SETUP_Init = TRUE;
                } 
                else if (ovenOven.mode == BROIL)
                {
                    ovenOven.mode = BAKE;
                    BAKE_SETUP_Init = TRUE;
                }
            } 
            else if (B3_Elapsed > 5 && ovenOven.mode == BAKE) SETTINGS_SELECTOR *= -1;
            ovenOven.state = SETUP;
            break;
        case COOKING:
            KeepLEDOn = 1;
            if (B4_InitLED == 1) 
            {
                LEDS_SET(0xFF);
                B4_InitLED = 0;
            }
            if (B4_Init == 1) 
            {
                if (PREV_TIMER_TICK != TIMER_TICK) 
                {
                    if (ovenOven.cookTimeRemaining > 0 && (TIMER_TICK % 5 == 0)) 
                    {
                        ovenOven.cookTimeRemaining--;
                        AdcToMin = ovenOven.cookTimeRemaining / 60;
                        Adc_Min = AdcToMin;
                        AdcToSec = ovenOven.cookTimeRemaining % 60;
                        Adc_Sec = AdcToSec;
                        perTime = ovenOven.cookTimeRemaining * 100;
                        perTime /= ovenOven.cookTime;

                        // Tree used to decrement the LEDS via the time remaining.
                        if (perTime <= 84 && perTime > 72 && (C7_FLAG == TRUE)) 
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C7_FLAG = FALSE;
                        }
                        else if (perTime <= 72 && perTime > 60 && (C6_FLAG == TRUE)) 
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C6_FLAG = FALSE;
                        }
                        else if (perTime <= 60 && perTime > 48 && (C5_FLAG == TRUE)) 
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C5_FLAG = FALSE;
                        }
                        else if (perTime <= 48 && perTime > 36 && (C4_FLAG == TRUE)) 
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C4_FLAG = FALSE;
                        } 
                        else if (perTime <= 36 && perTime > 24 && (C3_FLAG == TRUE))
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C3_FLAG = FALSE;
                        }
                        else if (perTime <= 24 && perTime > 12 && (C2_FLAG == TRUE))
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C2_FLAG = FALSE;
                        }
                        else if (perTime <= 12 && (C1_FLAG == TRUE))
                        {
                            LEDS_SET(0xFF << LEDD);
                            LEDD++;
                            C1_FLAG = FALSE;
                        }
                        else if (ovenOven.cookTimeRemaining == 0)
                        {
                            LEDD++;
                            LEDS_SET(0xFF << LEDD);
                        }
                    }
                    else if (ovenOven.cookTimeRemaining == 0 && (TIMER_TICK % 5 == 0))
                    {
                        B4_Init = 0;
                        UPDATE_TIME_TEMP = TRUE;
                        UPDATE_LED_FLAG = TRUE;
                        LEDD = 1;
                        ovenOven.cookTimeRemaining = ovenOven.cookTime;
                        ovenOven.state = SETUP;
                    }
                }
                PREV_TIMER_TICK = TIMER_TICK;
            }
            if (ButtonAction & BUTTON_EVENT_4DOWN)
            {
                B4_Start = TIMER_TICK;
                ovenOven.state = RESET_PENDING;
            }
            break;
        case RESET_PENDING:
            if (ButtonAction & BUTTON_EVENT_4UP)
            {
                B4_Stop = TIMER_TICK;
                B4_Elapsed = B4_Stop - B4_Start;
                if (B4_Elapsed >= 5)
                {
                    ovenOven.cookTimeRemaining = ovenOven.cookTime;
                    UPDATE_TIME_TEMP = 1;
                    LEDS_SET(0x00);
                    KeepLEDOn = 0;
                    ovenOven.state = SETUP;
                } 
                else ovenOven.state = COOKING;
            }
            break;
    }

    // Updating OLED if only EVENT
    if (ovenOven.ADC_Event == TRUE ||
        ovenOven.Button_Event == TRUE) updateOvenOLED(ovenOven);

    // Reset flags
    ovenOven.Tick_Event = FALSE;
    ovenOven.ADC_Event = FALSE;
    ovenOven.Button_Event = FALSE;
}

int main()
{
    // Initialize HW components on Board
    LEDS_INIT();
    AdcInit();
    BOARD_Init();
    ButtonsInit();
    OledInit();

    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

    T2CON = 0;                              // everything should be off
    T2CONbits.TCKPS = 0b100;                // 1:16 prescaler
    PR2 = BOARD_GetPBClock() / 16 / 100;    // interrupt at .5s intervals
    T2CONbits.ON = 1;                       // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T2IF = 0; //clear the interrupt flag before configuring
    IPC2bits.T2IP = 4; // priority of  4
    IPC2bits.T2IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T2IE = 1; // turn the interrupt on

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescaler, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.

    T3CON = 0;                              // everything should be off
    T3CONbits.TCKPS = 0b111;                // 1:256 prescaler
    PR3 = BOARD_GetPBClock() / 256 / 5;     // interrupt at .5s intervals
    T3CONbits.ON = 1;                       // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T3IF = 0; //clear the interrupt flag before configuring
    IPC3bits.T3IP = 4; // priority of  4
    IPC3bits.T3IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T3IE = 1; // turn the interrupt on;

    printf("Welcome to jfrankne's Lab07 (Toaster Oven).  Compiled on %s %s.\n", __TIME__, __DATE__);

    // Initialize state machine
    ovenOven.state = SETUP;

    // Main Loop
    while (1)
    {
        // Event-checking
        if (ovenOven.Tick_Event == TRUE ||
                ovenOven.ADC_Event == TRUE ||
                ovenOven.Button_Event == TRUE) runOvenSM();
    }
}

// The 5hz timer is used to update the free-running timer and to generate TIMER_TICK events
void __ISR(_TIMER_3_VECTOR, ipl4auto) TimerInterrupt5Hz(void)
{
    // Clear the interrupt flag.
    IFS0CLR = 1 << 12;

    // Event-checking code here
    TIMER_TICK++;
    ovenOven.Tick_Event = TRUE;
}

// The 100hz timer is used to check for button and ADC events
void __ISR(_TIMER_2_VECTOR, ipl4auto) TimerInterrupt100Hz(void)
{
    // Clear the interrupt flag.
    IFS0CLR = 1 << 8;

    // Event-checking code here
    ButtonAction = ButtonsCheckEvents();
    ovenOven.Button_Event = TRUE;
    CheckAdcValue = AdcChanged();
    ovenOven.ADC_Event = TRUE;
}