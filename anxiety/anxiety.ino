#define USE_ARDUINO_INTERRUPTS false
#include <PulseSensorPlayground.h>
#include <WiFi101.h>
#include <MQTTClient.h>

// Transmit to shiftr.io
const char DEVICE_NAME[] = "anxiety";
const char SHIFTRIO_KEY[] = "11773d67";
const char SHIFTRIO_SECRET[] = "a2ad7ed0849fb6b6";
const char ssid[] = "BRIDGE";
const char pass[] = "internet";
const int SEND_INTERVAL = 500;
unsigned long lastMillis = 0;
WiFiClient net;
MQTTClient client;

// Heart beat
const int OUTPUT_TYPE = SERIAL_PLOTTER;
const int PIN_BEAT = A1;
const int THRESHOLD = 550;   // Adjust this number to avoid noise when idle
const byte SAMPLES_PER_SERIAL_SAMPLE = 10;
byte samplesUntilReport;
PulseSensorPlayground pulseSensor;

// Skin Conductivity
const int PIN_SKIN = A2;

// Heat
const int PIN_HEAT = 8;

void setup() {
	Serial.begin(115200);

	WiFi.begin(ssid, pass);
	client.begin("broker.shiftr.io", net);
	client.onMessage(messageReceived);
	connect();

	// Configure the PulseSensor manager.
	pulseSensor.analogInput(PIN_BEAT);
	pulseSensor.setSerial(Serial);
	pulseSensor.setOutputType(OUTPUT_TYPE);
	pulseSensor.setThreshold(THRESHOLD);

	// Skip the first SAMPLES_PER_SERIAL_SAMPLE in the loop().
	samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;

	pulseSensor.begin();
}

void loop() {
	client.loop();

	if (!client.connected()) {
		connect();
	}

	if (pulseSensor.sawNewSample()) {
		if (--samplesUntilReport == (byte) 0) {
			samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;

			if (pulseSensor.sawStartOfBeat()) {
				if (millis() - lastMillis > SEND_INTERVAL) {
					lastMillis = millis();
					client.publish("/bpm", String(pulseSensor.getBeatsPerMinute()));
					client.publish("/skin", String(analogRead(PIN_SKIN)));
				}
			}
		}
  	}
}

void connect() {
	Serial.print("checking wifi...");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(1000);
	}

	Serial.print("\nconnecting...");
	while (!client.connect(DEVICE_NAME, SHIFTRIO_KEY, SHIFTRIO_SECRET)) {
		Serial.print(".");
		delay(1000);
	}

	Serial.println("\nconnected!");

	// client.subscribe("/hello");
	// client.unsubscribe("/hello");
}

void messageReceived(String &topic, String &payload) {
	Serial.println("incoming: " + topic + " - " + payload);
}
