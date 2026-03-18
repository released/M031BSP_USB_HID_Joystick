/******************************************************************************
 * @file     hid_joystick_api.c
 * @brief    HID joystick API layer
 *
 * Report layout (matches descriptors.c):
 *   Byte0 : X   (uint8, 0..255, center ~0x80)
 *   Byte1 : Y   (uint8, 0..255, center ~0x80)
 *   Byte2 : Z   (uint8, 0..255, center ~0x80)
 *   Byte3 : Rz  (uint8, 0..255, center ~0x80)
 *   Byte4 : bit0..3 POV, bit4..7 Button1..4
 *   Byte5 : bit0..7 Button5..12 (or fewer if HID_JOYSTICK_BUTTON_COUNT < 12)
 *   Byte6 : Reserved/Extension0 (vendor-defined)
 *   Byte7 : Reserved/Extension1 (vendor-defined)
 *****************************************************************************/

#include <stdio.h>

#include "NuMicro.h"
#include "hid_transfer.h"
#include "hid_joystick_api.h"

#if (HID_JOYSTICK_AXIS_COUNT != 4U)
#error "Current hid_joystick_api.c implementation expects 4 axes (X/Y/Z/Rz)."
#endif

/*
 * Current base layout supports at most 12 buttons:
 *   Byte4 high nibble = Button1..4, Byte5 = Button5..12.
 * Tuning entry point: hid_joystick_conf.h
 */
#define HID_TOOL_BUTTON_COUNT               HID_JOYSTICK_BUTTON_COUNT
#define HID_TOOL_BUTTON_LOW_BITS            4U
#define HID_TOOL_BUTTON_HIGH_BITS           8U
#define HID_TOOL_BUTTON_LAYOUT_CAPACITY     (HID_TOOL_BUTTON_LOW_BITS + HID_TOOL_BUTTON_HIGH_BITS)

#if (HID_TOOL_BUTTON_COUNT == 0U) || (HID_TOOL_BUTTON_COUNT > HID_TOOL_BUTTON_LAYOUT_CAPACITY)
#error "HID_TOOL_BUTTON_COUNT must be 1..12 for current report layout."
#endif

#define HID_TOOL_BUTTON_LOW_COUNT           ((HID_TOOL_BUTTON_COUNT <= HID_TOOL_BUTTON_LOW_BITS) ? \
                                             HID_TOOL_BUTTON_COUNT : HID_TOOL_BUTTON_LOW_BITS)
#define HID_TOOL_BUTTON_HIGH_COUNT          ((HID_TOOL_BUTTON_COUNT > HID_TOOL_BUTTON_LOW_BITS) ? \
                                             (HID_TOOL_BUTTON_COUNT - HID_TOOL_BUTTON_LOW_BITS) : 0U)
#define HID_TOOL_BUTTON_MASK                ((uint16_t)((1UL << HID_TOOL_BUTTON_COUNT) - 1UL))
#define HID_TOOL_BUTTON_LOW_MASK            ((uint16_t)((1UL << HID_TOOL_BUTTON_LOW_COUNT) - 1UL))
#define HID_TOOL_BUTTON_HIGH_MASK           ((uint16_t)((1UL << HID_TOOL_BUTTON_HIGH_COUNT) - 1UL))
#define HID_TOOL_ADC_BUF_MAX_SAMPLES        HID_JOYSTICK_ADC_BUF_MAX_SAMPLES
#define HID_TOOL_ADC_DEFAULT_SAMPLES        HID_JOYSTICK_ADC_DEFAULT_SAMPLES

#define HID_TOOL_ADC_DEFAULT_MIN            HID_JOYSTICK_ADC_DEFAULT_MIN
#define HID_TOOL_ADC_DEFAULT_CENTER         HID_JOYSTICK_ADC_DEFAULT_CENTER
#define HID_TOOL_ADC_DEFAULT_MAX            HID_JOYSTICK_ADC_DEFAULT_MAX
#define HID_TOOL_AXIS_FILTER_ENABLE         HID_JOYSTICK_AXIS_FILTER_ENABLE
#define HID_TOOL_AXIS_DEADZONE_ADC          HID_JOYSTICK_AXIS_DEADZONE_ADC
#define HID_TOOL_AXIS_HYSTERESIS_ENABLE     HID_JOYSTICK_AXIS_HYSTERESIS_ENABLE
#define HID_TOOL_AXIS_HYSTERESIS_ADC        HID_JOYSTICK_AXIS_HYSTERESIS_ADC

#define HID_TOOL_POV_CENTER                 0x08U   /* Hat null-state(center): value > 7 */

#define HID_REPORT_IDX_X                    0U
#define HID_REPORT_IDX_Y                    1U
#define HID_REPORT_IDX_Z                    2U
#define HID_REPORT_IDX_RZ                   3U
#define HID_REPORT_IDX_POV_BTN14            4U
#define HID_REPORT_IDX_BTN512               5U
#define HID_REPORT_IDX_RSVD0                6U
#define HID_REPORT_IDX_RSVD1                7U

