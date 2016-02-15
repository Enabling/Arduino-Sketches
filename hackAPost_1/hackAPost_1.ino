/****
    Hack A Post

    Replay GPS data from CSV file on SD

    CSV structure :

      Timestamp, Latitude, Longtitude, Altitude


    Make replay speed configurable
    ??-> replay at normal speed (using timestamps from file) or speedup/down
    ??-> replay at set interval/delay

*/

#include <string.h>

//#include <Sodaq_DS3231.h>
//#include <RTCTimer.h>   // Install library !!!

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


// Serial monitor related
#define SERIAL_BAUD 57600

#define CSV_FILE "bike.csv"
#define MAX_LINE_LENGTH 80
#define SECONDS_BETWEEN_SENDS 15
#define MSECONDS_BETWEEN_SENDS 15000


float valGPSTimestamp, valGPSLatitude, valGPSLongitude, valGPSAltitude;
File file;
//RTCTimer timer;

// Modem & device
MicrochipLoRaModem Modem(&Serial1);
ATTDevice Device(&Modem);

void setup() {

  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, LOW);


  Serial.begin(SERIAL_BAUD);                   // set baud rate of the default serial debug connection
  //Serial1.begin(19200);   // set baud rate of the serial connection between Mbili and LoRa modem
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

  //setupTimer();

  Serial.println("Ready to playback/send data");
}

//void setupTimer() {
//  //Instruct the RTCTimer how to get the current time reading
//  timer.setNowCallback(getNow);
//  //Schedule the reading every xxxxx seconds
//  timer.every(MSECONDS_BETWEEN_SENDS, timedCallback);
//}
//uint32_t getNow() {
//  return millis();
//}

/*
   Open the file
   for each line
    -> parse data
    -> send GPS over LORA
    -> depending on running speed // pause
    -> file done ==> turn OFF LED pause for .....
    -> RESTART ?!
*/
void loop() {
  digitalWrite(LED1, HIGH);

  if (openCSVFile()) {
    // timedCallback(0);
    while (file.available()) {
//      timer.update();
            readAndParseLine();
            sendGPS();
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

//void timedCallback(uint32_t ts) {
//  readAndParseLine();
//  sendGPS();
//}

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

bool readAndParseLine() {
  Serial.println("Reading/parsing new line of data ...");
  char line[MAX_LINE_LENGTH];
  if (!readLine(line)) {
    return false;  // EOF or too long
  }
  char* command = strtok(line, ",");
  int valCnt = 0;
  while (command != 0)
  {
    switch (valCnt) {
      case 0:
        valGPSTimestamp = strtod(command, NULL);
        break;
      case 1:
        valGPSLongitude = strtod(command, NULL);
        break;
      case 2:
        valGPSLatitude = strtod(command, NULL);
        break;
      case 3:
        valGPSAltitude = strtod(command, NULL);
        break;
    }
    valCnt++;
    command = strtok(NULL, ",");
  }

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

void sendGPS() {
  Serial.println("Sending of GPS data ...");
  digitalWrite(LED2, HIGH);
  Device.Queue(valGPSLatitude);
  Device.Queue(valGPSLongitude);
  Device.Queue(valGPSAltitude);
  Device.Queue(valGPSTimestamp);
  Device.Send(GPS, WAIT_FOR_ACK);
  Serial.print("Sent 'GPS with values Lat = ");
  Serial.print(valGPSLatitude);
  Serial.print(" Long = ");
  Serial.print(valGPSLongitude);
  Serial.print(" Alt = ");
  Serial.print(valGPSAltitude);
  Serial.print(" TS = ");
  Serial.print(valGPSTimestamp);
  Serial.println("' ");
  digitalWrite(LED2, LOW);
}
