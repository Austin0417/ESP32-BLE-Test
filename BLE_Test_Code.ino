#include "BluetoothSerial.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MINUTES_TO_ADVERTISE 1
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Flags for current advertising status
volatile bool shouldStartAdvertising = false;
volatile bool isCurrentlyAdvertising = false;

// Pin number constants
constexpr int PIN_TOUCH_START_ADVERTISE = 4;
//constexpr int PIN_TOUCH_STOP_ADVERTISE = 12;
constexpr int ONBOARD_LED = 2;
constexpr TickType_t TASK_DELAY = 50;

// Global handles for BLE
BLEServer* mServer;
BLEService* mService;
BLECharacteristic* mCharacteristic;
BLEAdvertising* advertiser;

// Data packet for the characteristic value
StaticJsonDocument<300> jsonData;

// BLEServerCallback Implementation
class MyBLEServerCallbacks : public BLEServerCallbacks {
  public:
  void onConnect(BLEServer* server) override;
  void onDisconnect(BLEServer* server) override;
};

void MyBLEServerCallbacks::onConnect(BLEServer* server) {
    Serial.println("Client has connected");
}

void MyBLEServerCallbacks::onDisconnect(BLEServer* server) {
    Serial.println("Client has disconnected");
}

bool verifyBluetoothSetup() {
    return mServer != nullptr && mService != nullptr && mCharacteristic != nullptr && advertiser != nullptr;
}

void BLEAdvertiserThreadWork(void* parameters) {
    while (1) {
        if (shouldStartAdvertising && !isCurrentlyAdvertising) {
            Serial.println("Beginning BLE advertise...");
            advertiser->start();
            isCurrentlyAdvertising = true;
            delay(MINUTES_TO_ADVERTISE * 60000);
            Serial.println("Stopping BLE advertise...");
            advertiser->stop();
            isCurrentlyAdvertising = false;
            shouldStartAdvertising = false;
        }
        vTaskDelay(TASK_DELAY);
    }
}

// ISR Routine that will be called when Pin 4 is touched
void ISRSetAdvertising() {
    shouldStartAdvertising = true;
}

void initializeJSONData() {
  jsonData["motion"] = true;
  jsonData["proximity"] = false;
  jsonData["light"] = true;
  jsonData["occupied"] = true;

  // Numerical Data
  jsonData["lightIntensity"] = 323.4f;
  jsonData["distance"] = 130;
  jsonData["vibrationIntensity"] = 9.81f;
  jsonData["vibrationBaseline"] = 9.81f;
  jsonData["proximityBaseline"] = 131;
  jsonData["lightBaseline"] = 320.0f;
  jsonData["lightOffBaseline"] = 5.0f;

  jsonData["connected_devices"] = 1;
  jsonData["lightingStatus"] = "Lights On";
  jsonData["movement_status"] = "Person is present";
  jsonData["lastDetected"] = 2;
}

void updateCharacteristicValue() {
  String serializedJSON;
  serializeJson(jsonData, serializedJSON);
  uint8_t data[serializedJSON.length() + 1];
  memcpy(data, serializedJSON.c_str(), serializedJSON.length());
  mCharacteristic->setValue(data, serializedJSON.length());
}

void setup() {
  Serial.begin(115200);

  BLEDevice::init("ESP32");
  mServer = BLEDevice::createServer();
  mServer->setCallbacks(new MyBLEServerCallbacks());
  mService = mServer->createService(SERVICE_UUID);

  mCharacteristic = mService->createCharacteristic(BLEUUID(CHARACTERISTIC_UUID), BLECharacteristic::PROPERTY_READ
    | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY 
    | BLECharacteristic::PROPERTY_INDICATE);
  mCharacteristic->setValue("Sample Test Value");

  mService->start();
  advertiser = BLEDevice::getAdvertising();
  advertiser->addServiceUUID(SERVICE_UUID);

  // Bluetooth parameters initialized, dispatch the BLE thread and attach the interrupts
  if (verifyBluetoothSetup()) {
      Serial.println("Bluetooth peripherals were correctly setup");
      xTaskCreate(BLEAdvertiserThreadWork, "AdvertiserThread", 10000, NULL, 2, NULL);
      touchAttachInterrupt(PIN_TOUCH_START_ADVERTISE, ISRSetAdvertising, 40);
      initializeJSONData();
      updateCharacteristicValue();
  }
  else {
      Serial.println("Bluetooth failed to initialize properly");
  }
}

void loop() {
  digitalWrite(ONBOARD_LED, HIGH);
  delay(1000);
  digitalWrite(ONBOARD_LED, LOW);
  delay(1000);
}
