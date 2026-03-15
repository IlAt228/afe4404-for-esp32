/*
 * AFE4404 Basic Read library example.
 * https://github.com/rakshithbk/AFE4404-Library
 * 
 * On Arduino Uno -  | AFE4404 | Uno Pin
 *                   | I2C_Dat | A4
 *                   | I2C_Clk | A5
 * get_led_val() will return uint32_t values.
 * 
 * Advanced - you can modify the sample period and other
 * parameters in the library files (refer AFE4404 datasheet)
 */


#include <Wire.h>
#include <AFE_connect.h>

#define ADC_RDY_PIN 2

volatile bool newDataAvailable = false;

AFE A;

void byteDivision(int x) {
  if(newDataAvailable){
    Serial.write(0);
    Serial.write(0);
    Serial.write(0);
    return;
  }
  int a = x & 255;
  int b = (x >> 8) & 255;
  int c = (x >> 16) & 255;
  Serial.write(c);
  Serial.write(b);
  Serial.write(a);
}

// Обработчик прерываний (должен быть максимально быстрым)
void IRAM_ATTR dataReadyISR() {
  newDataAvailable = true; // Просто устанавливаем флаг
}

void setup() {
  Serial.begin(11520);
  Serial.println("AFE4404 basic readings -\n");
  A.init();
  pinMode(ADC_RDY_PIN, INPUT_PULLUP);
    // 3. Настроить прерывание по спадающему фронту (LOW)
  attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
}

void loop() {
  if (newDataAvailable) {
    // Отключаем прерывания на время чтения данных
    detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
    
    // Читаем данные из регистров AFE4404 (0x2Ah-0x2Fh)
    readAFE4404Registers();
    
    // Обрабатываем полученные данные
    processData();
    
    // Снова включаем прерывания
    attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
    
    newDataAvailable = false;
  } else {
    Serial.write('$');
    byteDivision(A.get_led1_val());
    byteDivision(A.get_led2_val());
    byteDivision(A.get_led3_val());
    byteDivision(0);
    Serial.write(';');
    delay(100);
  }
}
