#ifndef __HID_JOYSTICK_CONF_H__
#define __HID_JOYSTICK_CONF_H__

#include "NuMicro.h"

/*
 * Centralized joystick configuration.
 * Keep user-tunable values in this file for quick board migration.
 */

/* ----------------------------- Report profile ----------------------------- */
#define HID_JOYSTICK_REPORT_SIZE                    8U
#define HID_JOYSTICK_AXIS_COUNT                     4U   /* X, Y, Z, Rz */
#define HID_JOYSTICK_BUTTON_COUNT                   12U
#define HID_JOYSTICK_BUTTON_LAYOUT_CAPACITY         12U

#if (HID_JOYSTICK_BUTTON_COUNT == 0U) || (HID_JOYSTICK_BUTTON_COUNT > HID_JOYSTICK_BUTTON_LAYOUT_CAPACITY)
#error "HID_JOYSTICK_BUTTON_COUNT must be 1..12 for current 8-byte layout."
#endif

/* ------------------------------ ADC profile ------------------------------- */
#define HID_JOYSTICK_ADC_BUF_MAX_SAMPLES            128U
#define HID_JOYSTICK_ADC_DEFAULT_SAMPLES            64U
#define HID_JOYSTICK_ADC_DEFAULT_MIN                0U
#define HID_JOYSTICK_ADC_DEFAULT_CENTER             2048U
#define HID_JOYSTICK_ADC_DEFAULT_MAX                4095U
/* Axis filter on ADC domain (applies before ADC->HID conversion). */
#define HID_JOYSTICK_AXIS_FILTER_ENABLE             1U
#define HID_JOYSTICK_AXIS_DEADZONE_ADC              40U
#define HID_JOYSTICK_AXIS_HYSTERESIS_ENABLE         1U
#define HID_JOYSTICK_AXIS_HYSTERESIS_ADC            8U

/* --------------------------- Reserved byte data --------------------------- */
#define HID_JOYSTICK_RSVD0_DEFAULT                  0x04U
#define HID_JOYSTICK_RSVD1_DEFAULT                  0xFCU

/* --------------------------- Button SW debounce --------------------------- */
/*
 * Integration switch:
 *   0: keep existing behavior (no button debounce module hook in main.c)
 *   1: enable button_sw_debounce integration in main.c
 */
#define HID_JOYSTICK_USE_BUTTON_SW_DEBOUNCE         1U

/* Electrical active level: pressed=LOW(0) or pressed=HIGH(1). */
#define HID_JOYSTICK_BUTTON_ACTIVE_LEVEL            0U

/* Debounce time in 1ms ticks (used by button_sw_debounce.c). */
#define HID_JOYSTICK_BUTTON_DEBOUNCE_PRESS_TICKS    5U
#define HID_JOYSTICK_BUTTON_DEBOUNCE_RELEASE_TICKS  5U

/* Debug trace for debounce/button->POV path (UART printf). */
#define HID_JOYSTICK_DEBUG_BUTTON_PATH              0U

/* Debug trace for USB IN path (bus/setup/EP2/TX sequence heartbeat). */
#define HID_JOYSTICK_DEBUG_USB_PATH                 0U
#define HID_JOYSTICK_DEBUG_TX_PATH                  0U

/*
 * SOF clear policy in USBD IRQ:
 *   0: keep SOFIF for main-loop USB_trim_process() (recommended for crystal-less)
 *   1: clear SOFIF immediately in IRQ
 */
#define HID_JOYSTICK_USBD_CLEAR_SOF_IN_ISR          0U

/* Production GPIO template (16 digital inputs).
 * POV: U/D/L/R
 * Buttons: L1/L2/L3/R1/R2/R3/X/A/B/Y/SELECT/START
 * Replace PORT/PIN values with final PCB routing. */

/*

     -----------------------------------------------------------
    |                L1                          R1             |
    |                L2                          R2             |
    |        U                                           Y      |
    |        |                                           |      |
    |    L  -.-  R                                   X  -.-  B  |
    |        |                                           |      |
    |        D           SELECT      START               A      |
    |                                                           |
    |                L3                         R3              |
     -----------------------------------------------------------

*/