/* Defaults aligned to observed commercial gamepad trailing bytes. */
#define HID_TOOL_RSVD0_DEFAULT              HID_JOYSTICK_RSVD0_DEFAULT
#define HID_TOOL_RSVD1_DEFAULT              HID_JOYSTICK_RSVD1_DEFAULT

typedef struct
{
    uint16_t u16Min;
    uint16_t u16Center;
    uint16_t u16Max;
} S_HID_TOOL_ADC_CAL;

typedef struct
{
    uint16_t u16ButtonsMask;
    uint8_t u8Pov;
    uint16_t u16Frame;
} S_HID_TOOL_RUN_STATE;

static S_HID_TOOL_ADC_CAL s_asAxisCal[HID_TOOL_AXIS_COUNT];
static uint16_t s_au16AdcBuf[HID_TOOL_AXIS_COUNT][HID_TOOL_ADC_BUF_MAX_SAMPLES];
static uint16_t s_u16AdcBufCount = 0U;
static uint16_t s_u16AdcBufIndex = 0U;

static S_HID_TOOL_RUN_STATE s_sRunState =
{
    0x0001U,
    HID_TOOL_POV_CENTER,
    0U
};

/* External input injection state for real ADC/buttons */
static uint8_t s_u8UseExternalInput = 0U;
static uint16_t s_au16ExternalAdc[HID_TOOL_AXIS_COUNT] =
{
    HID_TOOL_ADC_DEFAULT_CENTER,
    HID_TOOL_ADC_DEFAULT_CENTER,
    HID_TOOL_ADC_DEFAULT_CENTER,
    HID_TOOL_ADC_DEFAULT_CENTER
};
static uint8_t s_u8ExternalPov = HID_TOOL_POV_CENTER;
static uint16_t s_u16ExternalButtons = 0U;
static uint8_t s_u8Reserved0 = HID_TOOL_RSVD0_DEFAULT;
static uint8_t s_u8Reserved1 = HID_TOOL_RSVD1_DEFAULT;

static volatile uint8_t s_au8LastReport[HID_JOYSTICK_REPORT_SIZE] = {0};
static volatile uint16_t s_au16LastAdc[HID_TOOL_AXIS_COUNT] = {0};
static volatile uint8_t s_au8LastAxisHid[HID_TOOL_AXIS_COUNT] = {0x80U, 0x80U, 0x80U, 0x80U};
static volatile uint32_t s_u32ReportTxSeq = 0U;
static uint32_t s_u32LastLogSeq = 0U;
static uint8_t s_au8AxisInDeadzone[HID_TOOL_AXIS_COUNT] = {1U, 1U, 1U, 1U};

static uint32_t HidTool_EnterCritical(void)
{
    uint32_t u32Primask = __get_PRIMASK();
    __disable_irq();
    return u32Primask;
}

static void HidTool_ExitCritical(uint32_t u32Primask)
{
    if(u32Primask == 0U)
    {
        __enable_irq();
    }
}

static uint8_t HidTool_ClampU8(int32_t i32Value)
{
    if(i32Value > 255)
    {
        return 255U;
    }
    if(i32Value < 0)
    {
        return 0U;
    }
    return (uint8_t)i32Value;
}

static uint8_t HidTool_ConvertAdcToHid(uint16_t u16Adc, const S_HID_TOOL_ADC_CAL *psCal)
{
    int32_t i32Delta;
    int32_t i32Range;
    int32_t i32Value;

    if((psCal == 0) || (psCal->u16Min >= psCal->u16Center) || (psCal->u16Center >= psCal->u16Max))
    {
        return 0x80U;
    }

    if(u16Adc >= psCal->u16Center)
    {
        i32Delta = (int32_t)u16Adc - (int32_t)psCal->u16Center;
        i32Range = (int32_t)psCal->u16Max - (int32_t)psCal->u16Center;
        if(i32Range <= 0)
        {
            return 0x80U;
        }
        i32Value = 128 + ((i32Delta * 127) / i32Range);
    }
    else
    {
        i32Delta = (int32_t)psCal->u16Center - (int32_t)u16Adc;
        i32Range = (int32_t)psCal->u16Center - (int32_t)psCal->u16Min;
        if(i32Range <= 0)
        {
            return 0x80U;
        }
        i32Value = 128 - ((i32Delta * 128) / i32Range);
    }

    return HidTool_ClampU8(i32Value);
}

