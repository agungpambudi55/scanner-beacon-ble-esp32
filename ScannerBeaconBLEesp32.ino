/*
 * Created on May 2019
 * Agung Pambudi <agung.pambudi5595@gmail.com>
 * 
 * Note : Blink 1x not connect wifi, blink off not connect to mqtt
*/

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//######################################################################################################## VARIABLE SETUP BLINKLED
#define LED_BUILTIN 2

//######################################################################################################## VARIABLE SETUP WIFI
const char* wifi_ssid = "<>";
const char* wifi_pass = "<>";

//######################################################################################################## VARIABLE SETUP MQTT
WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);

#define PUB_TOPIC "res/data"
#define SUB_TOPIC "req/scan"

const char* mqtt_server = "soldier.cloudmqtt.com";
const int mqtt_port = 15601;
const char* mqtt_user = "<>";
const char* mqtt_pass = "<>";

//######################################################################################################## VARIABLE SETUP BLE
BLEScan* pBLEScan;
int cb_index = 0;
int cb_sum = 5;
String cb_uuid[5];            //### Universally Unique Identifier
int cb_rssi[5];               //### Received Signal Strength Indicator
String scanner_ID = "ID01";   //### ScannerIndex
boolean scan_cmd_BLE = false;

//######################################################################################################## FUNCTION BLINKLED
void blinkLedOff(void){ digitalWrite(LED_BUILTIN, LOW); }

void blinkLed(int n, int t){
  for(int i=0; i<n; i++){
    digitalWrite(LED_BUILTIN, LOW);   delay(t);
    digitalWrite(LED_BUILTIN, HIGH);  delay(t);
  }
}

//######################################################################################################## FUNCTION WIFI CONNECT
void wifiConnect(){
  Serial.print("\nWiFi connecting to ");
  Serial.print(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    blinkLed(1, 50);
  }

  Serial.print("\nWiFi connected : ");
  Serial.println(WiFi.localIP());
}

//######################################################################################################## FUNCTION MQTT PUBLISHER
void mqtt_pub(String data_pub){
    char JSONmessageBuffer[100];
    StaticJsonBuffer<320> JSONbuffer;
    JsonObject& JSONencoder = JSONbuffer.createObject();    
    JSONencoder["node"]= scanner_ID;
    JSONencoder["data"]= data_pub;
    JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));    
    mqtt_client.publish(PUB_TOPIC, JSONmessageBuffer);
    Serial.printf("Message sended : Topic=%s Message=%s \n", PUB_TOPIC, JSONmessageBuffer);      
}

//######################################################################################################## FUNCTION MQTT SUBSCRIBER
void mqtt_sub(char* topic, byte* payload, unsigned int length) {
  Serial.print("\nMessage received : Topic=");
  Serial.print(topic);
  Serial.print(" Message=");
  Serial.println((char)payload[0]);

  if((char)payload[0] == '1'){
    scan_cmd_BLE = true;
    blinkLed(2, 50);
    Serial.println("Start scan");
    
  }else if((char)payload[0] == '0'){
    scan_cmd_BLE = false;
    blinkLed(2, 50);
    Serial.println("Stop scan");
    mqtt_pub("stop_scan");
  }
}

//######################################################################################################## FUNCTION MQTT CONNECT
void mqttConnect(void) {
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqtt_sub);
  
  while (!mqtt_client.connected()) {                                    //### Loop until reconnected
    Serial.print("MQTT connecting to ");
    Serial.print(mqtt_server);
    Serial.println("...");
    blinkLed(3, 50);
    blinkLedOff();

    if (mqtt_client.connect(scanner_ID.c_str(),mqtt_user,mqtt_pass)) {  //### Connect now
      blinkLed(1, 50);
      Serial.println("MQTT connected");      
      mqtt_client.subscribe(SUB_TOPIC);                                 //### Subscribe topic
    } else {
      Serial.print("Failed, status code = ");
      Serial.print(mqtt_client.state());
      Serial.println(", try again in 3 seconds");       
      delay(3000);
    }
  }
}

