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

void uploadDhtData(float temperature, float humidity, float score, String note);
void logTelegram(String msg);
float calculateHeatIndex(float tempC, float humidity);
float calculateScore(float temperature, float humidity);
String urlencode(String str);

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

    String telegramLog;
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT read failed (NaN)");
        telegramLog = "🟠 DHT read failed";
    } else {
        float score = calculateScore(temperature, humidity);
        Serial.println("T=" + String(temperature) + "C H=" + String(humidity) +
                       "% score=" + String(score));

        uploadDhtData(temperature, humidity, score, "");
        telegramLog = "☀️ " + String(temperature) + "C\n💧 " +
                      String(humidity) + "%\n📋 score " + String(score);
    }

    int sleepMinutes = config["sleep_time_in_minutes"].toInt();
    if (sleepMinutes <= 0) sleepMinutes = 30;
    Serial.println("Sleeping " + String(sleepMinutes) + " min...");
    telegramLog += "\n😴 " + String(sleepMinutes) + " min";
    logTelegram(telegramLog);
    delay((unsigned long)sleepMinutes * 60UL * 1000UL);
}

float calculateHeatIndex(float tempC, float humidity) {
    if (tempC < 26.0) {
        return tempC + (humidity > 70.0 ? (humidity - 70.0) * 0.02 : 0);
    }

    float tempF = (tempC * 9.0 / 5.0) + 32.0;
    float heatIndexF =
        0.5 * (tempF + 61.0 + ((tempF - 68.0) * 1.2) + (humidity * 0.094));

    if (heatIndexF > 80.0) {
        float T = tempF;
        float RH = humidity;
        heatIndexF = -42.379 + 2.04901523 * T + 10.14333127 * RH +
                     -0.22475541 * T * RH + -0.00683783 * T * T +
                     -0.05481717 * RH * RH + 0.00122874 * T * T * RH +
                     0.00085282 * T * RH * RH + -0.00000199 * T * T * RH * RH;
        if (RH < 13.0 && T >= 80.0 && T <= 112.0) {
            heatIndexF -=
                ((13.0 - RH) / 4.0) * sqrt((17.0 - abs(T - 95.0)) / 17.0);
        }
        if (RH > 85.0 && T >= 80.0 && T <= 87.0) {
            heatIndexF += ((RH - 85.0) / 10.0) * ((87.0 - T) / 5.0);
        }
    }
    return (heatIndexF - 32.0) * 5.0 / 9.0;
}

float calculateScore(float temperature, float humidity) {
    float comfortTemperature = config["comfort_temperature"].toFloat();
    float comfortHumidity = config["comfort_humidity"].toFloat();
    float tempWeight = config["temperature_weight"].toFloat();
    float humidityWeight = config["humidity_weight"].toFloat();
    float tempThreshold = config["temperature_threshold"].toFloat();
    float humidityThreshold = config["humidity_threshold"].toFloat();

    float feelsLikeTemp = calculateHeatIndex(temperature, humidity);
    float score = 0.0;

    if (feelsLikeTemp > comfortTemperature) {
        if (feelsLikeTemp > tempThreshold) {
            score += tempWeight * (10 + pow((feelsLikeTemp - tempThreshold), 2));
        } else {
            score += tempWeight * (feelsLikeTemp - comfortTemperature);
        }
    }

    if (humidity > comfortHumidity) {
        if (humidity > humidityThreshold) {
            score +=
                humidityWeight * (5 + pow((humidity - humidityThreshold), 1.5));
        } else {
            score += humidityWeight * (humidity - comfortHumidity);
        }
    }

    if (humidity > 75.0 && temperature > comfortTemperature) {
        float humidityAmplifier = (humidity - 75.0) / 25.0;
        score += tempWeight * (temperature - comfortTemperature) *
                 humidityAmplifier;
    }

    if (humidity < 40.0 && temperature > comfortTemperature) {
        float dryAirRelief = (40.0 - humidity) / 40.0 * 0.3;
        score *= (1.0 - dryAirRelief);
    }

    return truncf(score * 100) / 100;
}

void uploadDhtData(float temperature, float humidity, float score,
                   String note) {
    Serial.println("Connecting to Google Forms...");
    HTTPClient formRequest;
    if (!formRequest.begin(*client.httpClient, GOOGLE_FORM_URL)) {
        Serial.println("[HTTPS] Unable to connect");
        return;
    }

    formRequest.addHeader("Content-Type",
                          "application/x-www-form-urlencoded");
    String body = "entry.243518312=" + String(temperature) +
                  "&entry.1071209622=" + String(score) +
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

String urlencode(String str) {
    String encodedString = "";
    for (unsigned int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            char code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            char code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
        yield();
    }
    return encodedString;
}

void logTelegram(String msg) {
    if (!wifi.isConnected()) return;

    HTTPClient telegram;
    String url = "https://api.telegram.org/" + String(TELEGRAM_API_KEY) +
                 "/sendMessage?chat_id=-" + String(TELEGRAM_GROUP_ID) +
                 "&text=" + urlencode(msg);
    if (!telegram.begin(*client.httpClient, url)) {
        Serial.println("[Telegram] Unable to connect");
        return;
    }
    int responseCode = telegram.GET();
    Serial.println("[Telegram] HTTP " + String(responseCode));
    telegram.end();
}
