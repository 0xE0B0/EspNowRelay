#include "EspNowRelay.h"
#include "credentials.h"
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>

// Status LED
LEDControl led(LED_BUILTIN, true);

ESP8266WebServer server(80);

// Relay output pin
static constexpr uint8_t RELAY_PIN = D1;

// Button pin to trigger OTA update mode
static constexpr uint8_t BUTTON_PIN = D2;

// Magic key to identify datagrams
static constexpr uint32_t MAGIC_KEY = 0xDEADBEEF;

struct __attribute__((packed)) Datagram {
    uint32_t magic;
    uint8_t switchState;
    uint8_t activeChannels;

};

static inline Print& beginl(Print &stream) {
    static constexpr const char name[] = "REL";
    return beginl<name>(stream);
}

void packetReceived_cb(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    if (len != sizeof(Datagram)) {
        Serial << beginl << red << "invalid packet size: " << len << DI::endl;
        return;
    }
    Datagram* data = reinterpret_cast<Datagram*>(incomingData);
    if (data->magic != MAGIC_KEY) {
        Serial << beginl << red << "invalid magic key: " << hex << data->magic << DI::endl;
        return;
    }
    Serial << beginl << green << "packet received: switchState=" << int(data->switchState)
           << " activeChannels=" << int(data->activeChannels) << DI::endl;
    if (data->switchState) {
        led.setState(LEDControl::LED_ON);
        digitalWrite(RELAY_PIN, HIGH);  // relay on
    } else {
        led.setState(LEDControl::LED_OFF);
        digitalWrite(RELAY_PIN, LOW);   // relay off
    }
}

void setupEspNowDevice() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    bool initialized = (esp_now_init() == 0);
    if (!initialized) {
        Serial << beginl << red << "esp-now init failed" << DI::endl;
        led.setState(LEDControl::LED_FLASH_FAST);
    } else {
        Serial << beginl << green << "esp-now device ready" << DI::endl;
        esp_now_register_recv_cb(packetReceived_cb);
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        led.setState(LEDControl::LED_OFF);
    }
}

void setupOTAUpdate() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial << beginl << yellow << "Connecting to " << WIFI_SSID << "..." << DI::endl;

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        led.setState(LEDControl::LED_ON);
        delay(250);
        led.setState(LEDControl::LED_OFF);
        delay(250);
        Serial << ".";
    }
    Serial << DI::endl;
    Serial << beginl << green << "Connected to " << WIFI_SSID << DI::endl;
    Serial << beginl << yellow << "IP address: " << WiFi.localIP() << DI::endl;
    led.setState(LEDControl::LED_FLASH_SLOW);

    server.on("/", []() {
        server.send(200, "text/html",
            "<!DOCTYPE html>"
            "<html lang=\"en\">"
            "<head>"
                "<meta charset=\"UTF-8\">"
                "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                "<title>ESP-NOW-Relay Firmware Update</title>"
                "<style>"
                    "body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background-color: #f5f5f5; }"
                    ".container { background-color: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
                    "h1 { color: #333; }"
                    ".info { background-color: #e3f2fd; padding: 15px; border-radius: 4px; margin: 20px 0; }"
                    "a { color: #1976d2; text-decoration: none; }"
                    "a:hover { text-decoration: underline; }"
                "</style>"
            "</head>"
            "<body>"
                "<div class=\"container\">"
                    "<h1>ESP-NOW-Relay Firmware Update</h1>"
                    "<p>This is the Firmware Update Webinterface for ESP-NOW-Relay.</p>"
                    "<div class=\"info\">"
                        "<p>Current firmware version: " + String(REL_VERSION_MAJOR) + "." + String(REL_VERSION_MINOR) + "." + String(REL_VERSION_SUB) + " (" + __TIMESTAMP__ + ")</p>"
                    "</div>"
                    "<div class=\"info\">"
                        "<p>To upload new firmware, go to <a href=\"/update\">/update.</a></p>"
                    "</div>"
                "</div>"
            "</body>"
            "</html>"
        );
    });

    ElegantOTA.begin(&server);
    server.begin();
    Serial << beginl << yellow << "HTTP server started" << DI::endl;
    led.setState(LEDControl::LED_OFF);

}

void setup() {
    Serial.begin(debugBaudRate);
    Serial << magenta << F("ESP-NOW-Relay v");
    Serial << magenta << REL_VERSION_MAJOR << F(".") << REL_VERSION_MINOR << F(".") << REL_VERSION_SUB;
    Serial << magenta << F(" (") <<  __TIMESTAMP__  << F(")") << DI::endl;

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // relay off

    led.setState(LEDControl::LED_FLASH_SLOW);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    bool uploadState = digitalRead(BUTTON_PIN) != LOW;
    if (uploadState) {
        setupOTAUpdate();
    } else {
        setupEspNowDevice();
    }
}

void loop() {
    led.update();
    server.handleClient();
    ElegantOTA.loop();
}