#include <FlexCAN_T4.h>
#include <SPI.h>
#include "config.h"
#include <Adafruit_MAX31855.h>
#include <SoftwareSerial.h>


static FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can0;
SoftwareSerial nextion(16, 17);
static float zeroOffsetVoltage_mV = 0.0f;


void displayWrite(const char *component, const char *label, int value, const char *unit) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s.txt=\"%s %d %s\"", component, label, value, unit);
  nextion.print(buf);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
}


static void onCanFrame(const CAN_message_t &msg) {
  if (msg.id == MSG_ID_PACK_DATA) {
gith

    displayWrite("t0", "Voltage", msg.buf[1] / 10, "V");
    displayWrite("t1", "Avg Temp", msg.buf[0], "C");
    displayWrite("t2", "SOC", msg.buf[2] / 2, "%");
    displayWrite("t3", "Max ", msg.buf[3], "C");
    displayWrite("t4", "Min ", msg.buf[4], "C");
  }

  else if (msg.id == MSG_ID_PACK_STATUS) {
    uint8_t byte0 = msg.buf[0];
    uint8_t byte1 = msg.buf[1];

    if (byte0 & STATUS_VOLT_TOO_HIGH_MASK) {
      displayWrite("t4", "VOLT TOO HIGH", 0, "X");
    }
    if (byte1 & STATUS_REDUN_SUPPLY_MASK) {
      displayWrite("t4", "REDUN SUPPLY", 0, "X");
    }
  }

  else if (msg.id == MSG_ID_VSM_STATE) {
    uint8_t vsmState = msg.buf[0] & VSM_STATE_MASK;
    if (vsmState == VSM_STATE_DRIVE_ENABLED) {
      displayWrite("t4", "Drive ENABLED", 0, "X");

    } else if (vsmState == VSM_STATE_FAULT) {
      displayWrite("t4", "FAULT", 0, "X");
    }
  }

  else if (msg.id == MSG_ID_FAULT_FLAGS) {
    if (msg.buf[0] & FAULT_FLAGS_BIT0) {
      digitalWrite(FAULT_LED, 1);
    }

    else {
      digitalWrite(FAULT_LED, 0);
    }
  }

  else {
    displayWrite("t0","CAN_DC", 1, "C");
    displayWrite("t3", "X", 1, "V");
    displayWrite("t2", "X", 1, "%");
    displayWrite("t10", "X ", 1, "C");
    displayWrite("t8", "X ", 1, "C");
  }
}

static bool CurrentSensorRdy = false;
static int pwmCmd = 0;
static float target_I = 0.0f;

static void PumpController() {

  if (!CurrentSensorRdy) {
    float total = 0.0f;
    for (int i = 0; i < CURRENT_CAL_SAMPLES; i++) {
      int rawADC = analogRead(CURRENT_SENSOR_PIN);
      total += (rawADC * ADC_REF_MILLIVOLTS) / ADC_MAX_COUNTS;
      delay(CURRENT_CAL_DELAY_MS);
    }
    zeroOffsetVoltage_mV = total / CURRENT_CAL_SAMPLES;
    Serial.print("Calibrated Zero-Current Offset (mV): ");
    Serial.println(zeroOffsetVoltage_mV);

    pinMode(PWM_PIN, OUTPUT);
    pinMode(FAULT_LED, OUTPUT);
    pinMode(PWR_LED, OUTPUT);

    analogWriteResolution(8);
    analogWriteFrequency(PWM_PIN, 1000);
    analogWrite(PWM_PIN, 0);

    CurrentSensorRdy = true;
    return;  
  }

  int rawADC = analogRead(CURRENT_SENSOR_PIN);
  float sampleVoltage_mV = (rawADC * ADC_REF_MILLIVOLTS) / ADC_MAX_COUNTS;
  float amps = (sampleVoltage_mV - zeroOffsetVoltage_mV) / CURRENT_SENSITIVITY_MV_PER_A;
  amps = amps * 10;
  displayWrite("t4", "Amps", amps, "X");
  Serial.print("Current (A): ");
  Serial.println(amps, 2);

  static unsigned long five_seconds = millis();
  static unsigned long short_time = millis();
  static unsigned long last_step_ms = 0;
  
  if (millis() - five_seconds >= 5000) {
    target_I = amps;
    five_seconds = millis();
  }

  if (millis() - short_time >= 50) {
    float error = target_I - amps;
    if (error < 0 ) error *= -1;
    if (error > 0.5) {
      Serial.print("DO SOMETHING");
    }
    short_time = millis();
  }
  // Non-blocking step toward slower pump speed.
  if (pwmCmd < 190 && (millis() - last_step_ms >= 3000)) {
    pwmCmd += 30;
    pwmCmd = constrain(pwmCmd, PWM_MAX, PWM_MIN);
    analogWrite(PWM_PIN, pwmCmd);
    last_step_ms = millis();
  }  
}

