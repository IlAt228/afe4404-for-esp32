void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Chip Info ===");
  Serial.print("Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Cores: ");
  Serial.println(ESP.getChipCores());

  Serial.println();
  Serial.println("=== Bluetooth (compile-time check) ===");

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
  Serial.println("Bluetooth Classic (BR/EDR / SPP): YES - supported");
  Serial.println(">>> BluetoothSerial доступен, виртуальный COM порт будет работать.");
#else
  Serial.println("Bluetooth Classic (BR/EDR / SPP): NO  - not supported");
#endif

#if defined(CONFIG_BT_NIMBLE_ENABLED) || defined(CONFIG_BT_BLE_ENABLED) || defined(CONFIG_BLUEDROID_ENABLED)
  Serial.println("Bluetooth LE (BLE):               YES - supported");
#else
  Serial.println("Bluetooth LE (BLE):               NO  - not supported");
#endif

  Serial.println();

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  Serial.println("Target: ESP32-C3 — только BLE, Classic BT отсутствует.");
  Serial.println(">>> Для виртуального COM порта нужен HC-05 или смена чипа на ESP32.");
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.println("Target: ESP32-S3 — только BLE, Classic BT отсутствует.");
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  Serial.println("Target: ESP32-S2 — Bluetooth отсутствует полностью.");
#elif defined(CONFIG_IDF_TARGET_ESP32)
  Serial.println("Target: ESP32 — есть Classic BT + BLE.");
  Serial.println(">>> BluetoothSerial (SPP) доступен!");
#else
  Serial.println("Target: неизвестный чип.");
#endif
}

void loop() {
  
  Serial.println("=== Chip Info ===");
  Serial.print("Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Cores: ");
  Serial.println(ESP.getChipCores());

  Serial.println();
  Serial.println("=== Bluetooth (compile-time check) ===");

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
  Serial.println("Bluetooth Classic (BR/EDR / SPP): YES - supported");
  Serial.println(">>> BluetoothSerial доступен, виртуальный COM порт будет работать.");
#else
  Serial.println("Bluetooth Classic (BR/EDR / SPP): NO  - not supported");
#endif

#if defined(CONFIG_BT_NIMBLE_ENABLED) || defined(CONFIG_BT_BLE_ENABLED) || defined(CONFIG_BLUEDROID_ENABLED)
  Serial.println("Bluetooth LE (BLE):               YES - supported");
#else
  Serial.println("Bluetooth LE (BLE):               NO  - not supported");
#endif

  Serial.println();

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  Serial.println("Target: ESP32-C3 — только BLE, Classic BT отсутствует.");
  Serial.println(">>> Для виртуального COM порта нужен HC-05 или смена чипа на ESP32.");
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.println("Target: ESP32-S3 — только BLE, Classic BT отсутствует.");
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  Serial.println("Target: ESP32-S2 — Bluetooth отсутствует полностью.");
#elif defined(CONFIG_IDF_TARGET_ESP32)
  Serial.println("Target: ESP32 — есть Classic BT + BLE.");
  Serial.println(">>> BluetoothSerial (SPP) доступен!");
#else
  Serial.println("Target: неизвестный чип.");
#endif

}