//######################################################################################################## FUNCTION HANDLER BLE AND FILTER FOR CUBEACON
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {    
//    Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str()); // print all ble

    //### filter for search a cubeacon
    char work[7];
    String hexString = (String) BLEUtils::buildHexData(nullptr, (uint8_t*)advertisedDevice.getManufacturerData().data(), advertisedDevice.getManufacturerData().length());
    if (hexString.substring(0, 8).equals("4c000215")) {
      ("0x" + hexString.substring(40, 44)).toCharArray(work, 7);
      uint16_t cb_major = (uint16_t) atof(work);
      ("0x" + hexString.substring(44, 48)).toCharArray(work, 7);
      uint16_t cb_minor = (uint16_t) atof(work);
      ("0x" + hexString.substring(48, 52)).toCharArray(work, 7);
      cb_uuid[cb_index] =  hexString.substring(8, 16) + "" + 
              hexString.substring(16, 20) + "" + 
              hexString.substring(20, 24) + "" + 
              hexString.substring(24, 28) + "" + 
              hexString.substring(28, 40) + "/" +
              cb_major + "/" + cb_minor;
      cb_rssi[cb_index] = advertisedDevice.getRSSI() * -1;

      Serial.print("> ");
      Serial.print(cb_uuid[cb_index]);
      Serial.print(" ");
      Serial.println(cb_rssi[cb_index]);
      cb_index++;
    }
  }
};

//######################################################################################################## FUNCTION SETUP BLE
void setupBLE(void){
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();              // Create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);                // Active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);                      // Less or equal setInterval value
}

//######################################################################################################## FUNCTION SCAN BLE FOR CUBEACON
void scanCubeacon(int scanTimeBLE){             // Var scanTimeBLE in second unit
  Serial.print("\nScanning cubeacon : Start\n");
  pBLEScan->start(scanTimeBLE, false);
  pBLEScan->clearResults();                     // Delete results fromBLEScan buffer to release memory
  Serial.print("Scanning cubeacon : Done\n");
}

//######################################################################################################## FUNCTION BUBBLE SORT FOR CUBEACON
void sortData(int *a, String *b, int n){
  for (int i = 1; i < n; ++i){
    int j = a[i];
    String jn = b[i];
    int k;
    for (k = i - 1; (k >= 0) && (j < a[k]); k--){ 
      a[k + 1] = a[k]; 
      b[k + 1] = b[k];     
    }
    a[k + 1] = j;
    b[k + 1] = jn;
  }
}

//######################################################################################################## FUNCTION SORTING DATA CUBEACON
void sortCubeacon(void){
  //### Sorting bubble sort for RSSI (Received Signal Strength Indicator)
  sortData(cb_rssi, cb_uuid, cb_sum);

//  Serial.println("Sorting cubeacon");
//  for(int i = 0; i < cb_sum; i++){
//    if(cb_rssi[i] != 0){
//      Serial.print(cb_uuid[i]);
//      Serial.print(" ");
//      Serial.println(cb_rssi[i]);
//    }
//  }  
}

//######################################################################################################## FUNCTION NEAREST CUBEACON
void nearestCubeacon(void){
  String cb_nearest_uuid;
  int cb_nearest_rssi;
  
  for(int i = 0; i < cb_sum; i++){
    if(cb_rssi[i] != 0){
      cb_nearest_uuid = cb_uuid[i];
      cb_nearest_rssi = cb_rssi[i];
      break;
    }
  }
  Serial.print("Nearest cubeacon : ");
  Serial.print(cb_nearest_uuid);
  Serial.print(" ");
  Serial.println(cb_nearest_rssi);
  mqtt_pub(cb_nearest_uuid);
  blinkLed(1, 50);  
}

//######################################################################################################## FUNCTION CLEAR CUBEACON
void clearCubeacon(void){
  cb_index = 0;
  for(int n = 0; n < cb_sum; n++){
    cb_uuid[n] = "";
    cb_rssi[n] = 0;
  }
}

//######################################################################################################## FUNCTION SETUP ARDUINO
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  setupBLE();
}

//######################################################################################################## FUNCTION LOOP ARDUINO
void loop() {
  while(WiFi.status() != WL_CONNECTED){ wifiConnect(); }

  while(!mqtt_client.connected()){ mqttConnect(); }
  mqtt_client.loop();

  if(scan_cmd_BLE == true){
    scanCubeacon(5);    //### Has a time parameter for scanning all BLE in handler
    sortCubeacon();
    nearestCubeacon();    
    clearCubeacon();
  }
}
