/****************************************************************************
 * @file     button_sw_debounce.c
 * @brief    GPIO button debounce helper for HID joystick button mask update
 ****************************************************************************/

#include <string.h>
#include <stdio.h>

#include "NuMicro.h"
#include "hid_joystick_conf.h"
#include "hid_joystick_api.h"
#include "button_sw_debounce.h"

#define BUTTON_SW_PIN_MAX                  15U
#define BUTTON_SW_INPUT_COUNT              HID_JOYSTICK_GPIO_INPUT_COUNT
#define BUTTON_SW_MASK_ALL                 ((uint16_t)((1UL << BUTTON_SW_INPUT_COUNT) - 1UL))

#if (HID_JOYSTICK_BUTTON_DEBOUNCE_PRESS_TICKS == 0U)
#error "HID_JOYSTICK_BUTTON_DEBOUNCE_PRESS_TICKS must be >= 1"
#endif

#if (HID_JOYSTICK_BUTTON_DEBOUNCE_RELEASE_TICKS == 0U)
#error "HID_JOYSTICK_BUTTON_DEBOUNCE_RELEASE_TICKS must be >= 1"
#endif

#if (BUTTON_SW_INPUT_COUNT == 0U) || (BUTTON_SW_INPUT_COUNT > 16U)
#error "HID_JOYSTICK_GPIO_INPUT_COUNT must be 1..16."
#endif

static GPIO_T * const s_apsInputPort[BUTTON_SW_INPUT_COUNT] =
{
    HID_JOYSTICK_GPIO_DPAD_UP_PORT,
    HID_JOYSTICK_GPIO_DPAD_DOWN_PORT,
    HID_JOYSTICK_GPIO_DPAD_LEFT_PORT,
    HID_JOYSTICK_GPIO_DPAD_RIGHT_PORT,
    HID_JOYSTICK_GPIO_L1_PORT,
    HID_JOYSTICK_GPIO_L2_PORT,
    HID_JOYSTICK_GPIO_L3_PORT,
    HID_JOYSTICK_GPIO_R1_PORT,
    HID_JOYSTICK_GPIO_R2_PORT,
    HID_JOYSTICK_GPIO_R3_PORT,
    HID_JOYSTICK_GPIO_X_PORT,
    HID_JOYSTICK_GPIO_A_PORT,
    HID_JOYSTICK_GPIO_B_PORT,
    HID_JOYSTICK_GPIO_Y_PORT,
    HID_JOYSTICK_GPIO_SELECT_PORT,
    HID_JOYSTICK_GPIO_START_PORT
};

static const uint8_t s_au8InputPin[BUTTON_SW_INPUT_COUNT] =
{
    HID_JOYSTICK_GPIO_DPAD_UP_PIN,
    HID_JOYSTICK_GPIO_DPAD_DOWN_PIN,
    HID_JOYSTICK_GPIO_DPAD_LEFT_PIN,
    HID_JOYSTICK_GPIO_DPAD_RIGHT_PIN,
    HID_JOYSTICK_GPIO_L1_PIN,
    HID_JOYSTICK_GPIO_L2_PIN,
    HID_JOYSTICK_GPIO_L3_PIN,
    HID_JOYSTICK_GPIO_R1_PIN,
    HID_JOYSTICK_GPIO_R2_PIN,
    HID_JOYSTICK_GPIO_R3_PIN,
    HID_JOYSTICK_GPIO_X_PIN,
    HID_JOYSTICK_GPIO_A_PIN,
    HID_JOYSTICK_GPIO_B_PIN,
    HID_JOYSTICK_GPIO_Y_PIN,
    HID_JOYSTICK_GPIO_SELECT_PIN,
    HID_JOYSTICK_GPIO_START_PIN
};

static volatile uint16_t s_u16StableMask = 0U;
static uint8_t s_au8TransitionTicks[BUTTON_SW_INPUT_COUNT];

static uint16_t ButtonSwDebounce_GetHidBitByIndex(uint8_t u8ButtonIndex)
{
    if((u8ButtonIndex < 1U) || (u8ButtonIndex > HID_JOYSTICK_BUTTON_COUNT))
    {
        return 0U;
    }
    return (uint16_t)(1U << (u8ButtonIndex - 1U));
}

