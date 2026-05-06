// COS583 Smart Home Edge AI with Arduino Artemis
// Han Lee & Isha Wagle
// Arduino Artemis code

// included in edge impulse header file:
// DSP preprocessing (MFCC, etc.) --> extract to features 
// trained neural network
// helper functions
#include <v2SmartHome_inferencing.h>
#include <Servo.h>

#define BAUD_RATE 460800

#define SERVO_PIN 9
#define LED_PIN1 4
#define LED_PIN2 5

// adjustable thresholds to decide when to trigger actuators 
#define GARAGE_THRESHOLD 0.35
#define WINDOW_THRESHOLD 0.35
#define COOLDOWN_MS 1500 // prevent repeated actions that occur too quickly 

Servo garageServo;

// EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE comes from edge impulse model 
// it represents number of audio samples
int16_t audioBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
uint8_t byteBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * 2];

unsigned long lastActionTime = 0;

bool waitForMagic();
bool readExact(uint8_t *buffer, int length);
bool waitForAudioPacket();
void runInference();

// wait for python code for mic setup to start
bool waitForMagic() {
  
  // "AUD0" sent from python serial sender code 
  char target[4] = { 'A', 'U', 'D', '0' };
  int matched = 0;
  unsigned long start = millis();

  while (millis() - start < 5000) {
    if (Serial.available()) {
      char c = Serial.read();

      if (c == target[matched]) {
        matched++;
        if (matched == 4) {
          return true;
        }
      }
      else {
        matched = (c == target[0]) ? 1 : 0;
      }
    }
  }

  return false;
}

// read exact number of bytes from serial with timeout
bool readExact(uint8_t *buffer, int length) {
  
  int bytesRead = 0;
  unsigned long start = millis();

  while (bytesRead < length) {
    if (Serial.available()) {
      // incoming serial data; buffer is byteBuffer
      // take incoming bytes and place them starting at this position in byteBuffer
      bytesRead += Serial.readBytes(buffer + bytesRead, length - bytesRead);
    }

    if (millis() - start > 10000) {
      return false;
    }
  }

  return true;
}

// wait for packet from python code with raw audio data 
bool waitForAudioPacket() {
  if (!waitForMagic()) {
    Serial.println("ERROR no magic");
    return false;
  }

  Serial.println("GOT MAGIC");

  int neededBytes = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * 2;

  if (!readExact(byteBuffer, neededBytes)) {
    Serial.println("ERROR incomplete audio");
    return false;
  }

  Serial.println("GOT AUDIO");

  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
    audioBuffer[i] = (int16_t)(
      ((uint16_t)byteBuffer[2 * i]) |
      ((uint16_t)byteBuffer[2 * i + 1] << 8)
    );
  }

  return true;
}

// convert to float for neural net inference input 
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  for (size_t i = 0; i < length; i++) {
    out_ptr[i] = (float)audioBuffer[offset + i];
  }
  return 0;
}

// MAIN INFERENCE FUNCTION -- runs the classifier and emits actuator responses accordingly
void runInference() {
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &raw_feature_get_data;

  ei_impulse_result_t result = {0};

  unsigned long startTime = millis();
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
  unsigned long inferenceTime = millis() - startTime;

  // troubleshooting inference errors
  if (res != EI_IMPULSE_OK) {
    Serial.print("ERROR classifier ");
    Serial.println(res);
    return;
  }

  // 4 classification labels
  float garageScore = 0.0;
  float windowScore = 0.0;
  float noiseScore = 0.0;
  float unknownScore = 0.0;

  String bestLabel = "";
  float bestScore = 0.0;

  // go through all 4 labels to evaluate which has highest confidence 
  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    String label = ei_classifier_inferencing_categories[i];
    float score = result.classification[i].value;

    if (score > bestScore) {
      bestScore = score;
      bestLabel = label;
    }

    if (label == "garage_door") {
      garageScore = score;
    }
    else if (label == "window_light") {
      windowScore = score;
    }
    else if (label == "noise") {
      noiseScore = score;
    }
    else if (label == "unknown") {
      unknownScore = score;
    }
  }

  Serial.print("HEARD: ");
  Serial.print(bestLabel);
  Serial.print(" ");
  Serial.print(bestScore, 3);
  Serial.print(" garage=");
  Serial.print(garageScore, 3);
  Serial.print(" window=");
  Serial.print(windowScore, 3);
  Serial.print(" noise=");
  Serial.print(noiseScore, 3);
  Serial.print(" unknown=");
  Serial.print(unknownScore, 3);
  Serial.print(" inference_ms=");
  Serial.println(inferenceTime);

  if (millis() - lastActionTime < COOLDOWN_MS) {
    return;
  }

  // emit actuator response accordingly
  if (garageScore > GARAGE_THRESHOLD && garageScore > windowScore) {
    lastActionTime = millis();
    openGarage();
  }
  else if (windowScore > WINDOW_THRESHOLD && windowScore > garageScore) {
    lastActionTime = millis();
    turnOnWindowLight();
  }
}

// garage door servo actuator (open - wait - close)
void openGarage() {
  Serial.println("ACTION: GARAGE DOOR");

  // raise slowly to prevent sudden torque
  for (int pos = 0; pos <= 90; pos++) {
    garageServo.write(pos);
    delay(20);
  }

  delay(3000);

  for (int pos = 90; pos >= 0; pos--) {
    garageServo.write(pos);
    delay(20);
  }
}

// window light LED actuator (on - wait - off)
void turnOnWindowLight() {
  Serial.println("ACTION: WINDOW LIGHT");

  digitalWrite(LED_PIN1, HIGH);
  digitalWrite(LED_PIN2, HIGH);
  delay(5000);
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
}

// arduino setup
void setup() {
  Serial.begin(BAUD_RATE);
  Serial.setTimeout(50);

  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);

  garageServo.attach(SERVO_PIN);
  garageServo.write(0);

  delay(1500);

  Serial.println("Artemis READY-window classifier");
  Serial.print("Expected samples: ");
  Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
}

void loop() {
  Serial.println("READY");

  // wait for audio packet from python code
  // then run inference and emit actuator responses accordingly
  bool audio_received = waitForAudioPacket(); 

  if (!audio_received) {
    Serial.println("ERROR packet");
    delay(200);
    return;
  }

  runInference();
}