static uint16_t HidTool_FilterAxisAdc(uint16_t u16Adc, uint8_t u8Axis, const S_HID_TOOL_ADC_CAL *psCal)
{
#if (HID_TOOL_AXIS_FILTER_ENABLE != 0U)
    int32_t i32Center;
    int32_t i32Delta;
    int32_t i32AbsDelta;
    int32_t i32Deadzone;
    int32_t i32EnterThreshold;
    int32_t i32ExitThreshold;
    int32_t i32Range;
    int32_t i32Excess;
    int32_t i32Scaled;
    uint16_t u16Filtered;

    if((psCal == 0) || (u8Axis >= HID_TOOL_AXIS_COUNT))
    {
        return u16Adc;
    }
    if((psCal->u16Min >= psCal->u16Center) || (psCal->u16Center >= psCal->u16Max))
    {
        return HID_TOOL_ADC_DEFAULT_CENTER;
    }

    if(u16Adc < psCal->u16Min)
    {
        u16Adc = psCal->u16Min;
    }
    if(u16Adc > psCal->u16Max)
    {
        u16Adc = psCal->u16Max;
    }

    i32Center = (int32_t)psCal->u16Center;
    i32Delta = (int32_t)u16Adc - i32Center;
    i32AbsDelta = (i32Delta >= 0) ? i32Delta : -i32Delta;
    i32Deadzone = (int32_t)HID_TOOL_AXIS_DEADZONE_ADC;

    if(i32Deadzone <= 0)
    {
        return u16Adc;
    }

#if (HID_TOOL_AXIS_HYSTERESIS_ENABLE != 0U)
    i32EnterThreshold = i32Deadzone + (int32_t)HID_TOOL_AXIS_HYSTERESIS_ADC;
    i32ExitThreshold = i32Deadzone - (int32_t)HID_TOOL_AXIS_HYSTERESIS_ADC;
    if(i32ExitThreshold < 0)
    {
        i32ExitThreshold = 0;
    }

    if(s_au8AxisInDeadzone[u8Axis] != 0U)
    {
        if(i32AbsDelta <= i32EnterThreshold)
        {
            return (uint16_t)i32Center;
        }
        s_au8AxisInDeadzone[u8Axis] = 0U;
    }
    else
    {
        if(i32AbsDelta <= i32ExitThreshold)
        {
            s_au8AxisInDeadzone[u8Axis] = 1U;
            return (uint16_t)i32Center;
        }
    }
#else
    if(i32AbsDelta <= i32Deadzone)
    {
        return (uint16_t)i32Center;
    }
#endif

    if(i32Delta > 0)
    {
        i32Range = (int32_t)psCal->u16Max - i32Center;
    }
    else
    {
        i32Range = i32Center - (int32_t)psCal->u16Min;
    }

    if(i32Range <= i32Deadzone)
    {
        return (uint16_t)i32Center;
    }

    i32Excess = i32AbsDelta - i32Deadzone;
    if(i32Excess < 0)
    {
        i32Excess = 0;
    }

    i32Scaled = (i32Excess * i32Range) / (i32Range - i32Deadzone);
    if(i32Scaled > i32Range)
    {
        i32Scaled = i32Range;
    }

    if(i32Delta > 0)
    {
        u16Filtered = (uint16_t)(i32Center + i32Scaled);
    }
    else
    {
        u16Filtered = (uint16_t)(i32Center - i32Scaled);
    }

    return u16Filtered;
#else
    (void)u8Axis;
    (void)psCal;
    return u16Adc;
#endif
}

static void HidTool_GetNextAdcSample(uint16_t *pu16X, uint16_t *pu16Y, uint16_t *pu16Z, uint16_t *pu16Rz)
{
    uint16_t u16Idx;

    if((pu16X == 0) || (pu16Y == 0) || (pu16Z == 0) || (pu16Rz == 0))
    {
        return;
    }

    if(s_u8UseExternalInput)
    {
        *pu16X = s_au16ExternalAdc[HID_TOOL_AXIS_X];
        *pu16Y = s_au16ExternalAdc[HID_TOOL_AXIS_Y];
        *pu16Z = s_au16ExternalAdc[HID_TOOL_AXIS_Z];
        *pu16Rz = s_au16ExternalAdc[HID_TOOL_AXIS_RZ];
        return;
    }

    if(s_u16AdcBufCount == 0U)
    {
        *pu16X = HID_TOOL_ADC_DEFAULT_CENTER;
        *pu16Y = HID_TOOL_ADC_DEFAULT_CENTER;
        *pu16Z = HID_TOOL_ADC_DEFAULT_CENTER;
        *pu16Rz = HID_TOOL_ADC_DEFAULT_CENTER;
        return;
    }

    u16Idx = s_u16AdcBufIndex;
    if(u16Idx >= s_u16AdcBufCount)
    {
        u16Idx = 0U;
    }

    *pu16X = s_au16AdcBuf[HID_TOOL_AXIS_X][u16Idx];
    *pu16Y = s_au16AdcBuf[HID_TOOL_AXIS_Y][u16Idx];
    *pu16Z = s_au16AdcBuf[HID_TOOL_AXIS_Z][u16Idx];
    *pu16Rz = s_au16AdcBuf[HID_TOOL_AXIS_RZ][u16Idx];

    u16Idx++;
    if(u16Idx >= s_u16AdcBufCount)
    {
        u16Idx = 0U;
    }
    s_u16AdcBufIndex = u16Idx;
}

static void HidTool_UpdateAutoControls(void)
{
    uint16_t u16MaxButtonMask = (uint16_t)(1U << (HID_TOOL_BUTTON_COUNT - 1U));

    s_sRunState.u16Frame++;

    if((s_sRunState.u16Frame % 40U) == 0U)
    {
        s_sRunState.u16ButtonsMask <<= 1;
        if((s_sRunState.u16ButtonsMask == 0U) || (s_sRunState.u16ButtonsMask > u16MaxButtonMask))
        {
            s_sRunState.u16ButtonsMask = 0x0001U;
        }
    }

    /* Keep POV at center in default pattern playback. */
    s_sRunState.u8Pov = HID_TOOL_POV_CENTER;
}

