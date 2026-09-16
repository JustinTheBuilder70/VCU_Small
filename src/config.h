#include <cstdint>

constexpr uint32_t MSG_ID_PACK_DATA = 0x019u;
constexpr uint32_t MSG_ID_PACK_STATUS = 0x020u;
constexpr uint32_t MSG_ID_VSM_STATE = 0x0AAu;
constexpr uint32_t MSG_ID_FAULT_FLAGS = 0x024u;

constexpr uint32_t STATUS_VOLT_TOO_HIGH_MASK = 0x0020u;
constexpr uint32_t STATUS_REDUN_SUPPLY_MASK = 0x1000u;
constexpr uint32_t VSM_STATE_MASK = 0x0Fu;
constexpr uint32_t VSM_STATE_DRIVE_ENABLED = 5u;
constexpr uint32_t VSM_STATE_FAULT = 7u;
constexpr uint32_t FAULT_FLAGS_BIT0 = 0x01u;

constexpr uint32_t CAN_BAUD_RATE = 500000u;

constexpr uint32_t PWM_PIN = 28u;
constexpr uint32_t FAULT_LED = 6u;
constexpr uint32_t PWR_LED = 7u;
constexpr uint32_t PWM_MIN = 200;
constexpr uint32_t PWM_MAX = 20;
constexpr uint32_t LED_PIN = 13u;

constexpr int NEXTION_BAUD = 9600;

constexpr int CURRENT_SENSOR_PIN = 15;
constexpr float CURRENT_SENSITIVITY_MV_PER_A = 185.0f;
constexpr int CURRENT_CAL_SAMPLES = 100;
constexpr int CURRENT_CAL_DELAY_MS = 10;
constexpr float ADC_MAX_COUNTS = 1023.0f;
constexpr float ADC_REF_MILLIVOLTS = 5000.0f;

constexpr int MAX31855_READ_INTERVAL_MS = 250;  // conversion takes ~100-170ms, don't poll faster
constexpr int MAX31855_SPI_HZ = 4000000;