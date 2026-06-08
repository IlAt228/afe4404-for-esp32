#include <Wire.h>
#include <AFE_connect.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define ADC_RDY_PIN 2
#define AFE4404_I2C_ADDR 0x58

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // app → esp32
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // esp32 → app

volatile bool newDataAvailable = false;
bool deviceConnected = false;

AFE A;
BLECharacteristic *pTxChar = nullptr;

// ── Register address table ────────────────────────────────────────────────────
const uint8_t ADDR_ar[42] = {
    0x00, 0x23, 0x1E, 0x39, 0x1D, 0x09, 0x0A, 0x01, 0x02, 0x15, 0x16,
    0x0D, 0x0E, 0x36, 0x37, 0x05, 0x06, 0x17, 0x18, 0x0F, 0x10, 0x03,
    0x04, 0x07, 0x08, 0x19, 0x1A, 0x11, 0x12, 0x0B, 0x0C, 0x1B, 0x1C,
    0x13, 0x14, 0x32, 0x33, 0x22, 0x21, 0x20, 0x3D, 0x3A};
volatile uint32_t Registers[42];

volatile bool setup_flag = false;
volatile bool update_flag = false;
volatile bool setup_data_ready = false;
volatile bool update_data_ready = false;
volatile uint8_t rx_step = 0;
volatile uint8_t rx_N = 0;
volatile uint32_t rx_raw = 0;
volatile uint8_t UPDaddr = 0;
volatile uint32_t UPDdata = 0;

// ── BLE helpers ───────────────────────────────────────────────────────────────
static uint8_t bleBuf[20];
static uint8_t bleLen = 0;

void blePush(uint8_t b)
{
  if (bleLen < sizeof(bleBuf))
    bleBuf[bleLen++] = b;
}

void bleFlush()
{
  if (deviceConnected && pTxChar && bleLen > 0)
  {
    pTxChar->setValue(bleBuf, bleLen);
    pTxChar->notify();
  }
  bleLen = 0;
}

void byteDivision(int x)
{
  blePush((x >> 16) & 0xFF);
  blePush((x >> 8) & 0xFF);
  blePush(x & 0xFF);
}

// ── AFE4404 direct register read ──────────────────────────────────────────────
int32_t AFE_ReadReg(uint8_t reg)
{
  Wire.beginTransmission(AFE4404_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((int)AFE4404_I2C_ADDR, 3);
  int32_t val = 0;
  if (Wire.available() >= 3) {
    val = (int32_t)Wire.read() << 16;
    val |= (int32_t)Wire.read() << 8;
    val |= Wire.read();
    // sign extension for 22-bit signed (ADC result registers 0x2A-0x2F)
    if (val & 0x00200000) {
      val &= 0x003FFFFF;
      val ^= (int32_t)0xFFC00000;
    }
  }
  return val;
}

// ── AFE4404 direct register write ─────────────────────────────────────────────
void AFE_WriteReg(uint8_t reg, uint32_t value)
{
  Wire.beginTransmission(AFE4404_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)((value >> 16) & 0xFF));
  Wire.write((uint8_t)((value >> 8) & 0xFF));
  Wire.write((uint8_t)(value & 0xFF));
  Wire.endTransmission();
}

