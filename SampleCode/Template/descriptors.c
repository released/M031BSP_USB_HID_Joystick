/******************************************************************************
 * @file     descriptors.c
 * @version  V1.00
 * $Revision: 6 $
 * $Date: 18/04/13 3:45p $
 * @brief    M031 series USBD driver source file
 *
 * @note
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/
/*!<Includes */
#include "NuMicro.h"
#include "hid_transfer.h"
#include "hid_joystick_conf.h"

/*
 * Report layout (8 bytes):
 *  Byte0: X   (uint8, 0..255, center ~0x80)
 *  Byte1: Y   (uint8, 0..255, center ~0x80)
 *  Byte2: Z   (uint8, 0..255, center ~0x80)
 *  Byte3: Rz  (uint8, 0..255, center ~0x80)
 *  Byte4: bit0..3 = POV(Hat, 0..7; value >7 is center/null), bit4..7 = Button1..4
 *  Byte5: bit0..7 = Button5..12 (or fewer if HID_JOYSTICK_BUTTON_COUNT < 12)
 *  Byte6: Reserved/Extension0 (vendor-defined)
 *  Byte7: Reserved/Extension1 (vendor-defined)
 *
 * To change button count:
 *  1) Update HID_JOYSTICK_BUTTON_COUNT in hid_joystick_conf.h.
 *  2) Keep total report bits byte-aligned (multiple of 8). If not aligned, add Constant padding Input.
 *  3) If count > 12, you must redesign this base layout (Byte4/Byte5 cannot hold more than 12 buttons).
 *  4) Update PC-side mapping if needed.
 */
const uint8_t HID_JoystickReportDescriptor[] =
{
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)

    /* X, Y, Z, Rz axis: 4 x uint8 (0..255) */
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Min (0)
    0x26, 0xFF, 0x00,  //   Logical Max (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    /* POV Hat: 4-bit (0..7 valid, null state allowed for center) */
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x07,        //   Logical Max (7)
    0x35, 0x00,        //   Physical Min (0)
    0x46, 0x3B, 0x01,  //   Physical Max (315)
    0x65, 0x14,        //   Unit (English Rotation, degrees)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,NullState)

    /* Buttons 1..N (1-bit each), N = HID_JOYSTICK_BUTTON_COUNT */
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Min (1)
    0x29, (uint8_t)HID_JOYSTICK_BUTTON_COUNT,  //   Usage Max (Button count)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x01,        //   Logical Max (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, (uint8_t)HID_JOYSTICK_BUTTON_COUNT,  //   Report Count (Button count)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    /* Reserved/Extension bytes (Byte6..Byte7), vendor-defined */
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        //   Usage (0x01)
    0x15, 0x00,        //   Logical Min (0)
    0x26, 0xFF, 0x00,  //   Logical Max (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    0xC0               // End Collection
};
/*----------------------------------------------------------------------------*/
/*!<USB Device Descriptor */
const uint8_t gu8DeviceDescriptor[] =
{
    LEN_DEVICE,     /* bLength */
    DESC_DEVICE,    /* bDescriptorType */
    0x10, 0x01,     /* bcdUSB */
    0x00,           /* bDeviceClass */
    0x00,           /* bDeviceSubClass */
    0x00,           /* bDeviceProtocol */
    EP0_MAX_PKT_SIZE,   /* bMaxPacketSize0 */
    /* idVendor */
    USBD_VID & 0x00FF,
    ((USBD_VID & 0xFF00) >> 8),
    /* idProduct */
    USBD_PID & 0x00FF,
    ((USBD_PID & 0xFF00) >> 8),
    0x00, 0x00,     /* bcdDevice */
    0x01,           /* iManufacture */
    0x02,           /* iProduct */
    0x00,           /* iSerialNumber - no serial */
    0x01            /* bNumConfigurations */
};

