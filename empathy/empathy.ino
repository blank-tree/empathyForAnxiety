#define USE_ARDUINO_INTERRUPTS false
#include <PulseSensorPlayground.h>
#include <WiFi101.h>
#include <MQTTClient.h>

unsigned long currentTime = 0;

// Transmit and receive with shiftr.io
const char DEVICE_NAME[] = "empathy";
const char SHIFTRIO_KEY[] = "11773d67";
const char SHIFTRIO_SECRET[] = "a2ad7ed0849fb6b6";
const char ssid[] = "BRIDGE";
const char pass[] = "internet";
const char PUBLISH_BPM[] = "/empathy-bpm";
const char PUBLISH_SKIN[] = "/empathy-skin";
const char RECEIVE_BPM[] = "/anxiety-bpm";
const char RECEIVE_SKIN[] = "/anxiety-skin";
const int SEND_INTERVAL = 450;
unsigned long lastTransmit = 0;
WiFiClient net;
MQTTClient client;

// Heart beat
const int OUTPUT_TYPE = SERIAL_PLOTTER;
const int PIN_BEAT = A1;
const int THRESHOLD = 550;   // Adjust this number to avoid noise when idle
const byte SAMPLES_PER_SERIAL_SAMPLE = 10;
byte samplesUntilReport;
PulseSensorPlayground pulseSensor;

// Galvanic skin response sensor
const int PIN_SKIN = A2;

// Vibration
const int VIBRATION_PINS[] = {6, 7, 8, 9};
const int vibrationMapLow = 200;
const int vibrationMapHigh = 500;
const int vibrationInputThreshold = 200;
int vibrationInput = 400;
int vibrationInterval = 100;
boolean vibrationActive = false;
boolean vibrationStatus[] = {false, false, false, false};
unsigned long vibrationLastExec = 0;

// Stepper
const int PIN_STEP = 3;
const int PIN_DIRE = 5;
const int STEPPER_THRESHOLD = 400;
const int STEPPER_POSITION_MAX = 100;
int stepperPosition = 0;
int stepperInput = 0;
boolean stepperDirection = false;

void setup() {
	Serial.begin(115200);

	for(int i = 0; i < 4; i++){
	    pinMode(VIBRATION_PINS[i], OUTPUT);
	}

	pinMode(PIN_STEP, OUTPUT);
	pinMode(PIN_DIRE, OUTPUT);

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
	currentTime = millis();

	client.loop();

	if (!client.connected()) {
		connect();
	}

	if (pulseSensor.sawNewSample()) {
		if (--samplesUntilReport == (byte) 0) {
			samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;

			if (pulseSensor.sawStartOfBeat()) {
				if (currentTime - lastTransmit > SEND_INTERVAL) {
					lastTransmit = currentTime;
					client.publish(PUBLISH_BPM, String(pulseSensor.getBeatsPerMinute()));
					client.publish(PUBLISH_SKIN, String(analogRead(PIN_SKIN)));
				}
			}
		}
  	}

  	vibrationLoop();
  	stepperLoop();
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

	client.subscribe(RECEIVE_BPM);
	client.subscribe(RECEIVE_SKIN);
}

void messageReceived(String &topic, String &payload) {
	// Serial.println("incoming: " + topic + " - " + payload);
	if (topic == RECEIVE_BPM) {
		vibrationInput = payload.toInt();
		updateVibrationInterval();
	}
	if (topic == RECEIVE_SKIN) {
		stepperInput = payload.toInt();
		updateStepperStatus();
	}
}

void stepperLoop() {
	if (stepperDirection) {
		digitalWrite(PIN_DIRE, HIGH);
		if (stepperPosition <= STEPPER_POSITION_MAX) {
			digitalWrite(PIN_STEP, HIGH);
			stepperPosition++;
		} else {
			digitalWrite(PIN_STEP, LOW);
		}
	} else {
		digitalWrite(PIN_DIRE, LOW);
		if (stepperPosition >= 0) {
			digitalWrite(PIN_STEP, HIGH);
			stepperPosition--;
		} else {
			digitalWrite(PIN_STEP, LOW);
		}
	}
}

void updateStepperStatus() {
	stepperDirection = stepperInput > STEPPER_THRESHOLD;
}

void vibrationLoop() {
	if (vibrationActive) {
		if (vibrationLastExec + vibrationInterval * 4 > currentTime) {
			for(int i=0; i < 4; i++){
			    vibrationStatus[i] = currentTime > vibrationLastExec + vibrationInterval * i 
			    	&& currentTime < vibrationLastExec + vibrationInterval * (i + 1);
			}
		} else {
			for(int i = 0; i < 4; i++){
			    vibrationStatus[i] = false;
			}
			vibrationActive = false;
		}
		activateVibration();
	}
}

void updateVibrationInterval() {
	if (!vibrationActive && vibrationInput > vibrationInputThreshold) {
		vibrationInterval = map(vibrationInput, 0, 1023, vibrationMapLow, vibrationMapHigh);
		vibrationLastExec = currentTime;
		vibrationActive = true;
	}
}

void activateVibration() {
	for(int i=0; i < 4; i++){
	    if (vibrationStatus[i]) {
	    	analogWrite(VIBRATION_PINS[i], HIGH);
	    } else {
	    	analogWrite(VIBRATION_PINS[i], LOW);
	    }
	}
}