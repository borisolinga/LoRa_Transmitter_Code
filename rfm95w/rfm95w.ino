// This Arduino code configures and controls an RFM95 LoRa module to periodically transmit an 8-byte message. 
// It allows dynamic configuration of bandwidth and spreading factor via serial input, manages power consumption 
// by enabling/disabling a load switch, and enters sleep mode between transmissions to optimize energy efficiency. 
// The system can be paused and resumed using serial commands ("STOP" and "GO").

#include <SPI.h>
#include <RH_RF95.h>
#include <LowPower.h> // Library for power-saving features

// Pin configuration
#define RFM95_CS 10       // Chip-Select
#define RFM95_RST 9       // Reset
#define RFM95_IRQ 2       // Interrupt (DIO0)
#define LED_PIN 5        // LED
#define SWITCH_PIN 7      // Load switch

uint8_t message[8] = {0xA7, 0xF1, 0xD9, 0x2A, 0x82, 0xC8, 0xD8, 0xFE}; // 8-byte message
unsigned long totalPacketCount = 0; // Total packets sent
bool isPaused = false; // Flag to control if the process is paused

// RF95 instance
RH_RF95 rf95(RFM95_CS, RFM95_IRQ);

// Function declarations
void configureParameters();
void initializeSystem();
void prepareTransmission();
void waitForNextCycle();
void sendMessage();
void checkForStopOrGo();
void initializeRF95Module();
void initializeLED();
void initializeSwitch();
void blinkLED(int duration);
void displayMessage(uint8_t* message, size_t size);
void displayPacketCount();
int getUserBandwidthInput();
int getUserSpreadingFactorInput();
void setBandwidth(int bwCode);
void setSpreadingFactor(int sf);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  initializeSystem(); // Complete system initialization
  configureParameters(); // Initial parameter configuration
}

void loop() {
  checkForStopOrGo(); // Check user commands STOP/GO

  if (!isPaused) {
    prepareTransmission(); // Prepare transmission
    sendMessage();         // Send LoRa message
    waitForNextCycle();    // Sleep cycle
  }
}

// Complete system initialization
void initializeSystem() {
  initializeRF95Module();
  initializeLED();
  initializeSwitch();
  Serial.println("RF95 module and system initialized.");
}

// Initialize the switch (load switch)
void initializeSwitch() {
  pinMode(SWITCH_PIN, OUTPUT);
  digitalWrite(SWITCH_PIN, LOW); // Disabled by default
  Serial.println("Switch initialized.");
}

// Prepare for transmission
void prepareTransmission() {
  Serial.println("Preparing for transmission...");
  rf95.setModeIdle();        // Set module to Standby mode
  digitalWrite(SWITCH_PIN, HIGH); // Activate switch to power the module
  delay(1000);               // Wait before transmission
}

// Send a LoRa message
void sendMessage() {
  displayMessage(message, sizeof(message)); // Show the message before sending
  if (rf95.send(message, sizeof(message))) {
    Serial.println("Message sent.");
    blinkLED(500); // Visual indication of message sent
    totalPacketCount++;
    displayPacketCount(); // Show total packet count
  } else {
    Serial.println("Message failed to send.");
  }
}

// Sleep mode before the next cycle
void waitForNextCycle() {
  Serial.println("Entering sleep mode...");
  rf95.sleep();               // Put LoRa module to sleep
  digitalWrite(SWITCH_PIN, LOW); // Disable switch to save power
  digitalWrite(LED_PIN, LOW);  // Turn off LED

  // Sleep for 3 seconds (12 cycles of 250 ms)
  for (int i = 0; i < 12; i++) { 
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  }
}

// Initialize the LoRa module (RF95)
void initializeRF95Module() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  // Reset the module
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println("RF95 initialization failed.");
    while (1);
  }

  rf95.setFrequency(868.0);
  rf95.setTxPower(7, false); // Set TX power to 7 dBm

  // Set preamble length
  rf95.spiWrite(0x20, 0x00); // RegPreambleMsb = 0
  rf95.spiWrite(0x21, 0x08); // RegPreambleLsb = 8
