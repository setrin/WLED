#pragma once

#include "wled.h"

class UsermodPhotoresistorMqtt : public Usermod {
  private:
    int8_t LDR_PIN; // Analog pin where DLR is connected
    long UPDATE_MS; // Upper threshold between mqtt messages
    int CHANGE_THRESHOLD; // Change threshold in percentage to send before UPDATE_MS
    String SENSOR_NAME; // Sensor name used in MQTT messages
    bool INVERT_VALUE; // Invert analog value from LDR

    // variables
    String mqttLightSensorTopic = "";
    bool mqttInitialized = false;
    long lastTime = 0;
    long timeDiff = 0;
    long readTime = 0;
    int lightValue = 0; 
    float lightPercentage = 0;
    float lastPercentage = 0;
  public:
    void setup() {
      pinMode(LDR_PIN, INPUT);
    }

    void _mqttInitialize() {
      mqttLightSensorTopic = String(mqttDeviceTopic) + "/" + String(SENSOR_NAME);

      String t = String("homeassistant/sensor/") + mqttClientID + "/" + String(SENSOR_NAME) + "/config";

      StaticJsonDocument<300> doc;

      doc["name"] = String(SENSOR_NAME);
      doc["state_topic"] = mqttLightSensorTopic;
      doc["unique_id"] = String(mqttClientID) + String(SENSOR_NAME);
      doc["unit_of_measurement"] = "%";
      doc["expire_after"] = 1800;

      JsonObject device = doc.createNestedObject("device"); // attach the sensor to the same device
      device["identifiers"] = String("wled-sensor-") + mqttClientID;
      device["manufacturer"] = "Aircoookie";
      device["model"] = "WLED";
      device["sw_version"] = VERSION;
      device["name"] = mqttClientID;

      String temp;
      serializeJson(doc, temp);
      Serial.println(t);
      Serial.println(temp);

      mqtt->publish(t.c_str(), 0, true, temp.c_str());
    }

    void loop() {
      // Read only every 1000ms, otherwise it causes the board to hang
      if (millis() - readTime > 1000) {
        readTime = millis();
        timeDiff = millis() - lastTime;

        Serial.print("PIN: ");
        Serial.println(LDR_PIN);

        if (WLED_MQTT_CONNECTED) {
          if (!mqttInitialized) {
            _mqttInitialize();
            mqttInitialized = true;
          }

          // Convert value to percentage
          lightValue = analogRead(LDR_PIN);

          if (INVERT_VALUE) {
            lightValue = (float)lightValue * -1 + 1024;
          }

          lightPercentage = (float)lightValue / (float)1024 *100;
          
          // Send MQTT message on significant change or after UPDATE_MS
          if (abs(lightPercentage - lastPercentage) > CHANGE_THRESHOLD || timeDiff > UPDATE_MS) {
            mqtt->publish(mqttLightSensorTopic.c_str(), 0, true, String(lightPercentage).c_str());

            lastTime = millis();
            lastPercentage = lightPercentage;
          }
        } else {
          Serial.println("Missing MQTT connection. Not publishing data");
          mqttInitialized = false;
        }
      }
    }

    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject("UsermodPhotoresistorMqtt");
      
      JsonArray pinArray = top.createNestedArray("LDR_PIN");
      pinArray.add(LDR_PIN);
      top["LDR_PIN"] = LDR_PIN;
      top["INVERT_VALUE"] = INVERT_VALUE;
      top["UPDATE_MS"] = UPDATE_MS;
      top["CHANGE_THRESHOLD"] = CHANGE_THRESHOLD;
      top["SENSOR_NAME"] = SENSOR_NAME;
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root["UsermodPhotoresistorMqtt"];

      bool configComplete = !top.isNull();

      // A 3-argument getJsonValue() assigns the 3rd argument as a default value if the Json value is missing
      configComplete &= getJsonValue(top["LDR_PIN"][0], LDR_PIN, A0);
      configComplete &= getJsonValue(top["INVERT_VALUE"], INVERT_VALUE, false);
      configComplete &= getJsonValue(top["UPDATE_MS"], UPDATE_MS, 30000);  
      configComplete &= getJsonValue(top["CHANGE_THRESHOLD"], CHANGE_THRESHOLD, 5);
      configComplete &= getJsonValue(top["SENSOR_NAME"], SENSOR_NAME, "light_percentage");

      return configComplete;
    }
};