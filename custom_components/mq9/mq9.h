#include "esphome.h"
#include <MQUnifiedsensor.h>

/***********************Software Related Macros************************************/
#define Type ("MQ-9")    // MQ9
#define Voltage_Resolution (5)
#define ADC_Bit_Resolution_ESP8266 (10)
#define ADC_Bit_Resolution_ESP32 (12)
#define RatioMQ9CleanAir (9.6)    // RS / R0 = 60 ppm
/*****************************Globals***********************************************/

static const char *TAG = "MQ9Sensor";

class MQ9Sensor : public PollingComponent, public Sensor {
   public:
    MQUnifiedsensor mq9;
    Sensor *lpg_sensor = new Sensor();
    Sensor *ch4_sensor = new Sensor();
    Sensor *co_sensor = new Sensor();

    MQ9Sensor(String board, int pin, uint32_t update_interval) : PollingComponent(update_interval) {
        board.toUpperCase();
        bool isESP8266 = (board == "ESP8266") ? true : false;

        mq9 = MQUnifiedsensor(isESP8266 ? "ESP8266" : "ESP-32", Voltage_Resolution,
                              isESP8266 ? ADC_Bit_Resolution_ESP8266 : ADC_Bit_Resolution_ESP32, pin, Type);
    }

    float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

    void setup() override {
        mq9.setRegressionMethod(1);
        mq9.init();

        ESP_LOGD("Calibrating please wait.");
        float calcR0 = 0;
        for (int i = 1; i <= 100; i++) {
            mq9.update();
            calcR0 += mq9.calibrate(RatioMQ9CleanAir);
        }
        mq9.setR0(calcR0 / 100);
        ESP_LOGD(TAG, "R0 = %f", calcR0);

        if (isinf(calcR0)) {
            ESP_LOGW(TAG, "Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your "
                          "wiring and supply");
        }
        if (calcR0 == 0) {
            ESP_LOGW(TAG,
                     "Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please "
                     "check your wiring and supply");
        }
    }

    void update() override {
        mq9.update();

        /*
        Exponential regression:
        GAS     | a      | b
        LPG     | 1000.5 | -2.186
        CH4     | 4269.6 | -2.648
        CO      | 599.65 | -2.244
        */

        // LPG
        mq9.setA(1000.5);
        mq9.setB(-2.186);
        float LPG = mq9.readSensor();

        // CH4
        mq9.setA(4269.6);
        mq9.setB(-2.648);
        float CH4 = mq9.readSensor();

        // C0
        mq9.setA(599.65);
        mq9.setB(-2.244);
        float CO = mq9.readSensor();

        ESP_LOGD(TAG, "| LPG: %.2f | CH4: %.2f | CO: %.2f |", LPG, CH4, CO);

        lpg_sensor->publish_state(LPG);
        ch4_sensor->publish_state(CH4);
        co_sensor->publish_state(CO);
    }
};