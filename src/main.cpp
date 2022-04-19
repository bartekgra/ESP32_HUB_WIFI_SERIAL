#include <Arduino.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "ThingSpeak.h"
#include <string>
#include <iostream>
#include <ArduinoJson.h>

// Set your access point network credentials
const char* ssid = "***********";   // your network SSID (name) 
const char* password = "***********";   // your network password

WiFiClient  client;

AsyncWebServer server(80);

unsigned long myChannelNumber = 1683811;
const char * myWriteAPIKey = "***********";
const char * myReadAPIKey = "***********";

String serverName = "https://api.thingspeak.com/channels/1683811/feeds.json?api_key=RRIA1L2JRSG0KSWJ&results=1";

#define LED 2
bool ledState;

String decodeJSON(char *json, String field);

QueueHandle_t xQueue1;
SemaphoreHandle_t xSemaphoreSerialWrite;

void serialWrite(void * parameter){
  uint32_t num_to_send = 0;
  char data_serial[4];
  for(;;){
    if(xSemaphoreTake(xSemaphoreSerialWrite, 0) == pdTRUE ){
      xQueueReceive(xQueue1, &(num_to_send), 0);
      if(Serial.availableForWrite()){
        data_serial[0] = (num_to_send >> 24)& 0xFF;
        data_serial[1] = (num_to_send >> 16) & 0xFF;
        data_serial[2] = (num_to_send >> 8) & 0xFF;
        data_serial[3] = (num_to_send) & 0xFF;

        Serial.write(data_serial, 4);
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void toggleLED(void * parameter){
  for(;;){
    digitalWrite(LED, HIGH);
    ledState = true;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(LED, LOW);
    ledState = false;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void wifiUpdate(void * parameter){
  uint32_t number_read = 555555;
  for(;;){
    if(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, password);
    }else{
      HTTPClient http;
      http.begin(serverName.c_str());
      if(http.GET() == 200){
        String payload = http.getString();
        char* char_arr = &payload[0];
        http.end();

        String tmp_strr = decodeJSON(char_arr, "field2");
        number_read = atoi(tmp_strr.c_str());
        
        xQueueSend( xQueue1, ( void * ) &number_read, ( TickType_t ) 0 );
        xSemaphoreGive(xSemaphoreSerialWrite);
      }
    }

    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

String test() {
  Serial.println("http get");
  if(ledState)
    return String("LED ON");
  else
    return String("LED OFF");
}

void setup() {
  Serial.begin(115200);
    
  WiFi.mode(WIFI_STA);   
  
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  pinMode(LED, OUTPUT);

  xQueue1 = xQueueCreate( 1, sizeof(uint32_t));
  xSemaphoreSerialWrite = xSemaphoreCreateBinary();

  xTaskCreate(
    toggleLED,       // Function that should be called
    "Toggle LED",    // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreate(
    serialWrite,       // Function that should be called
    "Serial Write",    // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreatePinnedToCore(
    wifiUpdate,       // Function that should be called
    "WiFi Update",    // Name of the task (for debugging)
    5000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL,             // Task handle
    CONFIG_ARDUINO_RUNNING_CORE
  );

}

void loop() {  

}

String decodeJSON(char *json, String field) {
  StaticJsonBuffer<3*1024> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(json); // Parse JSON
  if (!root.success()) {
  }
  JsonObject& root_data = root["channel"]; // Begins and ends within first set of { }
  String id   = root_data["id"];
  String name = root_data["name"];
  String field1_name = root_data["field1"];
  String datetime    = root_data["updated_at"];
  
  for (int result = 0; result < 5; result++){
    JsonObject& channel = root["feeds"][result]; // Now we can read 'feeds' values and so-on
    String entry_id     = channel["entry_id"];
    String field1value  = channel["field2"];

    return field1value;
  }
}
