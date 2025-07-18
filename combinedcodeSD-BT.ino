/*
  Model Rocket Motor Test Stand Code with Bluetooth
  Features:
  - HX711 load cell integration
  - Calibration with weight
  - Manual calibration value adjustment
  - Data logging (time, force)
  - SD card logging
  - Bluetooth serial communication on pins 2(RX) and 3(TX)
  - IGNITION CONTROL REMOVED TO SAVE MEMORY
*/

#include <HX711_ADC.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>

// HX711 pins
const int HX711_dout = 8; // MCU > HX711 dout pin
const int HX711_sck = 9;  // MCU > HX711 sck pin

// SD card pins
const int SD_CS = 10;     // Chip Select pin for SD card

// Bluetooth module pins
const int BT_RX = 3;      // Arduino TX -> BT RX
const int BT_TX = 2;      // Arduino RX <- BT TX

// Software Serial for Bluetooth
SoftwareSerial BTSerial(BT_TX, BT_RX); // RX, TX

// HX711 constructor
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// SD card file
File dataFile;

// EEPROM address for calibration value
const int calVal_eepromAdress = 0;

// Variables for thrust measurement
float mtime = 0;       // Current time in seconds
float force = 0;       // Force in Newtons
float last = 0;        // Last time measurement
bool testRunning = false;

// Timing variables
unsigned long t = 0;
const int serialPrintInterval = 20; // Update interval in ms (increased to save processing)

// SD card status
bool sdCardPresent = false;

// Operating mode (using byte instead of enum to save memory)
#define MODE_STANDBY 0
#define MODE_CALIBRATION 1
#define MODE_MEASUREMENT 2
byte currentMode = MODE_STANDBY;

// Flag for serial plotter mode
bool plotterMode = false;

// Test filename counter
int fileCounter = 0;
char currentFileName[13]; // 8.3 filename format (12 chars + null terminator)

// Bluetooth communication enabled flag
bool btEnabled = true;

void setup() {
  Serial.begin(9600);
  // Initialize Bluetooth serial
  BTSerial.begin(9600); // Most BT modules use 9600 baud by default
  
  delay(10);
  
  Serial.println(F("Model Rocket Test Stand with Bluetooth"));
  BTSerial.println(F("Model Rocket Test Stand with Bluetooth"));
  
  // Initialize SD card
  Serial.print(F("Initializing SD card..."));
  if (SD.begin(SD_CS)) {
    Serial.println(F("SD card present"));
    BTSerial.println(F("SD card present"));
    sdCardPresent = true;
    
    // Find next available file number
    while (SD.exists(getFileName(fileCounter))) {
      fileCounter++;
    }
    Serial.print(F("Next file: TEST"));
    Serial.print(fileCounter);
    Serial.println(F(".CSV"));
    
    BTSerial.print(F("Next file: TEST"));
    BTSerial.print(fileCounter);
    BTSerial.println(F(".CSV"));
  } else {
    Serial.println(F("SD card failed/missing"));
    BTSerial.println(F("SD card failed/missing"));
    sdCardPresent = false;
  }
  
  // Initialize load cell
  LoadCell.begin();
  float calibrationValue;
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  
  // Check if there is a valid calibration value in EEPROM
  if (isnan(calibrationValue) || calibrationValue == 0) {
    calibrationValue = 217.84; // Default value if EEPROM is empty
  }
  
  // Set calibration value
  LoadCell.setCalFactor(calibrationValue);
  
  // Start up the load cell
  long stabilizingtime = 2000;
  bool _tare = false;
  LoadCell.start(stabilizingtime, _tare);
  
  // Check if the HX711 is properly connected
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println(F("Timeout, check wiring"));
    BTSerial.println(F("Timeout, check wiring"));
    while (1);
  }
  else {
    Serial.println(F("Test Stand Ready"));
    Serial.print(F("Cal factor: "));
    Serial.println(calibrationValue);
    
    BTSerial.println(F("Test Stand Ready"));
    BTSerial.print(F("Cal factor: "));
    BTSerial.println(calibrationValue);
  }
  
  // Wait for load cell to stabilize
  while (!LoadCell.update());
  
  // Print menu
  printMenu();
}