static void HidTool_BuildReportFromSource(uint8_t *pu8Report)
{
    uint16_t u16X;
    uint16_t u16Y;
    uint16_t u16Z;
    uint16_t u16Rz;
    uint8_t u8X;
    uint8_t u8Y;
    uint8_t u8Z;
    uint8_t u8Rz;
    uint16_t u16Buttons;
    uint8_t u8Pov;

    if(pu8Report == 0)
    {
        return;
    }

    HidTool_GetNextAdcSample(&u16X, &u16Y, &u16Z, &u16Rz);

    u16X = HidTool_FilterAxisAdc(u16X, HID_TOOL_AXIS_X, &s_asAxisCal[HID_TOOL_AXIS_X]);
    u16Y = HidTool_FilterAxisAdc(u16Y, HID_TOOL_AXIS_Y, &s_asAxisCal[HID_TOOL_AXIS_Y]);
    u16Z = HidTool_FilterAxisAdc(u16Z, HID_TOOL_AXIS_Z, &s_asAxisCal[HID_TOOL_AXIS_Z]);
    u16Rz = HidTool_FilterAxisAdc(u16Rz, HID_TOOL_AXIS_RZ, &s_asAxisCal[HID_TOOL_AXIS_RZ]);

    u8X = HidTool_ConvertAdcToHid(u16X, &s_asAxisCal[HID_TOOL_AXIS_X]);
    u8Y = HidTool_ConvertAdcToHid(u16Y, &s_asAxisCal[HID_TOOL_AXIS_Y]);
    u8Z = HidTool_ConvertAdcToHid(u16Z, &s_asAxisCal[HID_TOOL_AXIS_Z]);
    u8Rz = HidTool_ConvertAdcToHid(u16Rz, &s_asAxisCal[HID_TOOL_AXIS_RZ]);

    if(s_u8UseExternalInput)
    {
        u16Buttons = (uint16_t)(s_u16ExternalButtons & HID_TOOL_BUTTON_MASK);
        u8Pov = (uint8_t)(s_u8ExternalPov & 0x0FU);
    }
    else
    {
        u16Buttons = (uint16_t)(s_sRunState.u16ButtonsMask & HID_TOOL_BUTTON_MASK);
        u8Pov = (uint8_t)(s_sRunState.u8Pov & 0x0FU);
    }

    pu8Report[HID_REPORT_IDX_X] = u8X;
    pu8Report[HID_REPORT_IDX_Y] = u8Y;
    pu8Report[HID_REPORT_IDX_Z] = u8Z;
    pu8Report[HID_REPORT_IDX_RZ] = u8Rz;
    pu8Report[HID_REPORT_IDX_POV_BTN14] = (uint8_t)(u8Pov |
                                                     (((u16Buttons & HID_TOOL_BUTTON_LOW_MASK) << 4) & 0xF0U));
    pu8Report[HID_REPORT_IDX_BTN512] = (uint8_t)((u16Buttons >> 4) & HID_TOOL_BUTTON_HIGH_MASK);
    pu8Report[HID_REPORT_IDX_RSVD0] = s_u8Reserved0;
    pu8Report[HID_REPORT_IDX_RSVD1] = s_u8Reserved1;

    s_au16LastAdc[HID_TOOL_AXIS_X] = u16X;
    s_au16LastAdc[HID_TOOL_AXIS_Y] = u16Y;
    s_au16LastAdc[HID_TOOL_AXIS_Z] = u16Z;
    s_au16LastAdc[HID_TOOL_AXIS_RZ] = u16Rz;
    s_au8LastAxisHid[HID_TOOL_AXIS_X] = u8X;
    s_au8LastAxisHid[HID_TOOL_AXIS_Y] = u8Y;
    s_au8LastAxisHid[HID_TOOL_AXIS_Z] = u8Z;
    s_au8LastAxisHid[HID_TOOL_AXIS_RZ] = u8Rz;

    if(!s_u8UseExternalInput)
    {
        HidTool_UpdateAutoControls();
    }
}

uint16_t HidTool_GetAdcBufferCapacity(void)
{
    return HID_TOOL_ADC_BUF_MAX_SAMPLES;
}

uint16_t HidTool_GetAdcBufferCount(void)
{
    return s_u16AdcBufCount;
}

void HidTool_ResetAdcCalibration(void)
{
    uint32_t u32Axis;

    for(u32Axis = 0U; u32Axis < HID_TOOL_AXIS_COUNT; u32Axis++)
    {
        s_asAxisCal[u32Axis].u16Min = HID_TOOL_ADC_DEFAULT_MIN;
        s_asAxisCal[u32Axis].u16Center = HID_TOOL_ADC_DEFAULT_CENTER;
        s_asAxisCal[u32Axis].u16Max = HID_TOOL_ADC_DEFAULT_MAX;
    }
}

