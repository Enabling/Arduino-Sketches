/****
    Status Controller device

    Sending a container type/value every 2 minutes
    to privide our backend with data to determine
    API/Network status.

    POSSIBLE IMPROVEMENTS :

      - After send, store new container ID en timestamp in persistant memory !
        -> in startup, fetch from there ....

      - Battery level calculation ... connect on other port ?!


*/
#include <string.h>

// LoRa related
#include <ATT_LoRa_IOT.h>
#include <MicrochipLoRaModem.h>
#include "keys.h"
#define WAIT_FOR_ACK false

// SD related
#include <SD.h>
#include <SPI.h>
//Digital pin 11 is the MicroSD slave select pin on the Mbili
#define SD_SS_PIN 11


// Battery related
#define ADC_AREF 3.3
#define BATVOLTPIN A6
#define BATVOLT_R1 4.7
#define BATVOLT_R2 10

//serial monitor
#define SERIAL_BAUD 57600

#define CSV_FILE "meteo.csv"
#define MAX_LINE_LENGTH 80
#define SECONDS_BETWEEN_SENDS 30

File file;
const unsigned long sendInterval = 30000; // 30 seconds
unsigned long lastSentAt = 0;

bool led2On = 0;

short valContainerID;
float valTemperatureSensor = 0.0; // unit: °C  minimum: −273.15
float valLightSensor = 0.0;       // unit: lux  minimum: 0.0
float valPressureSensor = 0.0;    // unit: hPa  minimum: 300.0  maximum: 1100.0
float valHumiditySensor = 0.0;    // unit: RH (relative humidity)  minimum: 0.0  maximum: 100.0
short valAirQualitySensor = 0;    // unit: --  minimum: 0  maximum: 1024


MicrochipLoRaModem Modem(&Serial1);
ATTDevice Device(&Modem);

void setup() {

  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, LOW);


  Serial.begin(SERIAL_BAUD);                   // set baud rate of the default serial debug connection
  Serial1.begin(Modem.getDefaultBaudRate());   // set baud rate of the serial connection between Mbili and LoRa modem

  while (!Device.Connect(DEV_ADDR, APPSKEY, NWKSKEY))           //there is no point to continue if we can't connect to the cloud: this device's main purpose is to send data to the cloud.
  {
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, HIGH);
    delay(2500);
    Serial.println("retrying to connect to LoRa...");
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, LOW );
  }
  Device.SetMaxSendRetry(0);

  // Init SD / File access
  while (!SD.begin(SD_SS_PIN)) {
    Serial.println("SD init error. Please insert SD card containing needed CSV file!!");
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    delay(1000);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
  }


  Serial.println("Ready to send data");
  doYourThing();
}

void loop() {
  digitalWrite(LED1, HIGH);

  if (openCSVFile()) {
//    timedCallback(0);
    while (file.available()) {
//      timer.update();
            readAndSendData();
            throttle();
    }
  }
  digitalWrite(LED1, LOW);
  delay(30000);//30 second wait between iterations.
}

void throttle() {
  Serial.print ("Backing off for ");
  Serial.print (SECONDS_BETWEEN_SENDS);
  Serial.println(" s.");
  delay(SECONDS_BETWEEN_SENDS * 1000);
}

void toggleLED2() {
  led2On = 1 - led2On;
  digitalWrite(LED2, led2On ? HIGH : LOW);
}


// need to connect battery through data connection ...'BATVOLTPIN'
float getBatteryVoltageValue() {
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  Serial.print("Battery analog read = ");
  Serial.println(batteryVoltage);
  return 1000 * ((ADC_AREF / 1023.0) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage);
}


void showConsoleMessage() {
  long remaining = (long) (sendInterval - (millis() - lastSentAt)) / 1000;
  Serial.print("Still going strong ... Next update in ");
  Serial.print(remaining);
  Serial.println(" s.");
}

void doYourThing() {
}


void sendContainerData(float floatData, short sensor) {
  digitalWrite(LED1, HIGH);

  Device.Send(floatData, sensor, WAIT_FOR_ACK);
  Serial.print("Data (val=");
  Serial.print(floatData);
  Serial.println(") sent on the LoRa network.");

  digitalWrite(LED1, LOW);
}
void sendContainerData(short shortData, short sensor) {
  digitalWrite(LED1, HIGH);

  Device.Send(shortData, sensor, WAIT_FOR_ACK);
  Serial.print("Data (val=");
  Serial.print(shortData);
  Serial.println(") sent on the LoRa network.");

  digitalWrite(LED1, LOW);
}


bool openCSVFile() {
  file = SD.open(CSV_FILE, FILE_READ);
  if (!file) {
    Serial.print("error opening CSV file '");
    Serial.print(CSV_FILE);
    Serial.println("'");
    return false;
  }
  return true;
}

bool readLine(char* line) {
  for (size_t n = 0; n < MAX_LINE_LENGTH; n++) {
    int c = file.read();
    if ( c < 0 && n == 0) return false;  // EOF
    if (c < 0 || c == '\n') {
      line[n] = 0;
      return true;
    }
    line[n] = c;
  }
  return false; // line too long
}

bool readAndSendData() {
  Serial.println("Reading/parsing new line of data ...");
  char line[MAX_LINE_LENGTH];
  if (!readLine(line)) {
    return false;  // EOF or too long
  }
  char* command = strtok(line, ",");
  if (command != 0)  {
    valContainerID = (short) strtol(command, NULL,10);
    command = strtok(NULL, ",");
    switch (valContainerID) {
      case TEMPERATURE_SENSOR:
        valTemperatureSensor = strtod(command, NULL);
        sendContainerData(valTemperatureSensor,TEMPERATURE_SENSOR);
        break;
      case LIGHT_SENSOR:
        valLightSensor = strtod(command, NULL);
        sendContainerData(valLightSensor,LIGHT_SENSOR);
        break;
      case PRESSURE_SENSOR:
        valPressureSensor = strtod(command, NULL);
        sendContainerData(valPressureSensor,PRESSURE_SENSOR);
        break;
      case HUMIDITY_SENSOR:
        valHumiditySensor = strtod(command, NULL);
        sendContainerData(valHumiditySensor,HUMIDITY_SENSOR);
        break;
      case AIR_QUALITY_SENSOR:
        valAirQualitySensor = (short) strtol(command, NULL,10);
        sendContainerData(valAirQualitySensor,AIR_QUALITY_SENSOR);
        break;
      default:
        Serial.println("Reading/parsing new line of data FAILED, WRONG CONTAINER TYPE !!!");
    }
  }else{
    Serial.println("ERROR Reading/parsing data ...");
  }
}
