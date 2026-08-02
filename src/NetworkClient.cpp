#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class NetworkClient {

public:
    std::unique_ptr<WiFiClientSecure> httpClient;
    NetworkClient() {
        Serial.println("Creating NetworkClient...");
        httpClient.reset(new WiFiClientSecure);
        httpClient->setInsecure();
    }
};