void HidTool_SetAdcCalibration(uint8_t u8Axis, uint16_t u16Min, uint16_t u16Center, uint16_t u16Max)
{
    uint32_t u32Primask;

    if(u8Axis >= HID_TOOL_AXIS_COUNT)
    {
        return;
    }

    if((u16Min >= u16Center) || (u16Center >= u16Max))
    {
        return;
    }

    u32Primask = HidTool_EnterCritical();
    s_asAxisCal[u8Axis].u16Min = u16Min;
    s_asAxisCal[u8Axis].u16Center = u16Center;
    s_asAxisCal[u8Axis].u16Max = u16Max;
    HidTool_ExitCritical(u32Primask);
}

void HidTool_LoadDefaultAdcPattern(void)
{
    uint32_t u32Primask;
    uint16_t u16Idx;
    uint16_t u16PhaseX;
    uint16_t u16PhaseY;
    uint16_t u16Half = (HID_TOOL_ADC_DEFAULT_SAMPLES / 2U);

    if(u16Half == 0U)
    {
        return;
    }

    u32Primask = HidTool_EnterCritical();
    for(u16Idx = 0U; u16Idx < HID_TOOL_ADC_DEFAULT_SAMPLES; u16Idx++)
    {
        u16PhaseX = u16Idx;
        u16PhaseY = (uint16_t)((u16Idx + (u16Half / 2U)) % HID_TOOL_ADC_DEFAULT_SAMPLES);

        if(u16PhaseX < u16Half)
        {
            s_au16AdcBuf[HID_TOOL_AXIS_X][u16Idx] = (uint16_t)(512U + ((uint32_t)u16PhaseX * 3072U) / u16Half);
        }
        else
        {
            s_au16AdcBuf[HID_TOOL_AXIS_X][u16Idx] = (uint16_t)(3584U - ((uint32_t)(u16PhaseX - u16Half) * 3072U) / u16Half);
        }

        if(u16PhaseY < u16Half)
        {
            s_au16AdcBuf[HID_TOOL_AXIS_Y][u16Idx] = (uint16_t)(3584U - ((uint32_t)u16PhaseY * 3072U) / u16Half);
        }
        else
        {
            s_au16AdcBuf[HID_TOOL_AXIS_Y][u16Idx] = (uint16_t)(512U + ((uint32_t)(u16PhaseY - u16Half) * 3072U) / u16Half);
        }

        if(u16Idx < u16Half)
        {
            s_au16AdcBuf[HID_TOOL_AXIS_Z][u16Idx] = (uint16_t)((uint32_t)u16Idx * 4095U / u16Half);
            s_au16AdcBuf[HID_TOOL_AXIS_RZ][u16Idx] = (uint16_t)(4095U - ((uint32_t)u16Idx * 4095U / u16Half));
        }
        else
        {
            s_au16AdcBuf[HID_TOOL_AXIS_Z][u16Idx] =
                (uint16_t)(4095U - ((uint32_t)(u16Idx - u16Half) * 4095U / u16Half));
            s_au16AdcBuf[HID_TOOL_AXIS_RZ][u16Idx] = (uint16_t)(((uint32_t)(u16Idx - u16Half) * 4095U) / u16Half);
        }
    }

    s_u16AdcBufCount = HID_TOOL_ADC_DEFAULT_SAMPLES;
    s_u16AdcBufIndex = 0U;
    HidTool_ExitCritical(u32Primask);
}

uint16_t HidTool_LoadAdcBufferEx(const uint16_t *pu16X,
                                 const uint16_t *pu16Y,
                                 const uint16_t *pu16Z,
                                 const uint16_t *pu16Rz,
                                 uint16_t u16Count)
{
    uint16_t u16LoadCount;
    uint16_t u16Idx;
    uint32_t u32Primask;

    if((pu16X == 0) || (pu16Y == 0) || (pu16Z == 0) || (pu16Rz == 0) || (u16Count == 0U))
    {
        return 0U;
    }

    u16LoadCount = u16Count;
    if(u16LoadCount > HID_TOOL_ADC_BUF_MAX_SAMPLES)
    {
        u16LoadCount = HID_TOOL_ADC_BUF_MAX_SAMPLES;
    }

    u32Primask = HidTool_EnterCritical();
    for(u16Idx = 0U; u16Idx < u16LoadCount; u16Idx++)
    {
        s_au16AdcBuf[HID_TOOL_AXIS_X][u16Idx] = pu16X[u16Idx];
        s_au16AdcBuf[HID_TOOL_AXIS_Y][u16Idx] = pu16Y[u16Idx];
        s_au16AdcBuf[HID_TOOL_AXIS_Z][u16Idx] = pu16Z[u16Idx];
        s_au16AdcBuf[HID_TOOL_AXIS_RZ][u16Idx] = pu16Rz[u16Idx];
    }
    s_u16AdcBufCount = u16LoadCount;
    s_u16AdcBufIndex = 0U;
    HidTool_ExitCritical(u32Primask);

    return u16LoadCount;
}

uint16_t HidTool_LoadAdcBuffer(const uint16_t *pu16X,
                               const uint16_t *pu16Y,
                               const uint16_t *pu16Z,
                               uint16_t u16Count)
{
    return HidTool_LoadAdcBufferEx(pu16X, pu16Y, pu16Z, pu16Z, u16Count);
}

