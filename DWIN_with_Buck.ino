// #define BUCK_ENABLE_PIN 25  // Change to your desired GPIO

// void setup() {
//   Serial.begin(115200);
//   pinMode(BUCK_ENABLE_PIN, INPUT_PULLDOWN);
//   pinMode(BUCK_ENABLE_PIN, OUTPUT);
  
  
//   // Start with buck disabled
  
// }

// void loop() {
 
//     // Wait 3 seconds

//   // Enable buck to power DWIN
//   digitalWrite(BUCK_ENABLE_PIN, HIGH);
//   Serial.println("Buck Enabled - DWIN Power ON");
//   delay(6000);  // Wait 5 seconds

//   //Disable again for test
//   digitalWrite(BUCK_ENABLE_PIN, LOW);
//   Serial.println("Buck Disabled - DWIN Power OFF");// Nothing in loop – one-time test in setup()
//   delay(6000);

#include <Arduino.h>
#include "HardwareSerial.h"
#include "driver/uart.h"

#define UART_NUM UART_NUM_2  // Using UART2 for communication
#define TX_PIN 17  // ESP32 UART TX
#define RX_PIN 16  // ESP32 UART RX
#define SWITCH_PIN 4  // GPIO for switch
#define BUCK_ENABLE_PIN 25  // GPIO connected to buck converter ENABLE

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
//uint8_t switchHomePage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};  // Home Page
uint8_t switchInstantConfirmationPage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0xE8, 0x5A, 0x01, 0x00, 0x15};  // Confirmation Page
//uint8_t removeOverlayScreen[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0xE8, 0x00, 0x00};

// Interrupt variables
volatile bool longPressDetected = false;
volatile unsigned long pressStartTime = 0;
bool waitingForUserInput = false;
bool systemPoweredOn = true;

// Interrupt Service Routine
void IRAM_ATTR switchISR() {
    if (digitalRead(SWITCH_PIN) == LOW) {
        if (pressStartTime == 0) {
            pressStartTime = millis();
        }
    } else {
        if (pressStartTime > 0) {
            if ((millis() - pressStartTime) >= 1000) {
                longPressDetected = true;
            }
        }
        pressStartTime = 0;
    }
}

void sendUartCommand(uint8_t *command, size_t length) {
    uart_write_bytes(UART_NUM, (const char*)command, length);
}

void processUserInput() {
    uint8_t data[10];
    systemPoweredOn = false;
    int len = uart_read_bytes(UART_NUM, data, sizeof(data), 200 / portTICK_PERIOD_MS);

    if (len > 0) {
        Serial.print("Received UART Data: ");
        for (int i = 0; i < len; i++) {
            Serial.print(data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        if (len >= 9 && data[0] == 0x5A && data[1] == 0xA5 && data[2] == 0x06 &&
            data[3] == 0x83 && data[4] == 0x89 && data[5] == 0x00 &&
            data[6] == 0x01 && data[7] == 0x00 && data[8] == 0x01) {

            Serial.println("Proceed Confirmed: Shutting down DWIN power...");
            digitalWrite(BUCK_ENABLE_PIN, LOW);  // Cut power to DWIN
            delay(200);
            //sendUartCommand(removeOverlayScreen, sizeof(removeOverlayScreen));  // Optional
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
    
    pinMode(BUCK_ENABLE_PIN, INPUT_PULLDOWN);
    pinMode(BUCK_ENABLE_PIN, OUTPUT);
    digitalWrite(BUCK_ENABLE_PIN, LOW);  // Keep DWIN powered OFF at startup

    //sendUartCommand(switchHomePage, sizeof(switchHomePage));  // Ready the page (if powered later)
}

void loop() {
    if (systemPoweredOn && digitalRead(SWITCH_PIN) == LOW) {
        Serial.println("Short Press Detected: Powering ON DWIN...");
        digitalWrite(BUCK_ENABLE_PIN, HIGH);  // Power ON DWIN
        // delay(100);
        // sendUartCommand(switchHomePage, sizeof(switchHomePage));
        // systemPoweredOn = false;
    }

    if (longPressDetected) {
        Serial.println("Long Press Detected: Showing Confirmation Page...");
        systemPoweredOn = false;
        sendUartCommand(switchInstantConfirmationPage, sizeof(switchInstantConfirmationPage));
        waitingForUserInput = true;
        longPressDetected = false;
    }

    if (waitingForUserInput) {
        processUserInput();
    }

    delay(10);
}

  
// }
