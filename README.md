# M031BSP_USB_HID_Joystick
M031BSP_USB_HID_Joystick

update @ 2026/03/18

1. init UART0 (PB12 : UART0_RX , PB13 : UART0_TX) for printf

2. init USB device , by using M032 EVB

- USB VID / PID 
```c
/* Define the vendor id and product id */
#define USBD_VID        0x0416
#define USBD_PID        0x5020
```

3. under \SampleCode\Template\JoyStickTestTool\JoyStickTestTool.exe , is HID Joystick tool for test USB HID Joystick communication

![image](https://github.com/released/M031BSP_USB_HID_Joystick/blob/main/JoyStickTestTool.jpg) 


4. under hid_joystick_conf.h , need to modify button GPIO , base on actual design

```c

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
```

5. use joystick shield with M032 EVB , to test HID joystick behavior


![image](https://github.com/released/M031BSP_USB_HID_Joystick/blob/main/Funduino_Joystick_Shield_Connections.jpg) 


![image](https://github.com/released/M031BSP_USB_HID_Joystick/blob/main/KY_023_Shield_PIC_2.png) 


6. target c code : hid_joystick_api.c , hid_joystick_api.h

- how to porting in BSP code , 

```c

void USBD_IRQHandler(void)
{
	...
	
    if (u32IntSts & USBD_INTSTS_BUS)
    {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if (u32State & USBD_STATE_USBRST)
        {
            /* Bus reset */
            USBD_ENABLE_USB();
            USBD_SwReset();
            g_u8Suspend = 0;
            g_u32UsbBusResetCount++;
#if 1
            HidTool_ResetState();
#else
            /* legacy: no extra reset actions */
#endif
		
	...
}


void EP2_Handler(void)  /* Interrupt IN handler */
{
    HID_SetInReport();
}

void EP3_Handler(void)  /* Interrupt OUT handler */
{
#if 1
    /* Joystick profile does not use Interrupt OUT endpoint. */
#else
    uint8_t *ptr;
    /* Interrupt OUT */
    ptr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP3));
    HID_GetOutReport(ptr, USBD_GET_PAYLOAD_LEN(EP3));
    USBD_SET_PAYLOAD_LEN(EP3, EP3_MAX_PKT_SIZE);
#endif
}


void HID_GetOutReport(uint8_t *pu8EpBuf, uint32_t u32Size)
{
#if 1
    HidTool_GetOutReport(pu8EpBuf, u32Size);
#else
...
}


void HID_SetInReport(void)
{
#if 1
    HidTool_SetInReport();
#else
...
}


```

7. by using tool (JoyStickTestTool\JoyStickTestTool.exe) , to test MCU HID joy stick behavior

below is video 


![image](https://github.com/released/M031BSP_USB_HID_Joystick/blob/main/operation.mp4) 


8. below is test log 


![image](https://github.com/released/M031BSP_USB_HID_Joystick/blob/main/log.jpg) 

