#define MSG_ID_PACK_DATA   0x019u
#define MSG_ID_PACK_STATUS 0x020u
#define MSG_ID_VSM_STATE   0x0AAu
#define MSG_ID_FAULT_FLAGS 0x024u

#define STATUS_VOLT_TOO_HIGH_MASK 0x0020u
#define STATUS_REDUN_SUPPLY_MASK  0x1000u
#define VSM_STATE_MASK            0x0Fu
#define VSM_STATE_DRIVE_ENABLED   5u
#define VSM_STATE_FAULT           7u
#define FAULT_FLAGS_BIT0          0x01u

#define CAN_BAUD_RATE 500000u

#define PWM_PIN   28u
#define FAULT_LED 6u
#define PWR_LED   7u
#define PWM_MIN   200
#define PWM_MAX   20
#define LED_PIN   13u

#define NEXTION_SERIAL Serial5
#define NEXTION_BAUD   9600

#define CURRENT_SENSOR_PIN           15
#define CURRENT_SENSITIVITY_MV_PER_A 185.0f
#define CURRENT_CAL_SAMPLES          100
#define CURRENT_CAL_DELAY_MS         10
#define ADC_MAX_COUNTS               1023.0f
#define ADC_REF_MILLIVOLTS           5000.0f

#define MAX31855_READ_INTERVAL_MS 250  // conversion takes ~100-170ms, don't poll faster
#define MAX31855_SPI_HZ           4000000