/*
    This program reads an MPR121 touch sensor events and triggers audio
    playback from an SD card via a VS1052 audio chip.

    Copyright © 2023 Tim Clark

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// include SPI, MP3, SD and touch libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>

// Used to get bit values out of a word
#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

// lower numbers == louder volume!
#define VOLUME 20

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

// Maximum number of files to index
#define MAXFILES    12
// Store of found files
uint8_t fileCount = 0;
String files[MAXFILES];

void setup() {
  Serial.begin(9600);
  
  Serial.println("Quilt Player");

  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));
  
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  // Set volume for left, right channels.
  musicPlayer.setVolume(VOLUME, VOLUME);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  // Scan SD card for playable files and insert sort them
  File dir = SD.open("/");
  while(true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    if (!entry.isDirectory()) {
      String filename = String(entry.name());
      if(filename.endsWith(".MP3")){
        bool inserted = false;
        // Insert into correct place in list
        for(uint8_t checkFileNo=0; checkFileNo < fileCount; checkFileNo++){
          if(filename.compareTo(files[checkFileNo])<0){
            // make space in array
            if(fileCount<MAXFILES){
              fileCount++;
            }
            for(int8_t moveFileNo = (fileCount-1); moveFileNo > checkFileNo; moveFileNo--){
              files[moveFileNo]=files[moveFileNo-1];
            }
            files[checkFileNo] = filename;
            inserted=true;
            break;
          }
        }
        // At the end of the list, insert if still space
        if((!inserted) && (fileCount<MAXFILES)){
          files[fileCount]=filename;
          fileCount++;
        }
      }
    }
    entry.close(); 
  }
  // Print the found filenames
  for(uint8_t fileNo=0; fileNo < fileCount; fileNo++) {
    Serial.print(fileNo,DEC);
    Serial.print(": ");
    Serial.println(files[fileNo]);
  }
  
}

void loop() {
  // Get the currently touched pads
  currtouched = cap.touched();
  
  for (uint8_t i=0; i<fileCount; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println("i touched");
      // If not already playing play new file
      if(musicPlayer.stopped()){
        Serial.println(files[i].c_str());
        musicPlayer.startPlayingFile(files[i].c_str());
      }
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" released");
    }
  }

  // reset our state
  lasttouched = currtouched;

  // comment out this line for detailed data from the sensor!
  return;
}
