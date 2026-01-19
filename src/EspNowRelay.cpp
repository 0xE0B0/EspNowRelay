#include <Arduino.h>
#include "DebugInterface.h"
#include "LEDControl.h"
#include "version.h"
#include <espnow.h>
#include <WiFiManager.h>

// WiFi manager for OTA update and WiFi configuration
WiFiManager wm;

// Status LED
LEDControl led(LED_BUILTIN, true);

// Relay output pin
static constexpr uint8_t RELAY_PIN = D1;

// Button pin to trigger OTA update mode
static constexpr uint8_t BUTTON_PIN = D2;

// Access point name
static constexpr const char* APName = "ESP-NOW-Relay_AP";

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

// Regular operation / esp now device
void startEspNowDevice() {
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
// Wi-Fi manager + OTA update mode
void startWiFiManager() {
    WiFi.mode(WIFI_STA);
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(60);
    bool res = wm.autoConnect(APName);
    if (!res) {
        Serial << beginl << yellow << "started config portal in AP mode, IP: " << WiFi.softAPIP() << DI::endl;
        led.setState(LEDControl::LED_FLASH_FAST);
    } else {
        Serial << beginl << green << "connected to Wi-Fi with IP: " << WiFi.localIP() << DI::endl;
    }
}

void setup() {
    Serial.begin(debugBaudRate);
    Serial << magenta << F("ESP-NOW-Relay v");
    Serial << magenta << REL_VERSION_MAJOR << F(".") << REL_VERSION_MINOR << F(".") << REL_VERSION_SUB;
    Serial << magenta << F(" (") <<  __TIMESTAMP__  << F(")") << DI::endl;

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // relay off
    led.setState(LEDControl::LED_FLASH_SLOW);

    // button pressed at startup for one second enters config mode
    // to configure Wifi credentials and run OTA firmware update

    auto buttonPressed = []() {
        bool pressed = true;
        for (uint16_t i = 0; i < 1000; i++) {
            if (digitalRead(BUTTON_PIN) == HIGH) {
                pressed = false;
                break;
            }
            delay(1);
        }
        return pressed;
    };

    if ((digitalRead(BUTTON_PIN) == LOW) && buttonPressed()) {
        Serial << beginl << yellow << "button held for one second, entering config mode" << DI::endl;
        startWiFiManager();
    } else {
        startEspNowDevice();
    }
}

void loop() {
    led.update();
    wm.process();
}