static uint8_t ButtonSwDebounce_ReadRawPressed(uint8_t u8SignalIndex)
{
    GPIO_T *psPort;
    uint8_t u8Pin;
    uint8_t u8Level;

    if(u8SignalIndex >= BUTTON_SW_INPUT_COUNT)
    {
        return 0U;
    }

    psPort = s_apsInputPort[u8SignalIndex];
    u8Pin = s_au8InputPin[u8SignalIndex];
    if((psPort == 0) || (u8Pin > BUTTON_SW_PIN_MAX))
    {
        return 0U;
    }

    u8Level = ((psPort->PIN & (1UL << u8Pin)) != 0U) ? 1U : 0U;

#if (HID_JOYSTICK_BUTTON_ACTIVE_LEVEL == 0U)
    return (u8Level == 0U) ? 1U : 0U;
#else
    return u8Level;
#endif
}

static uint8_t ButtonSwDebounce_BuildDpadLeft(uint16_t u16RawMask)
{
    uint8_t u8DpadMask = 0U;

    if((u16RawMask & HID_JOYSTICK_MAP_L_DPAD_UP_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_UP;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_L_DPAD_DOWN_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_DOWN;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_L_DPAD_LEFT_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_LEFT;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_L_DPAD_RIGHT_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_RIGHT;
    }

    return u8DpadMask;
}

static uint8_t ButtonSwDebounce_BuildDpadRight(uint16_t u16RawMask)
{
    uint8_t u8DpadMask = 0U;

    if((u16RawMask & HID_JOYSTICK_MAP_R_DPAD_UP_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_UP;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_R_DPAD_DOWN_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_DOWN;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_R_DPAD_LEFT_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_LEFT;
    }
    if((u16RawMask & HID_JOYSTICK_MAP_R_DPAD_RIGHT_SRC_MASK) != 0U)
    {
        u8DpadMask |= HID_TOOL_DPAD_RIGHT;
    }

    return u8DpadMask;
}

static uint16_t ButtonSwDebounce_BuildButtonsLeft(uint16_t u16RawMask)
{
    uint16_t u16OutButtonsMask = 0U;

#define APPEND_LEFT_BTN(src_mask, btn_index) \
    do { \
        if(((src_mask) != 0U) && ((u16RawMask & (src_mask)) != 0U)) \
        { \
            u16OutButtonsMask |= ButtonSwDebounce_GetHidBitByIndex((btn_index)); \
        } \
    } while(0)

    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN1_SRC_MASK, 1U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN2_SRC_MASK, 2U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN3_SRC_MASK, 3U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN4_SRC_MASK, 4U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN5_SRC_MASK, 5U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN6_SRC_MASK, 6U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN7_SRC_MASK, 7U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN8_SRC_MASK, 8U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN9_SRC_MASK, 9U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN10_SRC_MASK, 10U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN11_SRC_MASK, 11U);
    APPEND_LEFT_BTN(HID_JOYSTICK_MAP_L_BTN12_SRC_MASK, 12U);

#undef APPEND_LEFT_BTN
    return u16OutButtonsMask;
}

static uint16_t ButtonSwDebounce_BuildButtonsRight(uint16_t u16RawMask)
{
    uint16_t u16OutButtonsMask = 0U;

#define APPEND_RIGHT_BTN(src_mask, btn_index) \
    do { \
        if(((src_mask) != 0U) && ((u16RawMask & (src_mask)) != 0U)) \
        { \
            u16OutButtonsMask |= ButtonSwDebounce_GetHidBitByIndex((btn_index)); \
        } \
    } while(0)

    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN1_SRC_MASK, 1U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN2_SRC_MASK, 2U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN3_SRC_MASK, 3U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN4_SRC_MASK, 4U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN5_SRC_MASK, 5U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN6_SRC_MASK, 6U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN7_SRC_MASK, 7U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN8_SRC_MASK, 8U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN9_SRC_MASK, 9U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN10_SRC_MASK, 10U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN11_SRC_MASK, 11U);
    APPEND_RIGHT_BTN(HID_JOYSTICK_MAP_R_BTN12_SRC_MASK, 12U);

#undef APPEND_RIGHT_BTN
    return u16OutButtonsMask;
}