// Function to generate filenames consistently
const char* getFileName(int counter) {
  static char filename[13];
  sprintf(filename, "TEST%d.CSV", counter);
  return filename;
}

void loop() {
  // Update load cell data
  static bool newDataReady = 0;
  if (LoadCell.update()) newDataReady = true;
  
  // Process new data
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      newDataReady = 0;
      t = millis();
      
      // Get current time and force
      mtime = micros() / 1000000.0;
      force = LoadCell.getData() * 0.009806; // Convert grams to Newtons
      
      // Log data to serial in measurement mode
      if (currentMode == MODE_MEASUREMENT && testRunning) {
        float grams = LoadCell.getData(); // Raw data in grams
        
        // Standard formatted output (when not plotting)
        if (!plotterMode) {
          Serial.print(mtime, 3);  // Reduced precision to save memory
          Serial.print(F(","));
          Serial.print(grams, 2);  // Reduced precision
          Serial.print(F(","));
          Serial.println(force, 3); // Reduced precision
          
          // Send the same data over Bluetooth
          if (btEnabled) {
            BTSerial.print(mtime, 3);
            BTSerial.print(F(","));
            BTSerial.print(grams, 2);
            BTSerial.print(F(","));
            BTSerial.println(force, 3);
          }
        } 
        // Plotter-compatible output (just values)
        else {
          Serial.print(F("g:"));
          Serial.print(grams);
          Serial.print(F(" f:"));
          Serial.println(force);
          
          // Send plotter data over Bluetooth
          if (btEnabled) {
            BTSerial.print(F("g:"));
            BTSerial.print(grams);
            BTSerial.print(F(" f:"));
            BTSerial.println(force);
          }
        }
        
        // Write to SD card if available
        if (sdCardPresent && dataFile) {
          dataFile.print(mtime, 3);
          dataFile.print(F(","));
          dataFile.print(grams, 2);
          dataFile.print(F(","));
          dataFile.println(force, 3);
          
          // Flush every 20 data points to prevent data loss but save processing time
          static byte flushCounter = 0;
          if (++flushCounter >= 20) {
            dataFile.flush();
            flushCounter = 0;
          }
        }
        
        last = mtime;
      }
    }
  }
  
  // Check for serial commands from USB
  if (Serial.available() > 0) {
    char command = Serial.read();
    processCommand(command);
  }
  
  // Check for serial commands from Bluetooth
  if (BTSerial.available() > 0) {
    char command = BTSerial.read();
    processCommand(command);
    // Echo the command back to BT to confirm receipt
    BTSerial.print(F("Received command: "));
    BTSerial.println(command);
  }
  
  // Check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println(F("Tare complete"));
    BTSerial.println(F("Tare complete"));
  }
}

void processCommand(char command) {
  switch (command) {
    case 't': // Tare
      LoadCell.tareNoDelay();
      Serial.println(F("Taring..."));
      BTSerial.println(F("Taring..."));
      break;
      
    case 'r': // Run calibration
      currentMode = MODE_CALIBRATION;
      calibrate();
      break;
      
    case 'c': // Change calibration value manually
      changeSavedCalFactor();
      break;
      
    case 's': // Start measurement
      startMeasurement();
      break;
      
    case 'x': // Stop measurement
      stopMeasurement();
      break;
      
    case 'm': // Show menu
      printMenu();
      break;
      
    case 'p': // Toggle plotter mode
      plotterMode = !plotterMode;
      Serial.println(plotterMode ? F("Plotter ON") : F("Plotter OFF"));
      BTSerial.println(plotterMode ? F("Plotter ON") : F("Plotter OFF"));
      break;
      
    case 'd': // Test SD card
      testSDCard();
      break;
      
    case 'b': // Toggle Bluetooth
      btEnabled = !btEnabled;
      Serial.println(btEnabled ? F("Bluetooth ON") : F("Bluetooth OFF"));
      if (btEnabled) {
        BTSerial.println(F("Bluetooth ON"));
      }
      break;
  }
}