void HidTool_SetAdcBufferSampleEx(uint16_t u16Index,
                                  uint16_t u16X,
                                  uint16_t u16Y,
                                  uint16_t u16Z,
                                  uint16_t u16Rz)
{
    uint32_t u32Primask;

    if(u16Index >= HID_TOOL_ADC_BUF_MAX_SAMPLES)
    {
        return;
    }

    u32Primask = HidTool_EnterCritical();
    s_au16AdcBuf[HID_TOOL_AXIS_X][u16Index] = u16X;
    s_au16AdcBuf[HID_TOOL_AXIS_Y][u16Index] = u16Y;
    s_au16AdcBuf[HID_TOOL_AXIS_Z][u16Index] = u16Z;
    s_au16AdcBuf[HID_TOOL_AXIS_RZ][u16Index] = u16Rz;
    if(s_u16AdcBufCount <= u16Index)
    {
        s_u16AdcBufCount = (uint16_t)(u16Index + 1U);
    }
    HidTool_ExitCritical(u32Primask);
}

void HidTool_SetAdcBufferSample(uint16_t u16Index, uint16_t u16X, uint16_t u16Y, uint16_t u16Z)
{
    HidTool_SetAdcBufferSampleEx(u16Index, u16X, u16Y, u16Z, u16Z);
}

void HidTool_SetAdcBufferIndex(uint16_t u16Index)
{
    uint32_t u32Primask;

    u32Primask = HidTool_EnterCritical();
    if(s_u16AdcBufCount == 0U)
    {
        s_u16AdcBufIndex = 0U;
    }
    else
    {
        s_u16AdcBufIndex = (uint16_t)(u16Index % s_u16AdcBufCount);
    }
    HidTool_ExitCritical(u32Primask);
}

void HidTool_EnableExternalInput(uint8_t u8Enable)
{
    uint32_t u32Primask = HidTool_EnterCritical();
    s_u8UseExternalInput = (u8Enable != 0U) ? 1U : 0U;
    HidTool_ExitCritical(u32Primask);
}

void HidTool_SetExternalAdc(uint16_t u16X, uint16_t u16Y, uint16_t u16Z, uint16_t u16Rz)
{
    uint32_t u32Primask = HidTool_EnterCritical();
    s_au16ExternalAdc[HID_TOOL_AXIS_X] = u16X;
    s_au16ExternalAdc[HID_TOOL_AXIS_Y] = u16Y;
    s_au16ExternalAdc[HID_TOOL_AXIS_Z] = u16Z;
    s_au16ExternalAdc[HID_TOOL_AXIS_RZ] = u16Rz;
    HidTool_ExitCritical(u32Primask);
}

void HidTool_SetExternalPov(uint8_t u8Pov)
{
    uint32_t u32Primask = HidTool_EnterCritical();
    s_u8ExternalPov = (uint8_t)(u8Pov & 0x0FU);
    HidTool_ExitCritical(u32Primask);
}

uint8_t HidTool_PovFromDpadMask(uint8_t u8DpadMask)
{
    uint8_t u8Up;
    uint8_t u8Down;
    uint8_t u8Left;
    uint8_t u8Right;

    u8Up = (uint8_t)((u8DpadMask & HID_TOOL_DPAD_UP) ? 1U : 0U);
    u8Down = (uint8_t)((u8DpadMask & HID_TOOL_DPAD_DOWN) ? 1U : 0U);
    u8Left = (uint8_t)((u8DpadMask & HID_TOOL_DPAD_LEFT) ? 1U : 0U);
    u8Right = (uint8_t)((u8DpadMask & HID_TOOL_DPAD_RIGHT) ? 1U : 0U);

    /* Opposite directions cancel each other to avoid invalid states. */
    if((u8Up != 0U) && (u8Down != 0U))
    {
        u8Up = 0U;
        u8Down = 0U;
    }
    if((u8Left != 0U) && (u8Right != 0U))
    {
        u8Left = 0U;
        u8Right = 0U;
    }

    if((u8Up != 0U) && (u8Right != 0U))
    {
        return 1U;  /* Up-Right */
    }
    if((u8Down != 0U) && (u8Right != 0U))
    {
        return 3U;  /* Down-Right */
    }
    if((u8Down != 0U) && (u8Left != 0U))
    {
        return 5U;  /* Down-Left */
    }
    if((u8Up != 0U) && (u8Left != 0U))
    {
        return 7U;  /* Up-Left */
    }
    if(u8Up != 0U)
    {
        return 0U;  /* Up */
    }
    if(u8Right != 0U)
    {
        return 2U;  /* Right */
    }
    if(u8Down != 0U)
    {
        return 4U;  /* Down */
    }
    if(u8Left != 0U)
    {
        return 6U;  /* Left */
    }

    return HID_TOOL_POV_CENTER;    /* Center */
}

void HidTool_SetExternalPovFromDpadMask(uint8_t u8DpadMask)
{
    HidTool_SetExternalPov(HidTool_PovFromDpadMask((uint8_t)(u8DpadMask & 0x0FU)));
}

