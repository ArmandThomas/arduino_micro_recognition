#include <PDM.h>

static const char channels = 1;
static const int frequency = 4000;
const int targetSamples = 8000; 

short* sampleBuffer = nullptr;
int bufferSize = 0;
volatile int bufferIndex = 0;


int trainingCount = 0;
int trainingAsked = 100;

void setup() {
  // Initialisez la communication série
  Serial.begin(9600);

  PDM.onReceive(onPDMdata);
    if (!PDM.begin(channels, frequency)) {
        Serial.println("Failed to start PDM!");
        while (true) {}
  }

  bufferSize = targetSamples;
  sampleBuffer = new short[bufferSize];
}

void loop() {
  while (trainingCount < trainingAsked) {
    if (bufferIndex >= targetSamples) {
        printData();
        bufferIndex = 0;
    }
    delay(250);
  }
}

void onPDMdata() {
    int bytesAvailable = PDM.available();
    int samplesToRead = bytesAvailable / 2;
    if (bufferIndex + samplesToRead > bufferSize) {
        samplesToRead = bufferSize - bufferIndex;
    }
    PDM.read(sampleBuffer + bufferIndex, bytesAvailable);
    bufferIndex += samplesToRead;
}

void printData() {
  Serial.println();
    for (int i = 0; i < bufferIndex; i++) {
    Serial.print(sampleBuffer[i]);
    if (i < bufferIndex - 1) {
      Serial.print(", ");
    }
  }
}