static bool enable = false;
inline void test() {
  float ref = 2.58f;
  
  if (!enable) {
    pinMode(28, OUTPUT);
    analogWriteResolution(8);
    analogWriteFrequency(28, 1000);
    analogWrite(28, 0);
    enable = true;
  }

  int rawADC = analogRead(CURRENT_SENSOR_PIN);
  static float adc_scale = (float)rawADC / 1023;
  
  float resting = 0.555;
  float amps = (ref - (ref * adc_scale)) - 0.0185;
  float diff = amps - resting;
  bool pwm = false;

  // if (diff > 0.32 && !pwm) {
  //   analogWrite(28, 255);
  //   pwm = true;
  //   delay(5000);
  // }

  // else if (pwm && diff > 0.32) {
  //   analogWrite(28, 20);
  //   pwm = false;
  // }

  int pulse = 0;
  analogWrite(28, 255);
  delay(500);
  analogWrite(28, 50);
  Serial.println(diff, 5);
  displayWrite("t4", "Amps", amps, "A");
}
// static bool tempSensorRdy = false;
// static uint32_t maxRawData        = 0;
// static float    thermocoupleTempC = 0.0f;
// static float    internalTempC     = 0.0f;
// static bool     faultOpenCircuit  = false;
// static bool     faultShortGND     = false;
// static bool     faultShortVCC     = false;
// static bool     thermocoupleFault = false;
static bool     thermocoupleReady = false;
// static uint32_t lastReadMillis    = 0;

#define SCLK 13
#define MISO 12
#define MAX31855_CS_PIN 10

Adafruit_MAX31855 tc(SCLK, MAX31855_CS_PIN, MISO);
static void temperatureController() {
  displayWrite("t4", "Hello", 0, "X");
  double tempC = tc.readCelsius();
  
  if (isnan(tempC)) {
    displayWrite("t2", "temp", tempC, "X");
  } else {
    double tempF = tempC * 9.0 / 5.0 + 32.0;
    int tempF_int = (int)(tempF + 0.5);
    displayWrite("t2", "Temperature", tempC, "C");
    Serial.println(tempC);
    //sendToNextion("n4", tempF_int);
  }
  
  // // Send to Nextion n3 (integer amps)
  // int ampsInt = (int)(ampsFiltered + 0.5f);
  // sendToNextion("n3", ampsInt);
}


void setup() {
  Serial.begin(NEXTION_BAUD);
  nextion.begin(NEXTION_BAUD);

  can0.begin();
  can0.setBaudRate(CAN_BAUD_RATE);
  can0.setMaxMB(16);
  can0.enableFIFO();
  can0.enableFIFOInterrupt();
  can0.onReceive(onCanFrame);
  can0.mailboxStatus();
  displayWrite("t4", "Offline", 0, "X");
}

void loop() {
  can0.events();
  test();
 
}