void HidTool_SetExternalButtons(uint16_t u16ButtonsMask)
{
    uint32_t u32Primask = HidTool_EnterCritical();
    s_u16ExternalButtons = (uint16_t)(u16ButtonsMask & HID_TOOL_BUTTON_MASK);
    HidTool_ExitCritical(u32Primask);
}

void HidTool_SetExternalButton(uint8_t u8ButtonIndex, uint8_t u8Pressed)
{
    uint16_t u16Mask;
    uint32_t u32Primask;

    if((u8ButtonIndex < 1U) || (u8ButtonIndex > HID_TOOL_BUTTON_COUNT))
    {
        return;
    }

    u16Mask = (uint16_t)(1U << (u8ButtonIndex - 1U));

    u32Primask = HidTool_EnterCritical();
    if(u8Pressed)
    {
        s_u16ExternalButtons = (uint16_t)((s_u16ExternalButtons | u16Mask) & HID_TOOL_BUTTON_MASK);
    }
    else
    {
        s_u16ExternalButtons = (uint16_t)(s_u16ExternalButtons & (~u16Mask));
    }
    HidTool_ExitCritical(u32Primask);
}

void HidTool_SetReservedBytes(uint8_t u8Byte6, uint8_t u8Byte7)
{
    uint32_t u32Primask = HidTool_EnterCritical();
    s_u8Reserved0 = u8Byte6;
    s_u8Reserved1 = u8Byte7;
    HidTool_ExitCritical(u32Primask);
}

void HidTool_ResetState(void)
{
    /*
     * Keep external-input mode across USB bus reset path.
     * Otherwise reset handler will fall back to internal demo ADC pattern,
     * and host tool UI appears to "jump" even with no physical input.
     */
    uint8_t u8KeepExternalInput = s_u8UseExternalInput;

    HidTool_ResetAdcCalibration();
    HidTool_LoadDefaultAdcPattern();

    s_u8UseExternalInput = u8KeepExternalInput;
    s_au16ExternalAdc[HID_TOOL_AXIS_X] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16ExternalAdc[HID_TOOL_AXIS_Y] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16ExternalAdc[HID_TOOL_AXIS_Z] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16ExternalAdc[HID_TOOL_AXIS_RZ] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_u8ExternalPov = HID_TOOL_POV_CENTER;
    s_u16ExternalButtons = 0U;
    s_u8Reserved0 = HID_TOOL_RSVD0_DEFAULT;
    s_u8Reserved1 = HID_TOOL_RSVD1_DEFAULT;

    s_sRunState.u16ButtonsMask = 0x0001U;
    s_sRunState.u8Pov = HID_TOOL_POV_CENTER;
    s_sRunState.u16Frame = 0U;

    s_au8LastReport[0] = 0x80U;
    s_au8LastReport[1] = 0x80U;
    s_au8LastReport[2] = 0x80U;
    s_au8LastReport[3] = 0x80U;
    s_au8LastReport[4] = HID_TOOL_POV_CENTER;
    s_au8LastReport[5] = 0U;
    s_au8LastReport[6] = s_u8Reserved0;
    s_au8LastReport[7] = s_u8Reserved1;

    s_au16LastAdc[HID_TOOL_AXIS_X] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16LastAdc[HID_TOOL_AXIS_Y] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16LastAdc[HID_TOOL_AXIS_Z] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au16LastAdc[HID_TOOL_AXIS_RZ] = HID_TOOL_ADC_DEFAULT_CENTER;
    s_au8LastAxisHid[HID_TOOL_AXIS_X] = 0x80U;
    s_au8LastAxisHid[HID_TOOL_AXIS_Y] = 0x80U;
    s_au8LastAxisHid[HID_TOOL_AXIS_Z] = 0x80U;
    s_au8LastAxisHid[HID_TOOL_AXIS_RZ] = 0x80U;

    s_u32ReportTxSeq = 0U;
    s_u32LastLogSeq = 0U;
    s_au8AxisInDeadzone[HID_TOOL_AXIS_X] = 1U;
    s_au8AxisInDeadzone[HID_TOOL_AXIS_Y] = 1U;
    s_au8AxisInDeadzone[HID_TOOL_AXIS_Z] = 1U;
    s_au8AxisInDeadzone[HID_TOOL_AXIS_RZ] = 1U;
}

void HidTool_OnOutReady(void)
{
    /* Joystick profile has no OUT report path. */
}

