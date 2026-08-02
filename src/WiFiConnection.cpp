#include <Keys.h>
#include <WiFi.h>

class WiFiConnection {
public:
    void connectToWifi() {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true);

        while (true) {
            if (tryConnect(SSID, PASSWORD)) return;
            Serial.println("WiFi failed, retrying...");
            delay(2000);
        }
    }

    bool tryConnect(const char* ssid, const char* password, int timeoutSeconds = 25) {
        Serial.print("Connecting to ");
        Serial.print(ssid);
        Serial.print(" ...");

        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password);

        for (int i = 0; i < timeoutSeconds; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println(" connected");
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
                return true;
            }
            Serial.print('.');
            delay(1000);
        }

        Serial.print(" failed (status=");
        Serial.print((int)WiFi.status());
        Serial.println(")");
        WiFi.disconnect();
        return false;
    }

    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }
};