void printMenu() {
  Serial.println(F("\n==== ROCKET TEST STAND ===="));
  Serial.println(F("Commands:"));
  Serial.println(F("t - Tare scale to zero"));
  Serial.println(F("r - Run calibration"));
  Serial.println(F("c - Change calibration factor"));
  Serial.println(F("s - Start measurement"));
  Serial.println(F("x - Stop measurement"));
  Serial.println(F("p - Toggle plotter mode"));
  Serial.println(F("d - Test SD card"));
  Serial.println(F("b - Toggle Bluetooth"));
  Serial.println(F("m - Show menu"));
  Serial.println(F("=========================="));
  
  // Send the same menu over Bluetooth if enabled
  if (btEnabled) {
    BTSerial.println(F("\n==== ROCKET TEST STAND ===="));
    BTSerial.println(F("Commands:"));
    BTSerial.println(F("t - Tare scale to zero"));
    BTSerial.println(F("r - Run calibration"));
    BTSerial.println(F("c - Change calibration factor"));
    BTSerial.println(F("s - Start measurement"));
    BTSerial.println(F("x - Stop measurement"));
    BTSerial.println(F("p - Toggle plotter mode"));
    BTSerial.println(F("d - Test SD card"));
    BTSerial.println(F("b - Toggle Bluetooth"));
    BTSerial.println(F("m - Show menu"));
    BTSerial.println(F("=========================="));
  }
}

void startMeasurement() {
  // Reset measurement variables
  mtime = micros() / 1000000.0;
  last = mtime;
  
  if (!plotterMode) {
    Serial.println(F("MEASUREMENT STARTED"));
    Serial.println(F("Time(s),Weight(g),Force(N)"));
    
    if (btEnabled) {
      BTSerial.println(F("MEASUREMENT STARTED"));
      BTSerial.println(F("Time(s),Weight(g),Force(N)"));
    }
  }
  
  // Open a new file on SD card if available
  if (sdCardPresent) {
    // Create a new file name
    strcpy(currentFileName, getFileName(fileCounter));
    dataFile = SD.open(currentFileName, FILE_WRITE);
    
    if (dataFile) {
      Serial.print(F("Logging to: "));
      Serial.println(currentFileName);
      
      if (btEnabled) {
        BTSerial.print(F("Logging to: "));
        BTSerial.println(currentFileName);
      }
      
      // Write header to file
      dataFile.println(F("Time(s),Weight(g),Force(N)"));
      dataFile.flush();
      
      // Increment file counter for next test
      fileCounter++;
    } else {
      Serial.println(F("Error opening file!"));
      if (btEnabled) BTSerial.println(F("Error opening file!"));
    }
  } else {
    Serial.println(F("SD card not available"));
    if (btEnabled) BTSerial.println(F("SD card not available"));
  }
  
  currentMode = MODE_MEASUREMENT;
  testRunning = true;
}

void stopMeasurement() {
  testRunning = false;
  
  // Close the data file if open
  if (dataFile) {
    dataFile.close();
    Serial.print(F("Data saved to "));
    Serial.println(currentFileName);
    
    if (btEnabled) {
      BTSerial.print(F("Data saved to "));
      BTSerial.println(currentFileName);
    }
  }
  
  if (!plotterMode) {
    Serial.println(F("MEASUREMENT STOPPED"));
    if (btEnabled) BTSerial.println(F("MEASUREMENT STOPPED"));
  }
  
  currentMode = MODE_STANDBY;
}