static void ButtonSwDebounce_DebugTrace(uint16_t u16RawMask, uint8_t u8DpadMask, uint16_t u16OutButtonsMask)
{
#if (HID_JOYSTICK_DEBUG_BUTTON_PATH != 0U)
    static uint16_t s_u16LastRawMask = 0xFFFFU;
    static uint8_t s_u8LastDpadMask = 0xFFU;
    static uint16_t s_u16LastOutButtonsMask = 0xFFFFU;
    static uint16_t s_u16Heartbeat = 0U;
    uint8_t u8Changed = 0U;

    if((u16RawMask != s_u16LastRawMask) ||
       (u8DpadMask != s_u8LastDpadMask) ||
       (u16OutButtonsMask != s_u16LastOutButtonsMask))
    {
        u8Changed = 1U;
    }

    s_u16Heartbeat++;
    if((u8Changed != 0U) || (s_u16Heartbeat >= 100U)) /* 100 * 10ms = 1s */
    {
        printf("[DBG][BTN] raw=0x%04X dpad=0x%X out=0x%03X PC=%04lX PA=%04lX PB=%04lX\r\n",
               (unsigned int)u16RawMask,
               (unsigned int)u8DpadMask,
               (unsigned int)u16OutButtonsMask,
               (unsigned long)(PC->PIN & 0xFFFFUL),
               (unsigned long)(PA->PIN & 0xFFFFUL),
               (unsigned long)(PB->PIN & 0xFFFFUL));
        s_u16Heartbeat = 0U;
    }

    s_u16LastRawMask = u16RawMask;
    s_u8LastDpadMask = u8DpadMask;
    s_u16LastOutButtonsMask = u16OutButtonsMask;
#else
    (void)u16RawMask;
    (void)u8DpadMask;
    (void)u16OutButtonsMask;
#endif
}

void ButtonSwDebounce_Init(void)
{
    uint32_t u32Index;

    s_u16StableMask = 0U;
    (void)memset(s_au8TransitionTicks, 0, sizeof(s_au8TransitionTicks));

    for(u32Index = 0U; u32Index < BUTTON_SW_INPUT_COUNT; u32Index++)
    {
        if(s_au8InputPin[u32Index] <= BUTTON_SW_PIN_MAX)
        {
            GPIO_SetMode(s_apsInputPort[u32Index], (1UL << s_au8InputPin[u32Index]), GPIO_MODE_INPUT);
        }
    }
}

void ButtonSwDebounce_Timer1msTick(void)
{
    uint32_t u32Index;
    uint16_t u16Bit;
    uint8_t u8RawPressed;
    uint8_t u8StablePressed;
    uint8_t u8RequiredTicks;

    for(u32Index = 0U; u32Index < BUTTON_SW_INPUT_COUNT; u32Index++)
    {
        u16Bit = (uint16_t)(1U << u32Index);
        u8RawPressed = ButtonSwDebounce_ReadRawPressed((uint8_t)u32Index);
        u8StablePressed = ((s_u16StableMask & u16Bit) != 0U) ? 1U : 0U;

        if(u8RawPressed == u8StablePressed)
        {
            s_au8TransitionTicks[u32Index] = 0U;
            continue;
        }

        if(s_au8TransitionTicks[u32Index] < 0xFFU)
        {
            s_au8TransitionTicks[u32Index]++;
        }

        u8RequiredTicks = (u8RawPressed != 0U) ?
                          HID_JOYSTICK_BUTTON_DEBOUNCE_PRESS_TICKS :
                          HID_JOYSTICK_BUTTON_DEBOUNCE_RELEASE_TICKS;

        if(s_au8TransitionTicks[u32Index] >= u8RequiredTicks)
        {
            if(u8RawPressed != 0U)
            {
                s_u16StableMask = (uint16_t)(s_u16StableMask | u16Bit);
            }
            else
            {
                s_u16StableMask = (uint16_t)(s_u16StableMask & (~u16Bit));
            }
            s_au8TransitionTicks[u32Index] = 0U;
        }
    }
}

