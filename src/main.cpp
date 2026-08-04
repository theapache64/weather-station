#include <Arduino.h>
#include <HTTPClient.h>
#include <Keys.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <NetworkClient.cpp>
#include <WiFiConnection.cpp>
#include <map>

#include "DHT.h"

// ESP32 DevKit V1: raw GPIO (no D0/D1 aliases). GPIO1 is TX0 — don't use for DHT.
#define DHTPIN 5  // silkscreen D5
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

WiFiConnection wifi;
NetworkClient client;
std::map<String, String> config;

// The board is a dumb sensor now: it only uploads raw temperature/humidity.
// The discomfort score is computed by the dashboard (docs/index.html), so
// formula changes never need a reflash.
void uploadDhtData(float temperature, float humidity, String note);

std::map<String, String> fetchConfig() {
    std::map<String, String> data;
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(*client.httpClient, GOOGLE_SHEET_URL)) {
        Serial.println("Config: unable to begin HTTP");
        return data;
    }

    int responseCode = http.GET();
    Serial.println("Config HTTP: " + String(responseCode));
    if (responseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        int startPos = 0;
        while (startPos < (int)payload.length()) {
            int endPos = payload.indexOf('\n', startPos);
            if (endPos == -1) endPos = payload.length();

            String line = payload.substring(startPos, endPos);
            line.trim();
            startPos = endPos + 1;
            if (line.length() == 0) continue;

            int commaPos = line.indexOf(',');
            if (commaPos <= 0) continue;

            String key = line.substring(0, commaPos);
            String value = line.substring(commaPos + 1);
            key.replace("\"", "");
            value.replace("\"", "");
            key.trim();
            value.trim();
            if (key.length() == 0 || key == "key") continue;

            data[key] = value;
        }
    } else {
        Serial.println("Config GET failed: " + http.errorToString(responseCode));
    }
    http.end();
    Serial.println("Config entries: " + String(data.size()));
    return data;
}

void setup() {
    Serial.begin(115200);
    wifi.connectToWifi();
    dht.begin();
    config = fetchConfig();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        wifi.connectToWifi();
    }

    config = fetchConfig();

    // double-read to reduce stale DHT samples
    float temperature = NAN;
    float humidity = NAN;
    for (int i = 0; i < 2; i++) {
        temperature = dht.readTemperature();
        humidity = dht.readHumidity();
        delay(2000);
    }

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT read failed (NaN)");
    } else {
        Serial.println("T=" + String(temperature) + "C H=" + String(humidity) + "%");
        uploadDhtData(temperature, humidity, "");
    }

    int sleepMinutes = config["sleep_time_in_minutes"].toInt();
    if (sleepMinutes <= 0) sleepMinutes = 30;
    Serial.println("Sleeping " + String(sleepMinutes) + " min...");
    delay((unsigned long)sleepMinutes * 60UL * 1000UL);
}



void uploadDhtData(float temperature, float humidity, String note) {
    Serial.println("Connecting to Google Forms...");
    HTTPClient formRequest;
    if (!formRequest.begin(*client.httpClient, GOOGLE_FORM_URL)) {
        Serial.println("[HTTPS] Unable to connect");
        return;
    }

    formRequest.addHeader("Content-Type",
                          "application/x-www-form-urlencoded");
    // score entry intentionally left empty — the dashboard computes it
    String body = "entry.243518312=" + String(temperature) +
                  "&entry.1071209622=" +
                  "&entry.1423375811=" + String(note) +
                  "&entry.962580231=" + String(humidity);
    int httpCode = formRequest.POST(body);
    if (httpCode > 0) {
        Serial.println("[HTTPS] POST code: " + String(httpCode));
    } else {
        Serial.println("[HTTPS] POST failed: " + String(httpCode) + " - " +
                       formRequest.errorToString(httpCode));
    }
    formRequest.end();
}


