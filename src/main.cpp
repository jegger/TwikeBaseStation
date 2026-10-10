// IO check for Waveshare ESP32-S3-POE-ETH-8DI-8DO
//
// - Scans the I2C bus (TCA9554 IO expander + RTC)
// - Walks DO1..DO8 one at a time, then all on / all off (on demand via 'w')
// - Continuously prints DI1..DI8 and reports every change
// - Beeps the buzzer and blinks the RGB LED as a sign of life
//
// Serial commands (115200):
//   1..8  toggle that digital output
//   a     all outputs on
//   o     all outputs off
//   w     re-run the output walk
//   s     re-run the I2C scan
//   i     print current DI and DO state

#include <Arduino.h>
#include <Wire.h>

// Pins (from the Waveshare wiki)
static const uint8_t DI_PINS[8] = {4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t I2C_SDA = 42;
static const uint8_t I2C_SCL = 41;
static const uint8_t BUZZER_PIN = 46;
static const uint8_t RGB_PIN = 38;

// TCA9554 (drives DO1..DO8 via EXIO1..EXIO8)
static const uint8_t TCA9554_ADDR = 0x20;
static const uint8_t TCA_REG_OUTPUT = 0x01;
static const uint8_t TCA_REG_CONFIG = 0x03;

static uint8_t doState = 0x00;
static uint8_t lastDi = 0x00;

bool tcaWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// Outputs are active low at the expander (pin HIGH = channel off), so invert
// here and keep doState as 1 = ON everywhere else.
bool setOutputs(uint8_t mask) {
  doState = mask;
  return tcaWrite(TCA_REG_OUTPUT, (uint8_t)~doState);
}

uint8_t readInputs() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < 8; i++) {
    // Inputs are active low (closed contact pulls the pin LOW) -> 1 = active
    if (!digitalRead(DI_PINS[i])) mask |= (1 << i);
  }
  return mask;
}

void printMask(const char *label, uint8_t mask) {
  Serial.printf("%s ", label);
  for (uint8_t i = 0; i < 8; i++) Serial.print((mask >> i) & 1 ? '1' : '0');
  Serial.println("  (ch1..ch8)");
}

void i2cScan() {
  Serial.println("I2C scan:");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  no devices found!");
}

void beep(uint16_t ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void outputWalk() {
  Serial.println("Output walk: DO1..DO8");
  for (uint8_t i = 0; i < 8; i++) {
    bool ok = setOutputs(1 << i);
    Serial.printf("  DO%u ON %s\n", i + 1, ok ? "" : "(I2C write FAILED)");
    delay(500);
  }
  Serial.println("  all ON");
  setOutputs(0xFF);
  delay(1000);
  Serial.println("  all OFF");
  setOutputs(0x00);
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '8') {
      uint8_t ch = c - '1';
      setOutputs(doState ^ (1 << ch));
      Serial.printf("DO%u -> %s\n", ch + 1, (doState >> ch) & 1 ? "ON" : "OFF");
    } else if (c == 'a') {
      setOutputs(0xFF);
      Serial.println("all outputs ON");
    } else if (c == 'o') {
      setOutputs(0x00);
      Serial.println("all outputs OFF");
    } else if (c == 'w') {
      outputWalk();
    } else if (c == 's') {
      i2cScan();
    } else if (c == 'i') {
      printMask("DI state:  ", readInputs());
      printMask("DO state:  ", doState);
    }
  }
}

void setup() {
  // Force all outputs OFF first thing, before the pins become outputs.
  // (TCA9554 power-up latch is 0xFF, which is already "all off".)
  Wire.begin(I2C_SDA, I2C_SCL);
  bool ok = setOutputs(0x00) && tcaWrite(TCA_REG_CONFIG, 0x00);

  Serial.begin(115200);
  delay(2000);  // give USB CDC time to enumerate
  Serial.println("\n=== ESP32-S3-POE-ETH-8DI-8DO IO check ===");
  Serial.printf("Flash: %u MB, PSRAM: %u KB\n", ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / 1024);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  // Optocoupler outputs only pull low; without a pull-up the pins float
  for (uint8_t pin : DI_PINS) pinMode(pin, INPUT_PULLUP);

  i2cScan();
  Serial.printf("TCA9554 @0x%02X init (all outputs OFF): %s\n", TCA9554_ADDR,
                ok ? "OK" : "FAILED");

  beep(100);

  lastDi = readInputs();
  printMask("DI initial:", lastDi);
  Serial.println("Commands: 1-8 toggle DO, a=all on, o=all off, w=walk, s=scan, i=show IO state");
}

void loop() {
  handleSerial();

  // Debounce: accept a new input state only after it has been stable for 30ms
  static uint8_t pendingDi = 0;
  static uint32_t pendingSince = 0;
  uint8_t raw = readInputs();
  if (raw != pendingDi) {
    pendingDi = raw;
    pendingSince = millis();
  }
  uint8_t di = lastDi;
  if (millis() - pendingSince >= 30) di = pendingDi;

  if (di != lastDi) {
    for (uint8_t i = 0; i < 8; i++) {
      if (((di ^ lastDi) >> i) & 1) {
        Serial.printf("DI%u changed -> %u\n", i + 1, (di >> i) & 1);
      }
    }
    printMask("DI state:  ", di);
    lastDi = di;
    beep(20);
  }

  // Heartbeat on the RGB LED
  static uint32_t lastBlink = 0;
  static bool ledOn = false;
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    ledOn = !ledOn;
    rgbLedWrite(RGB_PIN, 0, ledOn ? 20 : 0, 0);
  }

  delay(10);
}
