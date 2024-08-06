#include <EEPROM.h>
#include <Base64.h>
#include <ArduinoBLE.h>
#include <neuton.h>
#include <PDM.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

char ssid[32] = "";
char pass[32] = "";

static const char channels = 1;
static const int frequency = 4000;
const int targetSamples = 8000;
short* sampleBuffer = nullptr;
int bufferSize = 0;
volatile int bufferIndex = 0;
int trainingCount = 0;
const char* macAddress = "98:aa:ca:f1:3d:75";
const char* serverIP = "192.168.85.183";
const int port = 3000;

const char* serviceUUID = "5ac1d00e-1ce6-4e53-b904-0822cd136d05";
const char* characteristicUUID = "43ada7f1-7ec0-4e14-a4af-920859b5a9e5";

int nbrBackground = 0;
String lastStored = "background";

neuton_input_t raw_inputs[targetSamples];
int status = WL_IDLE_STATUS;

WiFiClient wifi;
HttpClient client(wifi, serverIP, port);

BLEService DataService(serviceUUID);
BLECharacteristic DataCharacteristic(characteristicUUID, BLERead | BLEWrite, 256);

void setup() {
    EEPROM.begin(512);
    Serial.begin(9600);
    Serial.println("Setup Start");

    byte bssid[6];
    WiFi.BSSID(bssid);

    bufferSize = targetSamples;
    sampleBuffer = new short[bufferSize];

    PDM.onReceive(onPDMdata);
    if (!PDM.begin(channels, frequency)) {
        Serial.println("Failed to start PDM!");
        while (true) {}
    }

    Serial.println("1");

    if (strlen(ssid) > 0 && strlen(pass) > 0) {
        Serial.println("3");
        connectToWiFi();
        BLE.end();
        Serial.println("4");
    } else {
        Serial.println("5");
        if (!BLE.begin()) {
            Serial.println("Starting Bluetooth® Low Energy failed!");
            while (true) {}
        }

        DataService.addCharacteristic(DataCharacteristic);
        BLE.setLocalName(macAddress);
        BLE.setAdvertisedService(DataService);
        BLE.addService(DataService);
        BLE.advertise();
        Serial.println("BLE " + String(macAddress) + ", waiting for connections...");
    }

    neuton_nn_setup();
}

void loop() {
    if (strlen(ssid) > 0 && strlen(pass) > 0) {
        connectToWiFi();
        BLE.end();
        if (bufferIndex >= targetSamples) {
            Serial.println("9");
            makePrediction();
            bufferIndex = 0;
        }
    } else {

        BLEDevice central = BLE.central();
        if (central) {
            Serial.println("Connected to BLE device: " + central.address());
            if (DataCharacteristic.written()) {
                int length = DataCharacteristic.valueLength();
                if (length > 0) {
                    const uint8_t* data = DataCharacteristic.value();
                    String receivedValue = "";
                    for (int i = 0; i < length; i++) {
                        receivedValue += (char)data[i];
                    }
                    Serial.println("Received: " + receivedValue);
                    storeStringToEEPROM(receivedValue);
                    int delimiterIndexResult = receivedValue.indexOf(':');
                    if (delimiterIndexResult != -1) {
                        receivedValue.substring(0, delimiterIndexResult).toCharArray(ssid, sizeof(ssid));
                        receivedValue.substring(delimiterIndexResult + 1).toCharArray(pass, sizeof(pass));
                    }
                }
            }
        }
    }
    delay(250);
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

void makePrediction() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        connectToWiFi();
    }

    for (int i = 0; i < bufferIndex; i++) {
        raw_inputs[i] = sampleBuffer[i];
    }
    neuton_input_features_t* p_input = neuton_nn_feed_inputs(raw_inputs, neuton_nn_uniq_inputs_num());

    neuton_u16_t predicted_target;
    const neuton_output_t* probabilities;
    neuton_i16_t outputs_num = neuton_nn_run_inference(p_input, &predicted_target, &probabilities);

    String targetLabel;
    switch (predicted_target) {
        case 0:
            targetLabel = "glass";
            break;
        case 1:
            targetLabel = "background";
            break;
        case 2:
            targetLabel = "alarm";
            break;
        default:
            targetLabel = "unknown";
            break;
    }

    if (targetLabel == "background") {
        nbrBackground++;
    }

    if (nbrBackground == 5 || lastStored != targetLabel) {
        nbrBackground = 0;
        String jsonMessage = "{";
        jsonMessage += "\"prediction\": \"" + targetLabel + "\",";
        jsonMessage += "\"device\": \"" + String(macAddress) + "\"";
        jsonMessage += "}";

        Serial.println("Sending JSON:");
        Serial.println(jsonMessage);

        client.beginRequest();
        client.post("/prediction", "application/json", jsonMessage);

        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        Serial.println("Status code: " + String(statusCode));
        Serial.println("Response: " + response);
        client.endRequest();

        lastStored = targetLabel;
    }
}

void connectToWiFi() {
    if (strlen(ssid) == 0 || strlen(pass) == 0) {
        Serial.println("SSID or password not set. Cannot connect to WiFi.");
        return;
    }

    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    while (status != WL_CONNECTED) {
        delay(1000);
        status = WiFi.status();
        Serial.print(".");
    }

    if (status == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    } else {
        Serial.println("Failed to connect to WiFi");
    }
}

void storeStringToEEPROM(const String& message) {
    int address = 0;
    int length = message.length();

    EEPROM.write(address, length);
    address += 1;

    for (int i = 0; i < length; i++) {
        EEPROM.write(address + i, message[i]);
    }
    address += length;

    EEPROM.write(address, '\0');
    EEPROM.commit();

    Serial.println("Stored message to EEPROM.");
}

String readStringFromEEPROM() {
    int address = 0;
    int length = EEPROM.read(address);
    address += 1;

    char readStr[length + 1];
    for (int i = 0; i < length; i++) {
        readStr[i] = EEPROM.read(address + i);
    }
    readStr[length] = '\0';

    return String(readStr);
}