// ── Protocol state machine ────────────────────────────────────────────────────
void processIncomingByte(uint8_t b)
{
  if (b == 0x88)
  {
    uint8_t echo = 0x88;
    if (deviceConnected && pTxChar)
    {
      pTxChar->setValue(&echo, 1);
      pTxChar->notify();
    }
  }

  if (!setup_flag && b == 0x40 && !update_flag)
  {
    setup_flag = true;
    rx_raw = 0;
    rx_step = 0;
    rx_N = 0;
  }
  else if (setup_flag)
  {
    rx_raw |= (uint32_t)b << (rx_step * 8);
    rx_step++;
    if (rx_step == 3)
    {
      Registers[rx_N] = rx_raw;
      rx_raw = 0;
      rx_step = 0;
      rx_N++;
      if (rx_N == 41)
      {
        rx_N = 0;
        setup_flag = false;
        setup_data_ready = true;
      }
    }
  }
  else if (!update_flag && b == 0x44 && !setup_flag)
  {
    update_flag = true;
    rx_raw = 0;
    rx_step = 0;
    UPDaddr = 0;
  }
  else if (update_flag)
  {
    if (rx_step == 0)
    {
      UPDaddr = b;
      rx_step++;
    }
    else
    {
      rx_raw |= (uint32_t)b << ((rx_step - 1) * 8);
      rx_step++;
      if (rx_step == 4)
      {
        UPDdata = rx_raw;
        update_data_ready = true;
        update_flag = false;
        rx_step = 0;
        rx_raw = 0;
      }
    }
  }
  else if (!setup_flag && !update_flag && b == 0x48)
  {
    AFE_WriteReg(0x00, 0x000008);
  }
}

// ── BLE callbacks ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *) override
  {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer *pServer) override
  {
    deviceConnected = false;
    pServer->startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *c) override
  {
    uint8_t *d = c->getData();
    size_t n = c->getLength();
    if (n == 0)
      return;

    if (n == 1)
    {
      // single-byte: 0x48 (hard reset), 0x88 (ping)
      processIncomingByte(d[0]);
    }
    else if (n >= 5 && d[0] == 0x44)
    {
      // send_from_main: [0x44, addr, v0, v1, v2]
      UPDaddr = d[1];
      UPDdata = (uint32_t)d[2] | ((uint32_t)d[3] << 8) | ((uint32_t)d[4] << 16);
      update_data_ready = true;
    }
    else if (n == 4)
    {
      // sendCmd: [addr, v0, v1, v2]
      UPDaddr = d[0];
      UPDdata = (uint32_t)d[1] | ((uint32_t)d[2] << 8) | ((uint32_t)d[3] << 16);
      update_data_ready = true;
    }
    else
    {
      // batch protocol (0x40 prefix)
      for (size_t i = 0; i < n; i++)
        processIncomingByte(d[i]);
    }
  }
};

// ── ISR ───────────────────────────────────────────────────────────────────────
void IRAM_ATTR dataReadyISR()
{
  newDataAvailable = true;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);

  BLEDevice::init("AFE4404");
  Serial.print("BLE MAC: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pSvc = pServer->createService(NUS_SERVICE_UUID);

  pTxChar = pSvc->createCharacteristic(NUS_TX_CHAR_UUID,
                                       BLECharacteristic::PROPERTY_NOTIFY);
  pTxChar->addDescriptor(new BLE2902());

  BLECharacteristic *pRxChar = pSvc->createCharacteristic(
      NUS_RX_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxChar->setCallbacks(new RxCallbacks());

  pSvc->start();

  BLEAdvertising *pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(NUS_SERVICE_UUID);
  pAdv->setScanResponse(true);
  BLEDevice::startAdvertising();

  A.init();
  pinMode(ADC_RDY_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop()
{
  if (setup_data_ready)
  {
    detachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN));
    for (int i = 0; i < 41; i++)
      AFE_WriteReg(ADDR_ar[i], Registers[i]);
    setup_data_ready = false;
    attachInterrupt(digitalPinToInterrupt(ADC_RDY_PIN), dataReadyISR, FALLING);
  }
  else if (update_data_ready)
  {
    AFE_WriteReg(UPDaddr, UPDdata);
    update_data_ready = false;
  }
  else if (deviceConnected)
  {
    newDataAvailable = false;
    bleLen = 0;
    blePush('$');
    byteDivision(A.get_led1_val());
    byteDivision(A.get_led2_val());
    byteDivision(A.get_led3_val());
    byteDivision(AFE_ReadReg(0x2D));
    blePush(';');
    bleFlush();
    delay(7);
  }
  else
  {
    newDataAvailable = false;
  }
}