#define HID_JOYSTICK_GPIO_DPAD_UP_PORT              PC
#define HID_JOYSTICK_GPIO_DPAD_UP_PIN               3U
#define HID_JOYSTICK_GPIO_DPAD_DOWN_PORT            PC
#define HID_JOYSTICK_GPIO_DPAD_DOWN_PIN             4U
#define HID_JOYSTICK_GPIO_DPAD_LEFT_PORT            PC
#define HID_JOYSTICK_GPIO_DPAD_LEFT_PIN             5U
#define HID_JOYSTICK_GPIO_DPAD_RIGHT_PORT           PC
#define HID_JOYSTICK_GPIO_DPAD_RIGHT_PIN            2U

#define HID_JOYSTICK_GPIO_L1_PORT                   PA
#define HID_JOYSTICK_GPIO_L1_PIN                    7U
#define HID_JOYSTICK_GPIO_L2_PORT                   PA
#define HID_JOYSTICK_GPIO_L2_PIN                    6U
#define HID_JOYSTICK_GPIO_L3_PORT                   PA
#define HID_JOYSTICK_GPIO_L3_PIN                    5U

#define HID_JOYSTICK_GPIO_R1_PORT                   PA
#define HID_JOYSTICK_GPIO_R1_PIN                    7U
#define HID_JOYSTICK_GPIO_R2_PORT                   PA
#define HID_JOYSTICK_GPIO_R2_PIN                    6U
#define HID_JOYSTICK_GPIO_R3_PORT                   PA
#define HID_JOYSTICK_GPIO_R3_PIN                    5U

#define HID_JOYSTICK_GPIO_X_PORT                    PC
#define HID_JOYSTICK_GPIO_X_PIN                     5U
#define HID_JOYSTICK_GPIO_A_PORT                    PC
#define HID_JOYSTICK_GPIO_A_PIN                     4U
#define HID_JOYSTICK_GPIO_B_PORT                    PC
#define HID_JOYSTICK_GPIO_B_PIN                     2U
#define HID_JOYSTICK_GPIO_Y_PORT                    PC
#define HID_JOYSTICK_GPIO_Y_PIN                     3U
#define HID_JOYSTICK_GPIO_SELECT_PORT               PB
#define HID_JOYSTICK_GPIO_SELECT_PIN                10U
#define HID_JOYSTICK_GPIO_START_PORT                PB
#define HID_JOYSTICK_GPIO_START_PIN                 11U

/* Debounce input set: DPad(4) + L(3) + R(3) + ABXY(4) + Select/Start(2) = 16 */
#define HID_JOYSTICK_GPIO_INPUT_COUNT               16U
#define HID_JOYSTICK_SIG_BIT_DPAD_UP                (1U << 0)
#define HID_JOYSTICK_SIG_BIT_DPAD_DOWN              (1U << 1)
#define HID_JOYSTICK_SIG_BIT_DPAD_LEFT              (1U << 2)
#define HID_JOYSTICK_SIG_BIT_DPAD_RIGHT             (1U << 3)
#define HID_JOYSTICK_SIG_BIT_L1                     (1U << 4)
#define HID_JOYSTICK_SIG_BIT_L2                     (1U << 5)
#define HID_JOYSTICK_SIG_BIT_L3                     (1U << 6)
#define HID_JOYSTICK_SIG_BIT_R1                     (1U << 7)
#define HID_JOYSTICK_SIG_BIT_R2                     (1U << 8)
#define HID_JOYSTICK_SIG_BIT_R3                     (1U << 9)
#define HID_JOYSTICK_SIG_BIT_X                      (1U << 10)
#define HID_JOYSTICK_SIG_BIT_A                      (1U << 11)
#define HID_JOYSTICK_SIG_BIT_B                      (1U << 12)
#define HID_JOYSTICK_SIG_BIT_Y                      (1U << 13)
#define HID_JOYSTICK_SIG_BIT_SELECT                 (1U << 14)
#define HID_JOYSTICK_SIG_BIT_START                  (1U << 15)

/* HID button bit helper: Button1 -> bit0, Button12 -> bit11 */
#define HID_JOYSTICK_HID_BTN_BIT(btn_idx)           (1UL << ((btn_idx) - 1UL))

/*
 * Active mapping profile:
 * - LEFT : output left-half mapping only
 * - RIGHT: output right-half mapping only
 * - BOTH : merge left+right outputs (DPAD/Buttons are OR-combined)
 */
