/*
 * MAIN Generated Driver File
 *
 * @file main.c
 *
 * @defgroup main MAIN
 *
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.2
 *
 * @version Package Version: 3.1.2
 */

/*
© [2024] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip
    software and any derivatives exclusively with Microchip products.
    You are responsible for complying with 3rd party license terms
    applicable to your use of 3rd party software (including open source
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.?
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR
    THIS SOFTWARE.
 */

#include <stdbool.h>
#include <avr/io.h>

// MCC includes
#include "mcc_generated_files/system/system.h"
#include <usb_hid_mouse.h>
#include <usb_core.h>

#define SW1_bm PIN0_bm
#define SW2_bm PIN1_bm
#define SW3_bm PIN2_bm
#define J_PUSH_bm PIN3_bm
#define J_RIGHT_bm PIN4_bm
#define J_DOWN_bm PIN5_bm
#define J_LEFT_bm PIN6_bm
#define J_UP_bm PIN7_bm

#define MOUSE_SENSITIVITY 1
// Number of consecutive equal AC measurements before power is seen as stable
#define AC_MEASUREMENT_STABLE_COUNT 5U

void InputDebouncer();
void USB_ConnectionHandler();
void USB_PowerHandler();

RETURN_CODE_t ButtonLeftClick();
RETURN_CODE_t ButtonRightClick();

// Variable for input port debounce
uint8_t inputBeforeDebounce = 0x00;
uint8_t inputAfterDebounce = 0x00;

// Interrupt variables
volatile bool vbusFlag = false;
bool sendMouseMovement = false;

// USB status variable
RETURN_CODE_t status;

int main(void)
{


    SYSTEM_Initialize();

    // Set callbacks
    RTC_SetOVFIsrCallback(USB_PowerHandler);
    RTC_SetPITIsrCallback(InputDebouncer);

    while (1)
    {
        USB_ConnectionHandler();
        // Check USB status
        if (SUCCESS == status)
        {
            status = ButtonLeftClick();
        }
        if (SUCCESS == status)
        {
            status = ButtonRightClick();
        }

        if (SUCCESS == status)
        {
            if (sendMouseMovement)
            {
                sendMouseMovement = false;

                int8_t xMove = 0;
                int8_t yMove = 0;
                // Translates joystick button inputs to mouse movement
                if (!(inputAfterDebounce & J_UP_bm))
                {
                    yMove = -MOUSE_SENSITIVITY;
                }
                else if (!(inputAfterDebounce & J_DOWN_bm))
                {
                    yMove = MOUSE_SENSITIVITY;
                }

                if (!(inputAfterDebounce & J_RIGHT_bm))
                {
                    xMove = MOUSE_SENSITIVITY;
                }
                else if (!(inputAfterDebounce & J_LEFT_bm))
                {
                    xMove = -MOUSE_SENSITIVITY;
                }

                // Sends mouse movement with USB HID
                status = USB_HIDMouseMove(xMove, yMove);
            }
        }
        else
        {
            ; // INSERT ERROR CODE
        }
    }
}

RETURN_CODE_t ButtonLeftClick()
{
    static bool buttonPressedLeft = false;

    RETURN_CODE_t returnCode = SUCCESS;

    if ((!(inputAfterDebounce & SW2_bm) || !(inputAfterDebounce & J_PUSH_bm)) && !buttonPressedLeft)
    {
        // Send button down(press) with USB HID
        buttonPressedLeft = true;
        returnCode = USB_HIDMouseButtonLeft(true);
    }
    else if (((inputAfterDebounce & SW2_bm) && (inputAfterDebounce & J_PUSH_bm)) && buttonPressedLeft)
    {
        // Send button up(release) with USB HID
        buttonPressedLeft = false;
        returnCode = USB_HIDMouseButtonLeft(false);
    }
    return returnCode;
}

RETURN_CODE_t ButtonRightClick()
{
    static bool buttonPressedRight = false;

    RETURN_CODE_t returnCode = SUCCESS;

    if (!(inputAfterDebounce & SW3_bm) && !buttonPressedRight)
    {
        // Send button down(press) with USB HID
        buttonPressedRight = true;
        returnCode = USB_HIDMouseButtonRight(true);
    }
    else if ((inputAfterDebounce & SW3_bm) && buttonPressedRight)
    {
        // Send button up(release) with USB HID
        buttonPressedRight = false;
        returnCode = USB_HIDMouseButtonRight(false);
    }
    return returnCode;
}

void InputDebouncer()
{
    sendMouseMovement = true;
    // input debouncer 5ms delay
    if (inputBeforeDebounce == PORTA.IN)
    {
        inputAfterDebounce = inputBeforeDebounce;
    }
    inputBeforeDebounce = PORTA.IN;
}

/**
 * Routine that checks the connectivity of the USB peripheral and start/stops the USB driver when connected/disconnected
 */
void USB_ConnectionHandler()
{
    static volatile bool prevVbusState = false;
    // Check if VBUS was true last check, indicating that USB was connected
    if (prevVbusState == true)
    {
        // Handle USB Transfers
        status = USBDevice_StatusGet();
    }
    // Get current status of VBUS
    bool currentVbusState = vbusFlag;
    // If changes to USB VBUS state
    if (currentVbusState != prevVbusState)
    {
        // If USB has been connected
        if (currentVbusState == true)
        {
            // Start USB operations
            status = USB_Start();
            LED_SetHigh();
        }
        else
        {
            // Stop USB operations
            status = USB_Stop();
            LED_SetLow();
        }
        // Save state
        prevVbusState = currentVbusState;
    }
}

/**
 * Interrupt routine used to check VBUS status using the Real-Time Counter overflow interrupt and Analog Comparator
 */
void USB_PowerHandler()
{
    // Helper variables for RTC and AC
    static volatile uint8_t acStateCounter = 0U;
    static volatile uint8_t acPrevState = 0U;

    uint8_t acCurrentState = AC0_Read(); // Saves the current state of the AC status register ('1' means above threshold)

    // Makes sure that the AC state has been the same for a certain number of PIT interrupts
    if (acStateCounter == AC_MEASUREMENT_STABLE_COUNT)
    {
        if (acCurrentState) // When CMPSTATE is high, the AC output is high which means the VBUS is above the threshold and the pull-up on data+ should be enabled.
        {
            vbusFlag = true;
        }
        else
        {
            vbusFlag = false;
        }
        acStateCounter++; // Increment the counter to not run start/stop multiple times without an actual state change
    }
        // Increments a counter if the AC has been in the same state for a certain number of PIT interrupts
    else if (acCurrentState == acPrevState)
    {
        if (acStateCounter < AC_MEASUREMENT_STABLE_COUNT)
        {
            acStateCounter++;
        }
        else
        {
            ; // Stops counting if the AC has been in the same state for a while.
        }
    }
    else
    {
        acStateCounter = 0; // Resets the PIT counter if a state change has occurred.
    }
    acPrevState = acCurrentState;
}