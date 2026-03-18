#ifndef __BUTTON_SW_DEBOUNCE_H__
#define __BUTTON_SW_DEBOUNCE_H__

#include <stdint.h>

/*
 * Software debounce helper for joystick buttons.
 * - Call ButtonSwDebounce_Timer1msTick() from 1ms timer ISR/callback.
 * - Call ButtonSwDebounce_UpdateHidByProfile() in task context (for example 5~10 ms).
 * - Mapping profile (LEFT/RIGHT/BOTH) and GPIO definition are in hid_joystick_conf.h.
 */
void ButtonSwDebounce_Init(void);
void ButtonSwDebounce_Timer1msTick(void);
uint16_t ButtonSwDebounce_GetMask(void); /* raw signal mask (see HID_JOYSTICK_SIG_BIT_* in hid_joystick_conf.h) */
uint8_t ButtonSwDebounce_GetButtonState(uint8_t u8SignalIndex); /* index: 1..HID_JOYSTICK_GPIO_INPUT_COUNT */
void ButtonSwDebounce_UpdateHidButtons(void);
void ButtonSwDebounce_UpdateHidLeftHalf(void);
void ButtonSwDebounce_UpdateHidRightHalf(void);
void ButtonSwDebounce_UpdateHidBoth(void);
void ButtonSwDebounce_UpdateHidByProfile(void);

#endif /* __BUTTON_SW_DEBOUNCE_H__ */