void HidTool_ProcessLogTask(void)
{
    uint32_t u32Seq;
    uint16_t u16Buttons;
    uint8_t u8Pov;

    u32Seq = s_u32ReportTxSeq;
#if (HID_JOYSTICK_DEBUG_USB_PATH != 0U)
    {
        static uint16_t s_u16UsbDbgTick = 0U;
        static uint32_t s_u32LastUsbBusReset = 0U;
        static uint32_t s_u32LastUsbSetup = 0U;
        static uint32_t s_u32LastUsbEp2 = 0U;
        uint8_t u8NeedLog = 0U;

        s_u16UsbDbgTick++;
        if((s_u16UsbDbgTick >= 10U) ||
           (g_u32UsbBusResetCount != s_u32LastUsbBusReset) ||
           (g_u32UsbSetupCount != s_u32LastUsbSetup) ||
           (g_u32UsbEp2EventCount != s_u32LastUsbEp2))
        {
            u8NeedLog = 1U;
        }

        if(u8NeedLog != 0U)
        {
            printf("[DBG][USB] addr=%lu sus=%u busRst=%lu setCfg=%lu setup=%lu ep2=%lu tx=%lu attr=0x%08lX int=0x%08lX\r\n",
                   (unsigned long)USBD_GET_ADDR(),
                   (unsigned int)g_u8Suspend,
                   (unsigned long)g_u32UsbBusResetCount,
                   (unsigned long)g_u32UsbSetConfigCount,
                   (unsigned long)g_u32UsbSetupCount,
                   (unsigned long)g_u32UsbEp2EventCount,
                   (unsigned long)u32Seq,
                   (unsigned long)USBD->ATTR,
                   (unsigned long)USBD->INTSTS);
            s_u16UsbDbgTick = 0U;
            s_u32LastUsbBusReset = g_u32UsbBusResetCount;
            s_u32LastUsbSetup = g_u32UsbSetupCount;
            s_u32LastUsbEp2 = g_u32UsbEp2EventCount;
        }
    }
#endif

    if((u32Seq == 0U) || (u32Seq == s_u32LastLogSeq))
    {
        return;
    }
    s_u32LastLogSeq = u32Seq;

#if (HID_JOYSTICK_DEBUG_TX_PATH != 0U)
    u8Pov = (uint8_t)(s_au8LastReport[HID_REPORT_IDX_POV_BTN14] & 0x0FU);
    u16Buttons = (uint16_t)((((uint16_t)(s_au8LastReport[HID_REPORT_IDX_POV_BTN14] >> 4)) &
                             HID_TOOL_BUTTON_LOW_MASK) |
                            ((((uint16_t)s_au8LastReport[HID_REPORT_IDX_BTN512]) &
                              HID_TOOL_BUTTON_HIGH_MASK) << 4));
    u16Buttons = (uint16_t)(u16Buttons & HID_TOOL_BUTTON_MASK);

    printf("[JOY][TX] seq=%lu X=%u Y=%u Z=%u Rz=%u POV=0x%X BTN=0x%03X ADC=%u/%u/%u/%u RAW=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
           (unsigned long)u32Seq,
           (unsigned int)s_au8LastAxisHid[HID_TOOL_AXIS_X],
           (unsigned int)s_au8LastAxisHid[HID_TOOL_AXIS_Y],
           (unsigned int)s_au8LastAxisHid[HID_TOOL_AXIS_Z],
           (unsigned int)s_au8LastAxisHid[HID_TOOL_AXIS_RZ],
           (unsigned int)u8Pov,
           (unsigned int)u16Buttons,
           (unsigned int)s_au16LastAdc[HID_TOOL_AXIS_X],
           (unsigned int)s_au16LastAdc[HID_TOOL_AXIS_Y],
           (unsigned int)s_au16LastAdc[HID_TOOL_AXIS_Z],
           (unsigned int)s_au16LastAdc[HID_TOOL_AXIS_RZ],
           (unsigned int)s_au8LastReport[0],
           (unsigned int)s_au8LastReport[1],
           (unsigned int)s_au8LastReport[2],
           (unsigned int)s_au8LastReport[3],
           (unsigned int)s_au8LastReport[4],
           (unsigned int)s_au8LastReport[5],
           (unsigned int)s_au8LastReport[6],
           (unsigned int)s_au8LastReport[7]);
#endif           
}

void HidTool_Process(void)
{
    /* Legacy alias: keep old integration code buildable. */
    HidTool_ProcessLogTask();
}

void HidTool_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size)
{
    (void)pu8EpBuf;
    (void)u32Size;
}

void HidTool_GetLatestInReport(uint8_t *pu8Report, uint32_t u32Size)
{
    uint32_t u32Idx;

    if((pu8Report == 0) || (u32Size == 0U))
    {
        return;
    }

    for(u32Idx = 0U; u32Idx < u32Size; u32Idx++)
    {
        if(u32Idx < HID_JOYSTICK_REPORT_SIZE)
        {
            pu8Report[u32Idx] = s_au8LastReport[u32Idx];
        }
        else
        {
            pu8Report[u32Idx] = 0U;
        }
    }
}

void HidTool_SetInReport(void)
{
    uint8_t au8Report[HID_JOYSTICK_REPORT_SIZE] = {0};
    uint32_t u32Idx;

    HidTool_BuildReportFromSource(au8Report);

    USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2)), au8Report, HID_JOYSTICK_REPORT_SIZE);
    USBD_SET_PAYLOAD_LEN(EP2, HID_JOYSTICK_REPORT_SIZE);

    for(u32Idx = 0U; u32Idx < HID_JOYSTICK_REPORT_SIZE; u32Idx++)
    {
        s_au8LastReport[u32Idx] = au8Report[u32Idx];
    }

    s_u32ReportTxSeq++;
}
