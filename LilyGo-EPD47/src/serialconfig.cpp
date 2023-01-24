#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include "serialconfig.h"

#define CMD_CONFIG_DONE "/config-done"
#define CMD_FILE_REMOVE "/file-remove"
#define CMD_FILE_DONE   "/file-done"
#define CMD_FILE_READ   "/file-read"
#define CMD_FILE_APPEND "/file-append"




String input = "";
String eat = "";
bool removeBeforeAppend = true;

// returns the string until the first space from input and truncates the input
void eatinput() {
  int index = input.indexOf(' ');
  if ( index == -1 ) {
    eat = input;
    input = "";
  } else {
    eat = input.substring(0,index);
    input = input.substring(index + 1);
  }  
}

// Parse commands from the serial input
void parseInput() {
  if ( input.length() == 0 ) {
    return;
  }

  eatinput();

  if ( eat == CMD_CONFIG_DONE ) {
    Serial.println(CMD_CONFIG_DONE);
    ESP.restart();
    return;  
  } 

  if ( eat == CMD_FILE_REMOVE ) {
    eatinput();
    Serial.println("removeFile: " + eat);
    SPIFFS.remove(PARAM_FILE);
    removeBeforeAppend = true;
    return;
  }

  if ( eat == CMD_FILE_APPEND ) {
    eatinput();
    
    Serial.println("appendToFile: " + eat);

    // make sure that file is removed before appended
    if ( removeBeforeAppend == false ) {
      SPIFFS.remove(PARAM_FILE);
      removeBeforeAppend = true;
    }

    File file = SPIFFS.open(PARAM_FILE, FILE_APPEND);
    if (!file) {
      file = SPIFFS.open(PARAM_FILE, FILE_WRITE);
    }
    if (file) {
      file.println(input);
      file.close();
    }
    
  }


  if ( eat == CMD_FILE_READ ) {
    eatinput();
    Serial.println("readFile: " + eat);
    File file = SPIFFS.open(PARAM_FILE);
    if (!file) {
      Serial.println("Could not open file for reading");
      return;
    }
    
    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.println("/file-read " + line);
    }
    file.close();
    Serial.println("");
    Serial.println("/file-done");
  }
}


void handleSerial() {
 while ( Serial.available() ) {
    char c = Serial.read();
    if ( c == '\n' ) {
      parseInput();      
      input = "";
    } else {
      input += c;
    }
  }
}