void calibrate() {
  Serial.println(F("*** CALIBRATION ***"));
  Serial.println(F("Remove any load."));
  Serial.println(F("Send 't' to tare."));

  if (btEnabled) {
    BTSerial.println(F("*** CALIBRATION ***"));
    BTSerial.println(F("Remove any load."));
    BTSerial.println(F("Send 't' to tare."));
  }

  bool _resume = false;
  while (_resume == false) {
    LoadCell.update();
    
    // Check for commands from both Serial and BTSerial
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') {
        LoadCell.tareNoDelay();
      }
    }
    
    if (btEnabled && BTSerial.available() > 0) {
      char inByte = BTSerial.read();
      if (inByte == 't') {
        LoadCell.tareNoDelay();
      }
    }
    
    if (LoadCell.getTareStatus() == true) {
      Serial.println(F("Tare complete"));
      if (btEnabled) BTSerial.println(F("Tare complete"));
      _resume = true;
    }
  }

  Serial.println(F("Place calibration weight"));
  Serial.println(F("Send weight in grams"));
  
  if (btEnabled) {
    BTSerial.println(F("Place calibration weight"));
    BTSerial.println(F("Send weight in grams"));
  }

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    
    // Check for input from both Serial and BTSerial
    if (Serial.available() > 0) {
      known_mass = Serial.parseFloat();
      if (known_mass != 0) {
        Serial.print(F("Known mass: "));
        Serial.println(known_mass);
        if (btEnabled) {
          BTSerial.print(F("Known mass: "));
          BTSerial.println(known_mass);
        }
        _resume = true;
      }
    }
    
    if (btEnabled && BTSerial.available() > 0) {
      known_mass = BTSerial.parseFloat();
      if (known_mass != 0) {
        Serial.print(F("Known mass: "));
        Serial.println(known_mass);
        BTSerial.print(F("Known mass: "));
        BTSerial.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); // Refresh dataset for accuracy
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

  Serial.print(F("New cal value: "));
  Serial.println(newCalibrationValue);
  Serial.print(F("Save to EEPROM? (y/n): "));
  
  if (btEnabled) {
    BTSerial.print(F("New cal value: "));
    BTSerial.println(newCalibrationValue);
    BTSerial.print(F("Save to EEPROM? (y/n): "));
  }

  _resume = false;
  while (_resume == false) {
    // Check for response from both Serial and BTSerial
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        Serial.print(F("Value saved: "));
        Serial.println(newCalibrationValue);
        if (btEnabled) {
          BTSerial.print(F("Value saved: "));
          BTSerial.println(newCalibrationValue);
        }
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println(F("Value not saved"));
        if (btEnabled) BTSerial.println(F("Value not saved"));
        _resume = true;
      }
    }
    
    if (btEnabled && BTSerial.available() > 0) {
      char inByte = BTSerial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        Serial.print(F("Value saved: "));
        Serial.println(newCalibrationValue);
        BTSerial.print(F("Value saved: "));
        BTSerial.println(newCalibrationValue);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println(F("Value not saved"));
        BTSerial.println(F("Value not saved"));
        _resume = true;
      }
    }
  }

  // Set the new calibration value
  LoadCell.setCalFactor(newCalibrationValue);
  
  Serial.println(F("Calibration complete"));
  if (btEnabled) BTSerial.println(F("Calibration complete"));
  
  currentMode = MODE_STANDBY;
  printMenu();
}

void changeSavedCalFactor() {
  float currentCalibrationValue = LoadCell.getCalFactor();
  
  Serial.println(F("*** MANUAL CALIBRATION ***"));
  Serial.print(F("Current: "));
  Serial.println(currentCalibrationValue);
  Serial.println(F("Enter new value:"));
  
  if (btEnabled) {
    BTSerial.println(F("*** MANUAL CALIBRATION ***"));
    BTSerial.print(F("Current: "));
    BTSerial.println(currentCalibrationValue);
    BTSerial.println(F("Enter new value:"));
  }

  bool _resume = false;
  float newCalibrationValue;
  
  while (_resume == false) {
    // Check for input from both Serial and BTSerial
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print(F("New value: "));
        Serial.println(newCalibrationValue);
        if (btEnabled) {
          BTSerial.print(F("New value: "));
          BTSerial.println(newCalibrationValue);
        }
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
    
    if (btEnabled && BTSerial.available() > 0) {
      newCalibrationValue = BTSerial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print(F("New value: "));
        Serial.println(newCalibrationValue);
        BTSerial.print(F("New value: "));
        BTSerial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  
  Serial.print(F("Save to EEPROM? (y/n): "));
  if (btEnabled) BTSerial.print(F("Save to EEPROM? (y/n): "));
  
  _resume = false;
  while (_resume == false) {
    // Check for response from both Serial and BTSerial
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        Serial.print(F("Value saved: "));
        Serial.println(newCalibrationValue);
        if (btEnabled) {
          BTSerial.print(F("Value saved: "));
          BTSerial.println(newCalibrationValue);
        }
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println(F("Value not saved"));
        if (btEnabled) BTSerial.println(F("Value not saved"));
        _resume = true;
      }
    }
    
    if (btEnabled && BTSerial.available() > 0) {
      char inByte = BTSerial.read();
      if (inByte == 'y') {
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
        Serial.print(F("Value saved: "));
        Serial.println(newCalibrationValue);
        BTSerial.print(F("Value saved: "));
        BTSerial.println(newCalibrationValue);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println(F("Value not saved"));
        BTSerial.println(F("Value not saved"));
        _resume = true;
      }
    }
  }
  
  currentMode = MODE_STANDBY;
  printMenu();
}

