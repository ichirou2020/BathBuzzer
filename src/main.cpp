/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect hander associated with the server starts a background task that performs notification
   every couple of seconds.
*/

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define POLLING_DATA 	1 	//!<	ポーリングデータを送信
#define SWITCH_ON		2 	//!<	スイッチオン
#define SWITCH_PIN		18 	//!<	スイッチとして使用するGPIO まあ18番でいいのでは無いだろうか

BLEServer* pServer = NULL;					//!<	BLEサーバ
BLECharacteristic* pCharacteristic = NULL;	//!<	きゃらすきゃらす
bool deviceConnected = false;
bool oldDeviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#if 0

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#else

#define SERVICE_UUID        "d1583dcd-73b2-4455-8efa-3632ae8ecfcb"
#define CHARACTERISTIC_UUID "6538a81a-0601-4c85-a75d-812ce857b14b"

#endif

/**
 * @brief 接続状態検知
 * 
 */
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
	  Serial.printf("接続\n");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
	  Serial.printf("切断\n");
    }
};

/**
 * @brief 初期化処理色々
 * 
 */
void setup() {
	Serial.begin(115200);				//デバッグ用のシリアルよ～
	pinMode(SWITCH_PIN, INPUT_PULLUP);	//18番ピンを入力モード・プルアップ

	// Create the BLE Device
	BLEDevice::init("BathBuzzer");

	// Create the BLE Server
	pServer = BLEDevice::createServer();
	pServer->setCallbacks(new MyServerCallbacks());

	// Create the BLE Service
	BLEService *pService = pServer->createService(SERVICE_UUID);

	// Create a BLE Characteristic
	pCharacteristic = pService->createCharacteristic(
						CHARACTERISTIC_UUID,
						BLECharacteristic::PROPERTY_READ   |
						BLECharacteristic::PROPERTY_WRITE  |
						BLECharacteristic::PROPERTY_NOTIFY |
						BLECharacteristic::PROPERTY_INDICATE
					);

	// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
	// Create a BLE Descriptor
	pCharacteristic->addDescriptor(new BLE2902());

	// Start the service
	pService->start();

	// Start advertising
	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(SERVICE_UUID);
	pAdvertising->setScanResponse(false);
	pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
	BLEDevice::startAdvertising();
	Serial.println("Waiting a client connection to notify...");
}

/**
 * @brief ループ処理　OSからコールされるので細かいことは気にしない
 * 
 */
void loop() {
    if (deviceConnected) {
		int readRslt = digitalRead(SWITCH_PIN);	//ピンのデータ取得
		uint32_t value;

		if(readRslt == 1){
			value = POLLING_DATA;				//プルアップしているので、スイッチオフの時にHighが戻ってくる
		}else{
			value = SWITCH_ON;					//プルアップしているので、Closeになったら、スイッチオンと判定
		}
        pCharacteristic->setValue((uint8_t*)&value, 4);	//送信
        pCharacteristic->notify();						//変化通知
//        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
	// disconnecting
	if (!deviceConnected && oldDeviceConnected) {
		//再アドバタイジング処理
		//delay(500); // give the bluetooth stack the chance to get things ready
		pServer->startAdvertising(); // restart advertising
		Serial.println("start advertising");
		oldDeviceConnected = deviceConnected;
	}
	// connecting
	if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
		oldDeviceConnected = deviceConnected;
	}
	delay(1000);								//そんなにシビアにする必要は無いので1秒で充分
}