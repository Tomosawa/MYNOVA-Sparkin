/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */
#include "BatteryManager.h"
#include "IOPin.h"
#include "Common.h"
#include "Fingerprint.h"
#include "BluetoothManager.h"

extern Fingerprint fingerprint;
extern BluetoothManager bluetoothManager;
/* VBAT */
const float battery_min = 3.0; // (V) minimum voltage of battery before shutdown
const float battery_max = 4.2; // (V) maximum voltage of battery

const float resistanceHigh = 51000.0; // (Ohm) R1
const float resistanceLow = 51000.0; // (Ohm) R2
const float offsetVoltage = 28; // (mV) offset voltage for ADC reading

#define WAIT_FOR_CHARGE_TIME    50 // (ms) time for waiting for capacitor charge
#define CONVERSIONS_PER_PIN     10

uint8_t adc_pins[] = {PIN_BATTERY_ADC};  //some of ADC1 pins for ESP32
// Calculate how many pins are declared in the array - needed as input for the setup function of ADC Continuous
uint8_t adc_pins_count = sizeof(adc_pins) / sizeof(uint8_t);

// Flag which will be set in ISR when conversion is done
volatile bool adc_coversion_done = false;

// Result structure for ADC Continuous reading
adc_continuous_data_t *result = NULL;

// ISR Function that will be triggered when ADC conversion is done
void ARDUINO_ISR_ATTR adcComplete() {
  adc_coversion_done = true;
}

BatteryManager::BatteryManager(int adcPin, int testPin) 
  : adcPin(adcPin), testPin(testPin)
{
   
}

BatteryManager::~BatteryManager() {
}

void BatteryManager::init() {
    pinMode(adcPin, INPUT);
    pinMode(testPin, OUTPUT);
    analogContinuous(adc_pins, adc_pins_count, CONVERSIONS_PER_PIN, 20000, &adcComplete);
}

uint32_t BatteryManager::readADC()
{
    // 让电池测量有效
    digitalWrite(PIN_BATTERY_TEST, HIGH);
    delay(WAIT_FOR_CHARGE_TIME); // 等待电容充电

    uint32_t voltage = 0;
    
    adc_coversion_done = false;
    analogContinuousStart();

    while (adc_coversion_done == false)
    {
        yield();
    }

   if (adc_coversion_done == true)
   {
        // Read data from ADC
        if (analogContinuousRead(&result, 0)) {

        // Optional: Stop ADC Continuous conversions to have more time to process (print) the data
        analogContinuousStop();

        for (int i = 0; i < adc_pins_count; i++) {
            Serial.printf("\nADC PIN %d data:", result[i].pin);
            Serial.printf("\nAvg raw value = %d ", result[i].avg_read_raw);
            Serial.printf("\nAvg millivolts value = %d \n", result[i].avg_read_mvolts);
        }

        voltage = result[0].avg_read_mvolts;

        } else {
            Serial.println("Error occurred during reading data. Set Core Debug Level to error or lower for more information.");
        }
    }

    // 关闭测量引脚
    digitalWrite(PIN_BATTERY_TEST, LOW);
    return voltage; //单位(mV)
}

float BatteryManager::readVoltage() {
    /*通过R1&R2推算电池电压*/
    float battery_voltage = (resistanceHigh + resistanceLow) / resistanceLow * readADC() + offsetVoltage;
    Serial.printf("Battery voltage: %f mV\n", battery_voltage);

    return battery_voltage / 1000.0;
}

float BatteryManager::calculateBatteryPercent(float voltage) {
    float percent = (voltage - battery_min) / (battery_max - battery_min) * 100.0;
    if(percent < 0)
        percent = 0.0;
    if(percent > 100)
        percent = 100.0;
    return percent;
}

void BatteryManager::CheckBatteryLow(){
    float batteryVoltage = readVoltage();
    batteryPercentage = calculateBatteryPercent(batteryVoltage);
    bluetoothManager.setBatteryLevel((uint8_t)batteryPercentage);
    Serial.printf("Battery Voltage: %.2f V, Percentage: %.2f%%\n", batteryVoltage, batteryPercentage);
    // 如果电池电量低于阈值，发出警告
    if (batteryPercentage < 20.0) {
      fingerprint.setLEDCmd(Fingerprint::LED_CODE_BLINK,0x04,0x11,0x00,12);  // 红色闪烁灯
      Serial.println(">>>Warning<<<: Battery level is low! Please charge the device.");
    }
  }