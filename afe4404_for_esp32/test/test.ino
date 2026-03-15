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
#define AFE4404_I2C_ADDR 0x58   // 7-bit default address

volatile bool newDataAvailable = false;

AFE A;

// ── Register address table (matches STM32 ADDR_ar[42]) ───────────────────────
const uint8_t ADDR_ar[42] = {
  0x00, 0x23, 0x1E, 0x39, 0x1D, 0x09, 0x0A, 0x01, 0x02, 0x15, 0x16,
  0x0D, 0x0E, 0x36, 0x37, 0x05, 0x06, 0x17, 0x18, 0x0F, 0x10, 0x03,
  0x04, 0x07, 0x08, 0x19, 0x1A, 0x11, 0x12, 0x0B, 0x0C, 0x1B, 0x1C,
  0x13, 0x14, 0x32, 0x33, 0x22, 0x21, 0x20, 0x3D, 0x3A
};
volatile uint32_t Registers[42];

volatile bool     setup_flag        = false;
volatile bool     update_flag       = false;
volatile bool     setup_data_ready  = false;
volatile bool     update_data_ready = false;
volatile uint8_t  rx_step           = 0;
volatile uint8_t  rx_N              = 0;
volatile uint32_t rx_raw            = 0;
volatile uint8_t  UPDaddr           = 0;
volatile uint32_t UPDdata           = 0;

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

// ── Direct I2C register write (bypasses library for arbitrary registers) ──────
void AFE_WriteReg(uint8_t reg, uint32_t value) {
  Wire.beginTransmission(AFE4404_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)((value >> 16) & 0xFF));
  Wire.write((uint8_t)((value >>  8) & 0xFF));
  Wire.write((uint8_t)( value        & 0xFF));
  Wire.endTransmission();
}


void serialEvent() {
  // Pause ADC_RDY interrupt while processing serial commands
  detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
  Serial.write("serealEvent\n");
  while (Serial.available()) {
    Serial.write("sereal avalable\n");
    uint8_t b = (uint8_t)Serial.read();

    Serial.write(b);

    // ── Ping: echo back 0x88 (independent of state machine) ──────────────
    if (b == 0x88) {
      Serial.write(0x88);
    }

    // ── Protocol state machine ─────────────────────────────────────────────
    if (!setup_flag && b == 0x40 && !update_flag) {
      // Start full setup sequence
      setup_flag = true;
      rx_raw  = 0;
      rx_step = 0;
      rx_N    = 0;
    }
    else if (setup_flag) {
      // Accumulate 3 bytes per register, LSB first
      rx_raw |= (uint32_t)b << (rx_step * 8);
      rx_step++;
      if (rx_step == 3) {
        Registers[rx_N] = rx_raw;
        rx_raw  = 0;
        rx_step = 0;
        rx_N++;
        if (rx_N == 41) {
          // Full setup received — main loop will write all registers
          rx_N             = 0;
          setup_flag       = false;
          setup_data_ready = true;
          // ADC_RDY stays disabled; main loop re-enables after writing
          continue;
        }
      }
    }
    else if (!update_flag && b == 0x44 && !setup_flag) {
      // Start partial update sequence
      update_flag = true;
      rx_raw  = 0;
      rx_step = 0;
      UPDaddr = 0;
    }
    else if (update_flag) {
      if (rx_step == 0) {
        // First byte is register address
        UPDaddr = b;
        rx_step++;
      } else {
        // Next 3 bytes are register value, LSB first
        rx_raw |= (uint32_t)b << ((rx_step - 1) * 8);
        rx_step++;
        if (rx_step == 4) {
          UPDdata           = rx_raw;
          update_data_ready = true;
          update_flag       = false;
          rx_step           = 0;
          rx_raw            = 0;
          // ADC_RDY stays disabled; main loop re-enables after writing
          continue;
        }
      }
    }
    else if (!setup_flag && !update_flag && b == 0x48) {
      // Software reset AFE4404
      AFE_WriteReg(0x00, 0x000008); // CONTROL0: SW_RESET bit
    }
  }

  // Re-enable ADC_RDY only when no pending register writes remain
  if (!setup_data_ready && !update_data_ready) {
    attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
  }
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
    newDataAvailable = false;
    detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
    
    // Читаем данные из регистров AFE4404 (0x2Ah-0x2Fh)
    //readAFE4404Registers();
    
    // Обрабатываем полученные данные
    //processData();
    /*Serial.write('$');
    byteDivision(123);
    byteDivision(345);
    byteDivision(678);
    byteDivision(0);
    Serial.write(';');
    delay(100);*/

    serialEvent();
    
    // Снова включаем прерывания
    attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
    
    
  } else if (setup_data_ready) {
    Serial.write("setup\n");
    detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
    for (int i = 0; i < 41; i++) {
      AFE_WriteReg(ADDR_ar[i], Registers[i]);
    }
    setup_data_ready = false;
    attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
  } else if (update_data_ready) {
    Serial.write("update\n");
    detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
    AFE_WriteReg(UPDaddr, UPDdata);
    update_data_ready = false;
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
