#include <Adafruit_MAX31855.h>
#include <FlexCAN_T4.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include "config.h"

namespace velocity {
namespace {

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can0;

SoftwareSerial nextion(16, 17);

using State = void (*)();
State state = nullptr;

struct ids {
  static constexpr uint32_t pack_data = 0x00A0;
  static constexpr uint32_t pack_status = 0x00A1;
  static constexpr uint32_t vsm_state = 0x00A2;
  static constexpr uint32_t fault_flags = 0x00A4;

  static constexpr uint32_t STATUS_VOLT_TOO_HIGH_MASK = 0x0020u;
  static constexpr uint32_t STATUS_REDUN_SUPPLY_MASK = 0x1000u;
};

struct DTCError {
  DTC dtc;
  bool ative;
  void (*check)();
};

inline void displayWrite(const char* component, const char* label, int value, const char* unit) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s.txt=\"%s %d %s\"", component, label, value, unit);
  nextion.print(buf);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
}

struct PumpModule {
  volatile float ref = 2.58F;
  volatile float bias = 0.555F;
  volatile int rawADC = 0;

  volatile float adc_scale{};
  volatile uint8_t current{};

  volatile uint8_t biasCurrent{};
  volatile uint8_t temperature{};
  volatile uint8_t pwmCmd{};
  volatile float target_I{};
  volatile float error{};

  volatile bool ampSensReady = false;
  volatile bool tempSensReady = false;

  volatile unsigned long five_seconds = 0;
  volatile unsigned long short_time = 0;
  volatile unsigned long last_step_ms = 0;
} __attribute__((aligned(64)));

struct temperatureModule {
  volatile bool thermocoupleReady = false;
  double tempC{};
  int tempF_int{};
  double tempF{};
} __attribute__((aligned(32)));

struct PackModule {
  volatile uint8_t voltage;
  volatile uint8_t avgTemp;
  volatile uint8_t soc;
  volatile uint8_t maxTemp;
  volatile uint8_t minTemp;
} __attribute__((aligned(8)));

void handle(DTC dtc) {
  // Handle the error condition here
  // For example, you can log the error, set a flag, or take corrective action
  Serial.println("Handling DTC error...");
}

struct VCUModule {
  volatile uint8_t vcuState;
  volatile uint8_t vcuFault;
  volatile uint8_t vcuError;
  volatile uint8_t vcuWarning;

  volatile uint8_t canbusOperational;
  struct DTCError dtc;
  struct DTCError dtcErrors[DTC_COUNT] = {{STATUS_VOLT_TOO_HIGH_MASK, false, handle},
                                          {STATUS_REDUN_SUPPLY_MASK, false, handle},
                                          {VSM_STATE_MASK, false, nullptr},
                                          {VSM_STATE_DRIVE_ENABLED, false, nullptr},
                                          {VSM_STATE_FAULT, false, nullptr},
                                          {FAULT_FLAGS_BIT0, false, nullptr}};

  struct PackModule* pack;
  struct PumpModule* pump;
  struct temperatureModule* temp;
};

struct VCUModule vcu_ptr;
struct VCUModule* vcu = &vcu_ptr;

void systemInit() {
  struct PackModule pack_data_ptr;
  struct PumpModule pump_data_ptr;
  struct temperatureModule temp_data_ptr;

  vcu->pack = &pack_data_ptr;
  vcu->pump = &pump_data_ptr;
  vcu->temp = &temp_data_ptr;

  vcu->pump->current = 0;
  vcu->pump->temperature = 0;

  pinMode(PWM_PIN, OUTPUT);
  pinMode(FAULT_LED, OUTPUT);
  pinMode(PWR_LED, OUTPUT);
  analogWriteResolution(8);
  analogWriteFrequency(PWM_PIN, 1000);
  analogWrite(PWM_PIN, 0);

  vcu->pump->ampSensReady = true;
}

// void temperatureController() {
//   displayWrite("t4", "Hello", 0, "X");
//   double tempC = tc.readCelsius();
//   if (isnan(tempC)) {
//     displayWrite("t2", "temp", tempC, "X");
//   } else {
//     double tempF = tempC * 9.0 / 5.0 + 32.0;
//     int tempF_int = (int)(tempF + 0.5);
//     displayWrite("t2", "Temperature", tempC, "C");
//     Serial.println(tempC);
//     //sendToNextion("n4", tempF_int);
//   }
// }

void pumpModuleUpdate() {
  if (!vcu->pump->ampSensReady) {
    return;
  }

  vcu->pump->rawADC = analogRead(CURRENT_SENSOR_PIN);
  vcu->pump->adc_scale = (float)vcu->pump->rawADC / 1023;
  vcu->pump->current = (vcu->pump->ref - (vcu->pump->ref * vcu->pump->adc_scale)) - 0.0185;
  vcu->pump->biasCurrent = vcu->pump->current - vcu->pump->bias;
  vcu->pump->five_seconds = millis();
  vcu->pump->short_time = millis();
  vcu->pump->last_step_ms = 0;

  if (millis() - vcu->pump->five_seconds >= 5000) {
    vcu->pump->target_I = vcu->pump->current;
    vcu->pump->five_seconds = millis();
  }
  if (millis() - vcu->pump->short_time >= 50) {
    vcu->pump->error = vcu->pump->target_I - vcu->pump->current;
    if (vcu->pump->error < 0) {
      vcu->pump->error *= -1;
    }
    if (vcu->pump->error > 0.5) {
      Serial.print("DO SOMETHING");
    }
    vcu->pump->short_time = millis();
  }
  if (vcu->pump->pwmCmd < 190 && (millis() - vcu->pump->last_step_ms >= 3000)) {
    vcu->pump->pwmCmd += 30;
    vcu->pump->pwmCmd = constrain(vcu->pump->pwmCmd, PWM_MAX, PWM_MIN);
    analogWrite(PWM_PIN, vcu->pump->pwmCmd);
    vcu->pump->last_step_ms = millis();
  }
}

inline void onCanFrame(const CAN_message_t& msg) {
  switch (msg.id) {
    case MSG_ID_PACK_DATA: {
      vcu->pack->voltage = msg.buf[1] / 10;
      vcu->pack->avgTemp = msg.buf[0];
      vcu->pack->soc = msg.buf[2] / 2;
      vcu->pack->maxTemp = msg.buf[3];
      vcu->pack->minTemp = msg.buf[4];
      state = pumpModuleUpdate;

      break;
    }

    case 0x0A3: {
      uint32_t raw = msg.buf[0];
      uint32_t raw2 = msg.buf[1];
      Serial.println(raw);
      Serial.println(raw2);

      break;
    }

    case MSG_ID_PACK_STATUS: {
      uint8_t byte0 = msg.buf[0];
      uint8_t byte1 = msg.buf[1];

      if (byte0 & STATUS_VOLT_TOO_HIGH_MASK) {
        displayWrite("t4", "VOLT TOO HIGH", 0, "X");
      }
      if (byte1 & STATUS_REDUN_SUPPLY_MASK) {
        displayWrite("t4", "REDUN SUPPLY", 0, "X");
      }

      break;
    }

    default: {
      displayWrite("t0", "CAN_DC", 1, "C");
      displayWrite("t3", "X", 1, "V");
      displayWrite("t2", "X", 1, "%");
      displayWrite("t10", "X ", 1, "C");
      displayWrite("t8", "X ", 1, "C");

      break;
    }
  }
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
}

}  // namespace
}  // namespace velocity

void setup() {
  velocity::setup();
}

void loop() {
  velocity::loop();
}