uint16_t ButtonSwDebounce_GetMask(void)
{
    return (uint16_t)(s_u16StableMask & BUTTON_SW_MASK_ALL);
}

uint8_t ButtonSwDebounce_GetButtonState(uint8_t u8ButtonIndex)
{
    uint16_t u16Bit;

    if((u8ButtonIndex < 1U) || (u8ButtonIndex > BUTTON_SW_INPUT_COUNT))
    {
        return 0U;
    }

    u16Bit = (uint16_t)(1U << (u8ButtonIndex - 1U));
    return ((ButtonSwDebounce_GetMask() & u16Bit) != 0U) ? 1U : 0U;
}

void ButtonSwDebounce_UpdateHidButtons(void)
{
    ButtonSwDebounce_UpdateHidByProfile();
}

void ButtonSwDebounce_UpdateHidLeftHalf(void)
{
    uint16_t u16RawMask = ButtonSwDebounce_GetMask();
    uint8_t u8DpadMask = ButtonSwDebounce_BuildDpadLeft(u16RawMask);
    uint16_t u16OutButtonsMask = ButtonSwDebounce_BuildButtonsLeft(u16RawMask);

    HidTool_SetExternalPovFromDpadMask(u8DpadMask);
    HidTool_SetExternalButtons(u16OutButtonsMask);
    ButtonSwDebounce_DebugTrace(u16RawMask, u8DpadMask, u16OutButtonsMask);
}

void ButtonSwDebounce_UpdateHidRightHalf(void)
{
    uint16_t u16RawMask = ButtonSwDebounce_GetMask();
    uint8_t u8DpadMask = ButtonSwDebounce_BuildDpadRight(u16RawMask);
    uint16_t u16OutButtonsMask = ButtonSwDebounce_BuildButtonsRight(u16RawMask);

    HidTool_SetExternalPovFromDpadMask(u8DpadMask);
    HidTool_SetExternalButtons(u16OutButtonsMask);
    ButtonSwDebounce_DebugTrace(u16RawMask, u8DpadMask, u16OutButtonsMask);
}

void ButtonSwDebounce_UpdateHidBoth(void)
{
    uint16_t u16RawMask = ButtonSwDebounce_GetMask();
    uint8_t u8DpadMask = 0U;
    uint16_t u16OutButtonsMask = 0U;

    u8DpadMask = (uint8_t)(ButtonSwDebounce_BuildDpadLeft(u16RawMask) |
                           ButtonSwDebounce_BuildDpadRight(u16RawMask));
    u16OutButtonsMask = (uint16_t)(ButtonSwDebounce_BuildButtonsLeft(u16RawMask) |
                                   ButtonSwDebounce_BuildButtonsRight(u16RawMask));

    HidTool_SetExternalPovFromDpadMask(u8DpadMask);
    HidTool_SetExternalButtons(u16OutButtonsMask);
    ButtonSwDebounce_DebugTrace(u16RawMask, u8DpadMask, u16OutButtonsMask);
}

void ButtonSwDebounce_UpdateHidByProfile(void)
{
#if (HID_JOYSTICK_ACTIVE_MAP_PROFILE == HID_JOYSTICK_MAP_PROFILE_LEFT)
    ButtonSwDebounce_UpdateHidLeftHalf();
#elif (HID_JOYSTICK_ACTIVE_MAP_PROFILE == HID_JOYSTICK_MAP_PROFILE_RIGHT)
    ButtonSwDebounce_UpdateHidRightHalf();
#elif (HID_JOYSTICK_ACTIVE_MAP_PROFILE == HID_JOYSTICK_MAP_PROFILE_BOTH)
    ButtonSwDebounce_UpdateHidBoth();
#else
#error "HID_JOYSTICK_ACTIVE_MAP_PROFILE must be LEFT/RIGHT/BOTH."
#endif
}