/*!<USB Configure Descriptor */
const uint8_t gu8ConfigDescriptor[] =
{
    LEN_CONFIG,     /* bLength */
    DESC_CONFIG,    /* bDescriptorType */
    /* wTotalLength */
    (LEN_CONFIG+LEN_INTERFACE+LEN_HID+LEN_ENDPOINT) & 0x00FF,
    (((LEN_CONFIG+LEN_INTERFACE+LEN_HID+LEN_ENDPOINT) & 0xFF00) >> 8),
    0x01,           /* bNumInterfaces */
    0x01,           /* bConfigurationValue */
    0x00,           /* iConfiguration */
    0x80 | (USBD_SELF_POWERED << 6) | (USBD_REMOTE_WAKEUP << 5),/* bmAttributes */
    USBD_MAX_POWER,         /* MaxPower */

    /* I/F descr: HID */
    LEN_INTERFACE,  /* bLength */
    DESC_INTERFACE, /* bDescriptorType */
    0x00,           /* bInterfaceNumber */
    0x00,           /* bAlternateSetting */
    0x01,           /* bNumEndpoints */
    0x03,           /* bInterfaceClass */
    0x00,           /* bInterfaceSubClass */
    0x00,           /* bInterfaceProtocol */
    0x00,           /* iInterface */

    /* HID Descriptor */
    LEN_HID,        /* Size of this descriptor in UINT8s. */
    DESC_HID,       /* HID descriptor type. */
    0x10, 0x01,     /* HID Class Spec. release number. */
    0x00,           /* H/W target country. */
    0x01,           /* Number of HID class descriptors to follow. */
    DESC_HID_RPT,   /* Descriptor type. */
    /* Total length of report descriptor. */
    sizeof(HID_JoystickReportDescriptor) & 0x00FF,
    (sizeof(HID_JoystickReportDescriptor) & 0xFF00) >> 8,

    /* EP Descriptor: interrupt in. */
    LEN_ENDPOINT,                       /* bLength */
    DESC_ENDPOINT,                      /* bDescriptorType */
    (INT_IN_EP_NUM | EP_INPUT),         /* bEndpointAddress */
    EP_INT,                             /* bmAttributes */
    /* wMaxPacketSize */
    EP2_MAX_PKT_SIZE & 0x00FF,
    (EP2_MAX_PKT_SIZE & 0xFF00) >> 8,
    HID_DEFAULT_INT_IN_INTERVAL     /* bInterval */
};

/*!<USB Language String Descriptor */
const uint8_t gu8StringLang[4] =
{
    4,              /* bLength */
    DESC_STRING,    /* bDescriptorType */
    0x09, 0x04
};

/*!<USB Vendor String Descriptor */
const uint8_t gu8VendorStringDesc[] =
{
    16,
    DESC_STRING,
    'N', 0, 'u', 0, 'v', 0, 'o', 0, 't', 0, 'o', 0, 'n', 0
};

/*!<USB Product String Descriptor */
const uint8_t gu8ProductStringDesc[] =
{
    26,             /* bLength          */
    DESC_STRING,    /* bDescriptorType  */
    'U', 0, 'S', 0, 'B', 0, ' ', 0, 'J', 0, 'o', 0, 'y', 0, 's', 0, 't', 0, 'i', 0, 'c', 0, 'k', 0
};

/*!<USB BOS Descriptor */
uint8_t gu8BOSDescriptor[] =
{
    LEN_BOS,        /* bLength */
    DESC_BOS,       /* bDescriptorType */
    /* wTotalLength */
    0x0C & 0x00FF,
    ((0x0C & 0xFF00) >> 8),
    0x01,           /* bNumDeviceCaps */

    /* Device Capability */
    LEN_BOSCAP,     /* bLength */
    DESC_CAPABILITY,/* bDescriptorType */
    CAP_USB20_EXT,  /* bDevCapabilityType */
    0x02, 0x00, 0x00, 0x00  /* bmAttributes */
};

const uint8_t *gpu8UsbString[4] =
{
    gu8StringLang,
    gu8VendorStringDesc,
    gu8ProductStringDesc,
    0,
};

const uint8_t *gpu8UsbHidReport[2] =
{
    HID_JoystickReportDescriptor,
    NULL
};

const uint32_t gu32UsbHidReportLen[2] =
{
    sizeof(HID_JoystickReportDescriptor),
    0
};

const uint32_t gu32ConfigHidDescIdx[2] =
{
    (LEN_CONFIG + LEN_INTERFACE),
    0
};

const S_USBD_INFO_T gsInfo =
{
    (uint8_t *)gu8DeviceDescriptor,
    (uint8_t *)gu8ConfigDescriptor,
    (uint8_t **)gpu8UsbString,
    (uint8_t **)gpu8UsbHidReport,
    NULL,
    (uint32_t *)gu32UsbHidReportLen,
    (uint32_t *)gu32ConfigHidDescIdx,
};

