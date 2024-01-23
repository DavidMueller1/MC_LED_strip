#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// WiFi credentials
#define SSID "Rate mal"
#define PASSWORD "30484188001738191733"

// MQTT credentials
#define MQTT_SERVER "192.168.178.29"
#define MQTT_PORT 1883
#define MQTT_USER "stairled"
#define MQTT_PASSWORD "supersecret"
#define CLIENT_ID "ESP32"
#define STATE_TOPIC "home/stair_leds/status"
#define COMMAND_TOPIC "home/stair_leds/set"

// LED strip configuration
#define LED_PIN 5
#define NUM_LEDS 96
#define ANIMATION_TIME 500

// LED strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

String current_state = "OFF";
int current_brightness = 255;
int current_red = 255;
int current_green = 255;
int current_blue = 255;


void scaleRGB(int *red, int *green, int *blue, int brightness) {
    *red = *red * brightness / 255;
    *green = *green * brightness / 255;
    *blue = *blue * brightness / 255;
}

void setAllLeds(int red, int green, int blue) {
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(red, green, blue));
    }
    strip.show();
}

void animateLinear(int red, int green, int blue, int duartion) {
    int stepDelay = duartion / NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(red, green, blue));
        strip.show();
        delay(stepDelay);
    }
}

void updateLeds(boolean animate) {
    if (current_state == "ON") {
        int red = current_red;
        int green = current_green;
        int blue = current_blue;
        scaleRGB(&red, &green, &blue, current_brightness);
        if (animate) {
            animateLinear(red, green, blue, ANIMATION_TIME);
        } else {
            setAllLeds(red, green, blue);
        }
    } else {
        if (animate) {
            animateLinear(0, 0, 0, ANIMATION_TIME);
        } else {
            setAllLeds(0, 0, 0);
        }
    }
}

void publishState() {
    Serial.println("Publishing state");
    StaticJsonDocument<2024> doc;
    doc["state"] = current_state;
    doc["brightness"] = current_brightness;
    JsonObject color = doc.createNestedObject("color");
    color["r"] = current_red;
    color["g"] = current_green;
    color["b"] = current_blue;
    char buffer[2024];
    serializeJson(doc, buffer);
    client.publish(STATE_TOPIC, buffer);
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);

    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();

    if (strcmp(topic, COMMAND_TOPIC) == 0) {
        Serial.println("Command topic");
        StaticJsonDocument<2024> doc;
        deserializeJson(doc, payload, length);

        boolean animate = false;
        JsonObject obj = doc.as<JsonObject>();
        for (JsonPair kv : obj) {
            if (strcmp(kv.key().c_str(), "state") == 0) {
                String set_state = kv.value().as<String>();
                if (current_state != set_state) {
                    current_state = set_state;
                    animate = true;
                }
            } else if (strcmp(kv.key().c_str(), "brightness") == 0) {
                current_brightness = kv.value().as<int>();
            } else if (strcmp(kv.key().c_str(), "color") == 0) {
                JsonObject color = kv.value().as<JsonObject>();
                current_red = color["r"];
                current_green = color["g"];
                current_blue = color["b"];
            }
        }
        publishState();
        updateLeds(animate);
    }
}

void setupStrip() {
    strip.begin();
    strip.setPixelColor(0, strip.Color(50, 50, 50));
    strip.show();
    delay(200);
    strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.show();
    delay(200);
    strip.setPixelColor(0, strip.Color(50, 50, 50));
    strip.show();
    delay(200);
    strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.show(); // Initialize all pixels to 'off'
}

void setupWifi() {
    delay(10);
    // Connect to Wi-Fi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        strip.setPixelColor(0, strip.Color(0, 50, 30));
        delay(500);
        strip.setPixelColor(0, strip.Color(0, 0, 0));
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupMqtt() {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
}

void reconnect() {
    Serial.println("Reconnecting...");
    // Loop until we're reconnected
    while (!client.connected()) {
        if (WiFi.status() != WL_CONNECTED) {
            setupWifi();
        }
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("connected");
            // Subscribe
            client.subscribe(COMMAND_TOPIC);
            publishState();
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// Simple mqtt client that will publish a message to a topic and subscribe to a topic.
void setup() {
    Serial.begin(115200);
    delay(10);
    Serial.println("Starting...");
    setupStrip();
    setupWifi();
    setupMqtt();
}

void loop() {
    // Reconnect to wifi if connection is lost
    if (!client.connected() || WiFi.status() != WL_CONNECTED) {
        reconnect();
    }
    client.loop();
    delay(50);
}