// CRC ON
uint8_t modemConfig2 = rf95.spiRead(0x1E);   // Read the RegModemConfig2 register
modemConfig2 |= (1 << 2);                   // Set bit 2 (CRC_ON) to 1
rf95.spiWrite(0x1E, modemConfig2);          // Write the new value back to the register


  Serial.println("RF95 initialized with custom settings.");
}

// Parameter configuration
void configureParameters() {
  int bwCode = getUserBandwidthInput();
  setBandwidth(bwCode);

  int sf = getUserSpreadingFactorInput();
  setSpreadingFactor(sf);
}

// User input for bandwidth
int getUserBandwidthInput() {
  int bwCode = -1;
  Serial.println("Enter bandwidth code (0-9):");
  while (bwCode < 0) {
    if (Serial.available()) {
      bwCode = Serial.parseInt();
      if (bwCode >= 0 && bwCode <= 9) {
        Serial.print("Selected bandwidth: ");
        Serial.println(bwCode);
      } else {
        Serial.println("Invalid bandwidth code. Please try again.");
        bwCode = -1; // Reset to wait for valid input
      }
    }
  }
  return bwCode;
}

// User input for spreading factor
int getUserSpreadingFactorInput() {
  int sf = -1;
  Serial.println("Enter spreading factor (6-12):");
  while (sf < 0) {
    if (Serial.available()) {
      sf = Serial.parseInt();
      if (sf >= 6 && sf <= 12) {
        Serial.print("Selected Spreading Factor: ");
        Serial.println(sf);
      } else {
        Serial.println("Invalid Spreading Factor. Please try again.");
        sf = -1; // Reset to wait for valid input
      }
    }
  }
  return sf;
}

// Set bandwidth
void setBandwidth(int bwCode) {
  uint8_t bwValues[] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90};
  if (bwCode >= 0 && bwCode < 10) {
    uint8_t modemConfig1 = rf95.spiRead(0x1D); // Read RegModemConfig1
    modemConfig1 &= 0x0F; // Clear the bandwidth bits
    modemConfig1 |= bwValues[bwCode]; // Set the bandwidth bits
    rf95.spiWrite(0x1D, modemConfig1); // Write back to RegModemConfig1
    Serial.print("Bandwidth set to code: ");
    Serial.println(bwCode);
  }
}

// Set spreading factor
void setSpreadingFactor(int sf) {
  if (sf >= 6 && sf <= 12) {
    uint8_t modemConfig2 = rf95.spiRead(0x1E); // Read RegModemConfig2
    modemConfig2 &= 0x0F; // Clear the spreading factor bits
    modemConfig2 |= (sf << 4); // Set the spreading factor bits
    rf95.spiWrite(0x1E, modemConfig2); // Write back to RegModemConfig2
    Serial.print("Spreading Factor set to: ");
    Serial.println(sf);
  }
}

// Initialize the LED
void initializeLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Turn off by default
}

// Blink LED
void blinkLED(int duration) {
  digitalWrite(LED_PIN, HIGH);
  delay(duration);
  digitalWrite(LED_PIN, LOW);
}

// Display message before sending
void displayMessage(uint8_t* message, size_t size) {
  Serial.print("Message to send: ");
  for (size_t i = 0; i < size; i++) {
    Serial.print("0x");
    if (message[i] < 0x10) Serial.print("0");
    Serial.print(message[i], HEX);
    if (i < size - 1) Serial.print(", ");
  }
  Serial.println();
}

// Display total packets sent
void displayPacketCount() {
  Serial.print("Total packets sent: ");
  Serial.println(totalPacketCount);
}

// Handle STOP/GO user commands
void checkForStopOrGo() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.equalsIgnoreCase("STOP")) {
      isPaused = true;
      Serial.println("Process paused. Enter 'GO' to resume.");
    } else if (input.equalsIgnoreCase("GO")) {
      isPaused = false;
      Serial.println("Process resumed.");
    }
  }
}