#define HID_JOYSTICK_MAP_PROFILE_LEFT               0U
#define HID_JOYSTICK_MAP_PROFILE_RIGHT              1U
#define HID_JOYSTICK_MAP_PROFILE_BOTH               2U
#define HID_JOYSTICK_ACTIVE_MAP_PROFILE             HID_JOYSTICK_MAP_PROFILE_BOTH

/* [MAPPING-L]
 * UDLR -> POV
 * L1(B5), L2(B7), L3(B11), SELECT(B9), START(B10) */
#define HID_JOYSTICK_MAP_L_DPAD_UP_SRC_MASK         HID_JOYSTICK_SIG_BIT_DPAD_UP
#define HID_JOYSTICK_MAP_L_DPAD_DOWN_SRC_MASK       HID_JOYSTICK_SIG_BIT_DPAD_DOWN
#define HID_JOYSTICK_MAP_L_DPAD_LEFT_SRC_MASK       HID_JOYSTICK_SIG_BIT_DPAD_LEFT
#define HID_JOYSTICK_MAP_L_DPAD_RIGHT_SRC_MASK      HID_JOYSTICK_SIG_BIT_DPAD_RIGHT

#define HID_JOYSTICK_MAP_L_BTN1_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN2_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN3_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN4_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN5_SRC_MASK            HID_JOYSTICK_SIG_BIT_L1
#define HID_JOYSTICK_MAP_L_BTN6_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN7_SRC_MASK            HID_JOYSTICK_SIG_BIT_L2
#define HID_JOYSTICK_MAP_L_BTN8_SRC_MASK            0U
#define HID_JOYSTICK_MAP_L_BTN9_SRC_MASK            HID_JOYSTICK_SIG_BIT_SELECT
#define HID_JOYSTICK_MAP_L_BTN10_SRC_MASK           HID_JOYSTICK_SIG_BIT_START
#define HID_JOYSTICK_MAP_L_BTN11_SRC_MASK           HID_JOYSTICK_SIG_BIT_L3
#define HID_JOYSTICK_MAP_L_BTN12_SRC_MASK           0U

/* [MAPPING-R]
 * X(B1), A(B2), B(B3), Y(B4), R1(B6), R2(B8), R3(B12)
 * D-pad is disabled in RIGHT profile. */
#define HID_JOYSTICK_MAP_R_DPAD_UP_SRC_MASK         0U
#define HID_JOYSTICK_MAP_R_DPAD_DOWN_SRC_MASK       0U
#define HID_JOYSTICK_MAP_R_DPAD_LEFT_SRC_MASK       0U
#define HID_JOYSTICK_MAP_R_DPAD_RIGHT_SRC_MASK      0U

#define HID_JOYSTICK_MAP_R_BTN1_SRC_MASK            HID_JOYSTICK_SIG_BIT_X
#define HID_JOYSTICK_MAP_R_BTN2_SRC_MASK            HID_JOYSTICK_SIG_BIT_A
#define HID_JOYSTICK_MAP_R_BTN3_SRC_MASK            HID_JOYSTICK_SIG_BIT_B
#define HID_JOYSTICK_MAP_R_BTN4_SRC_MASK            HID_JOYSTICK_SIG_BIT_Y
#define HID_JOYSTICK_MAP_R_BTN5_SRC_MASK            0U
#define HID_JOYSTICK_MAP_R_BTN6_SRC_MASK            HID_JOYSTICK_SIG_BIT_R1
#define HID_JOYSTICK_MAP_R_BTN7_SRC_MASK            0U
#define HID_JOYSTICK_MAP_R_BTN8_SRC_MASK            HID_JOYSTICK_SIG_BIT_R2
#define HID_JOYSTICK_MAP_R_BTN9_SRC_MASK            HID_JOYSTICK_SIG_BIT_SELECT
#define HID_JOYSTICK_MAP_R_BTN10_SRC_MASK           HID_JOYSTICK_SIG_BIT_START
#define HID_JOYSTICK_MAP_R_BTN11_SRC_MASK           0U
#define HID_JOYSTICK_MAP_R_BTN12_SRC_MASK           HID_JOYSTICK_SIG_BIT_R3

#endif /* __HID_JOYSTICK_CONF_H__ */
