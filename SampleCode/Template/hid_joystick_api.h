#ifndef __HID_JOYSTICK_API_H__
#define __HID_JOYSTICK_API_H__

#include <stdint.h>
#include "hid_joystick_conf.h"

/* Joystick API layer extracted from hid_transfer.c */
void HidTool_ResetState(void);
void HidTool_OnOutReady(void);
/* Periodic debug/log task (optional, e.g. call every 100 ms). */
void HidTool_ProcessLogTask(void);
/* Legacy alias for HidTool_ProcessLogTask() to keep compatibility. */
void HidTool_Process(void);

void HidTool_SetInReport(void);
void HidTool_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size);
void HidTool_GetLatestInReport(uint8_t *pu8Report, uint32_t u32Size);

/* Axis selector for ADC/HID mapping */
typedef enum
{
    HID_TOOL_AXIS_X = 0,
    HID_TOOL_AXIS_Y = 1,
    HID_TOOL_AXIS_Z = 2,
    HID_TOOL_AXIS_RZ = 3,
    HID_TOOL_AXIS_COUNT = HID_JOYSTICK_AXIS_COUNT
} E_HID_TOOL_AXIS;

/* Integration guide (recommended)
 * ---------------------------------------------------------------------------
 * 1) One-time startup:
 *      HidTool_ResetState();
 *      HidTool_EnableExternalInput(1U);
 *
 * 2) Periodic update (example cadence used in this project):
 *      - 1 ms timer ISR/callback:
 *          ButtonSwDebounce_Timer1msTick();
 *          // Keep ISR short. If ADC read is slow, set a flag here.
 *
 *      - 10 ms task callback:
 *          ButtonSwDebounce_UpdateHidByProfile();
 *          HidTool_SetExternalAdc(adc_x, adc_y, adc_z, adc_rz);
 *
 *      - 100 ms task callback:
 *          HidTool_ProcessLogTask();  // optional debug/log task
 *
 * 3) USB IN report path:
 *      HidTool_SetInReport() is called by EP2 handler. App code should not
 *      call it directly in normal flow.
 *
 * Optional (without button_sw_debounce module):
 *      - Build GPIO state in 10 ms task callback, then call:
 *          HidTool_SetExternalButtons(button_mask);
 *          HidTool_SetExternalPovFromDpadMask(dpad_mask);
 *      - Example 10 ms task sequence:
 *          read_gpio();
 *          HidTool_SetExternalButtons(button_mask);
 *          HidTool_SetExternalPovFromDpadMask(dpad_mask);
 *          HidTool_SetExternalAdc(adc_x, adc_y, adc_z, adc_rz);
 *
 * Data range:
 *      HidTool_SetExternalAdc() expects 12-bit ADC values (0..4095).
 */

/* ADC playback buffer API (X/Y/Z/Rz) */
uint16_t HidTool_GetAdcBufferCapacity(void);
uint16_t HidTool_GetAdcBufferCount(void);
void HidTool_LoadDefaultAdcPattern(void);
uint16_t HidTool_LoadAdcBuffer(const uint16_t *pu16X,
                               const uint16_t *pu16Y,
                               const uint16_t *pu16Z,
                               uint16_t u16Count);
uint16_t HidTool_LoadAdcBufferEx(const uint16_t *pu16X,
                                 const uint16_t *pu16Y,
                                 const uint16_t *pu16Z,
                                 const uint16_t *pu16Rz,
                                 uint16_t u16Count);
void HidTool_SetAdcBufferSample(uint16_t u16Index,
                                uint16_t u16X,
                                uint16_t u16Y,
                                uint16_t u16Z);
void HidTool_SetAdcBufferSampleEx(uint16_t u16Index,
                                  uint16_t u16X,
                                  uint16_t u16Y,
                                  uint16_t u16Z,
                                  uint16_t u16Rz);
void HidTool_SetAdcBufferIndex(uint16_t u16Index);

/* ADC-to-HID axis calibration API */
void HidTool_ResetAdcCalibration(void);
void HidTool_SetAdcCalibration(uint8_t u8Axis,
                               uint16_t u16Min,
                               uint16_t u16Center,
                               uint16_t u16Max);

/* Enable/disable direct-input mode.
 * u8Enable=1: use external ADC/button data.
 * u8Enable=0: use built-in playback pattern. */
void HidTool_EnableExternalInput(uint8_t u8Enable);

/* Inject 12-bit ADC values (0..4095) for X/Y/Z/Rz. */
void HidTool_SetExternalAdc(uint16_t u16X, uint16_t u16Y, uint16_t u16Z, uint16_t u16Rz);

/* Inject POV(Hat) value: 0=Up, 1=Up-Right, 2=Right, ... 7=Up-Left, 8=center.
 * POV is a direction hat field (not generic button bits), so it has dedicated
 * 4-bit encoding in report Byte4 low nibble. */
void HidTool_SetExternalPov(uint8_t u8Pov);

/* D-pad GPIO mask helper for POV (Hat) conversion:
 * bit0=UP, bit1=DOWN, bit2=LEFT, bit3=RIGHT
 */
#define HID_TOOL_DPAD_UP       (1U << 0)
#define HID_TOOL_DPAD_DOWN     (1U << 1)
#define HID_TOOL_DPAD_LEFT     (1U << 2)
#define HID_TOOL_DPAD_RIGHT    (1U << 3)

/* Convert D-pad GPIO mask to POV code (0..8). */
uint8_t HidTool_PovFromDpadMask(uint8_t u8DpadMask);
/* Convenience helper: convert D-pad mask then write POV in one call. */
void HidTool_SetExternalPovFromDpadMask(uint8_t u8DpadMask);

/* Inject all button states at once.
 * bit0=Button1 ... bit(N-1)=ButtonN, N=HID_JOYSTICK_BUTTON_COUNT.
 *
 * Recommended for periodic GPIO scan (common case):
 * - Build a full button mask every cycle, then call this API once. */
void HidTool_SetExternalButtons(uint16_t u16ButtonsMask);

/* Inject single button state.
 * u8ButtonIndex: 1..HID_JOYSTICK_BUTTON_COUNT, u8Pressed: 0/1.
 *
 * Recommended for event-style updates:
 * - For example, edge ISR / event queue updates one button at a time.
 * - If you already have full button mask each cycle, prefer HidTool_SetExternalButtons(). */
void HidTool_SetExternalButton(uint8_t u8ButtonIndex, uint8_t u8Pressed);

/* Report Byte6/Byte7 reserved extension values. */
void HidTool_SetReservedBytes(uint8_t u8Byte6, uint8_t u8Byte7);

#endif /* __HID_JOYSTICK_API_H__ */
