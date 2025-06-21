#include <Arduino.h>
#include "HardwareSerial.h"
#include "driver/uart.h"

#define UART_NUM UART_NUM_2  // Using UART2 for communication
#define TX_PIN 17  // ESP32 UART TX
#define RX_PIN 16  // ESP32 UART RX
#define SWITCH_PIN 4  // GPIO for switch
 //GPIO for buck enable

// Define UART Configuration
uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
};

// Define Data Frames for Display Control
uint8_t turnOnDisplay[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x64, 0x64};  // Turn ON Display
uint8_t switchHomePage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};  // Switch to Home Page
uint8_t turnOffDisplay[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x00, 0x00};  // Turn OFF Backlight
uint8_t switchPage20[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x14};  // Switch to Page 20
uint8_t switchInstantConfirmationPage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0xE8, 0x5A, 0x01, 0x00, 0x15};  // Instant Page 21 Confirmation
uint8_t removeOverlayScreen[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0xE8, 0x00, 0x00};
// Interrupt variables
volatile bool longPressDetected = false;
volatile unsigned long pressStartTime = 0;
bool waitingForUserInput = false; // Track confirmation page state
bool systemPoweredOn = true; // Ensure short press only works at startup

// Interrupt Service Routine (ISR) for switch
void IRAM_ATTR switchISR() {
    if (digitalRead(SWITCH_PIN) == LOW) {
        if (pressStartTime == 0) {
            pressStartTime = millis();
        }
    } else { // Button Released
        if (pressStartTime > 0) {
            if ((millis() - pressStartTime) >= 1000) {
                longPressDetected = true;                
            }
        }
        pressStartTime = 0;
    }
}

// Function to send UART commands
void sendUartCommand(uint8_t *command, size_t length) {
    uart_write_bytes(UART_NUM, (const char*)command, length);
}

// Function to Process User Selection for "Proceed"
void processUserInput() {
    uint8_t data[10]; // Increased buffer size
    systemPoweredOn = false;
    int len = uart_read_bytes(UART_NUM, data, sizeof(data), 200 / portTICK_PERIOD_MS);

    if (len > 0) {
        Serial.print("Received UART Data: ");
        for (int i = 0; i < len; i++) {
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        // Detect "Proceed" button press
        if (len >= 9 && data[0] == 0x5A && data[1] == 0xA5 && data[2] == 0x06 && data[3] == 0x83 &&
            data[4] == 0x89 && data[5] == 0x00 && data[6] == 0x01 && data[7] == 0x00 && data[8] == 0x01) {  
            
            Serial.println("Proceed Confirmed: Turning OFF Backlight & Switching to Page 20...");
            sendUartCommand(turnOffDisplay, sizeof(turnOffDisplay));
            delay(200);
            sendUartCommand(switchPage20, sizeof(switchPage20));                                // Turning OFF the display and switches to Black screen and Removes overlay page.
            delay(200);
            sendUartCommand(removeOverlayScreen, sizeof(removeOverlayScreen));
            waitingForUserInput = false;
            systemPoweredOn = true;    
        }
    }
}

void setup() {
    Serial.begin(115200);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, 1024 * 2, 0, 0, NULL, 0);
    
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, CHANGE);
    // Performing system Start up action    
    sendUartCommand(switchPage20, sizeof(switchPage20));  // Show Home Page at startup    
}

void loop() {
    if (systemPoweredOn && digitalRead(SWITCH_PIN) == LOW) {
        uart_write_bytes(UART_NUM, (const char*)switchHomePage, sizeof(switchHomePage)); // Switch to Home Page
        delay(100);
        Serial.println("Short Press Detected: Turning ON Display & Switching to Home Page...");
        uart_write_bytes(UART_NUM, (const char*)turnOnDisplay, sizeof(turnOnDisplay)); // Turn ON Display
        systemPoweredOn = false;  // Disable short press in normal operation
    }

    if (longPressDetected) {
        Serial.println("Long Press Detected: Bringing Instant Confirmation Page...");
        systemPoweredOn = false;        
        sendUartCommand(switchInstantConfirmationPage, sizeof(switchInstantConfirmationPage)); // Show Confirmation Page (Page 21 instantly)
        waitingForUserInput = true;  // Begin waiting for user response
        longPressDetected = false;
       
    }

    if (waitingForUserInput) {
        processUserInput();     // processing function after receiving user input (i.e) Proceed Button
    }
    
    delay(10); // Small delay for stability
}