void testSDCard() {
  Serial.println(F("*** TESTING SD CARD ***"));
  if (btEnabled) BTSerial.println(F("*** TESTING SD CARD ***"));
  
  // Check if SD card is initialized
  if (!sdCardPresent) {
    Serial.println(F("SD card not initialized. Trying..."));
    if (btEnabled) BTSerial.println(F("SD card not initialized. Trying...")); 
    
    if (SD.begin(SD_CS)) {
      Serial.println(F("SD card initialized!"));
      if (btEnabled) BTSerial.println(F("SD card initialized!"));
      sdCardPresent = true;
    } else {
      Serial.println(F("SD card failed! Check wiring."));
      if (btEnabled) BTSerial.println(F("SD card failed! Check wiring."));
      return;
    }
  } else {
    Serial.println(F("SD card is present."));
    if (btEnabled) BTSerial.println(F("SD card is present."));
  }
  
  // Test write/read operations
  const char testFileName[] = "TEST_SD.TXT";
  Serial.print(F("Creating test file: "));
  Serial.println(testFileName);
  if (btEnabled) {
    BTSerial.print(F("Creating test file: "));
    BTSerial.println(testFileName);
  }
  
  // Try to create and write to a test file
  File testFile = SD.open(testFileName, FILE_WRITE);
  if (testFile) {
    testFile.println(F("SD Card Test File"));
    testFile.println(F("If you can read this, SD card works!"));
    testFile.close();
    Serial.println(F("Test file written."));
    if (btEnabled) BTSerial.println(F("Test file written."));
    
    // Try to read the file back
    testFile = SD.open(testFileName);
    if (testFile) {
      Serial.println(F("Reading test file:"));
      Serial.println(F("---------------------"));
      if (btEnabled) {
        BTSerial.println(F("Reading test file:"));
        BTSerial.println(F("---------------------"));
      }
      
      while (testFile.available()) {
        char c = testFile.read();
        Serial.write(c);
        if (btEnabled) BTSerial.write(c);
      }
      
      Serial.println(F("\n---------------------"));
      if (btEnabled) BTSerial.println(F("\n---------------------"));
      testFile.close();
      
      // List root directory content
      Serial.println(F("Files on SD card:"));
      if (btEnabled) BTSerial.println(F("Files on SD card:"));
      
      File root = SD.open("/");
      printDirectory(root, 0);
      root.close();
      
      Serial.println(F("SD card test successful!"));
      if (btEnabled) BTSerial.println(F("SD card test successful!"));
    } else {
      Serial.println(F("Error reading test file!"));
      if (btEnabled) BTSerial.println(F("Error reading test file!"));
    }
  } else {
    Serial.println(F("Error creating test file!"));
    if (btEnabled) BTSerial.println(F("Error creating test file!"));
  }
}

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    
    // Print filename with tabs for hierarchy
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
      if (btEnabled) BTSerial.print('\t');
    }
    
    Serial.print(entry.name());
    if (btEnabled) BTSerial.print(entry.name());
    
    if (entry.isDirectory()) {
      Serial.println(F("/"));
      if (btEnabled) BTSerial.println(F("/"));
      printDirectory(entry, numTabs + 1);
    } else {
      // Print file size
      Serial.print(F("\t\t"));
      Serial.print(entry.size(), DEC);
      Serial.println(F(" bytes"));
      
      if (btEnabled) {
        BTSerial.print(F("\t\t"));
        BTSerial.print(entry.size(), DEC);
        BTSerial.println(F(" bytes"));
      }
    }
    
    entry.close();
  }
}