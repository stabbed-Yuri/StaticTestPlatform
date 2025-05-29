/*
  ESP32 Rocket Test Stand
  SD Card Connections:
  - CS: GPIO 5
  - MOSI: GPIO 23
  - MISO: GPIO 19
  - SCK: GPIO 18
*/

#include <HX711_ADC.h>
#include <Preferences.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "BluetoothSerial.h"
#include <esp_pm.h>

BluetoothSerial bt;
Preferences p;
HX711_ADC lc(25, 26);
File df;

float t0=0, f=0;
bool run=0, sd=0, bt_on=1, plot=0;
byte m=0;
int fn=0;
char fn_buf[13];
unsigned long tm=0;
#define LED 2

void log_msg(const String& msg) {
  Serial.println(msg);
  if(bt_on) bt.println(msg);
}

void initSDCard() {
  log_msg("Initializing SD card...");
  if(!SD.begin(5)) {
    log_msg("SD Card Mount Failed! Check:");
    log_msg("- CS pin connection (GPIO 5)");
    log_msg("- SD card insertion");
    log_msg("- SPI connections (MOSI:23,MISO:19,SCK:18)");
    sd = false;
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    log_msg("No SD card attached!");
    sd = false;
    return;
  }

  String type = "SD Card Type: ";
  if(cardType == CARD_MMC) type += "MMC";
  else if(cardType == CARD_SD) type += "SDSC";
  else if(cardType == CARD_SDHC) type += "SDHC";
  else type += "UNKNOWN";
  log_msg(type);

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  String size = "SD Card Size: ";
  size += String(cardSize);
  size += "MB";
  log_msg(size);
  
  sd = true;
  log_msg("SD card initialization done.");
  
  // Find next available file number
  while(SD.exists(gn(fn))) fn++;
  String nextFile = "Next test file will be: ";
  nextFile += gn(fn);
  log_msg(nextFile);
}

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  log_msg("Starting ESP32 Rocket Test Stand...");
  
  // Configure DFS power management
  esp_pm_config_esp32_t pm_config = {
    .max_freq_mhz = 240, // Max CPU frequency (adjust as needed)
    .min_freq_mhz = 40,  // Min CPU frequency (adjust as needed)
    .light_sleep_enable = true // Allow light sleep when idle
  };
  esp_pm_configure(&pm_config);
  
  log_msg("Initializing Bluetooth...");
  bt.begin("RT");
  log_msg("Bluetooth initialized with name: RT");
  
  initSDCard();
  
  log_msg("Initializing load cell...");
  lc.begin();
  float calFactor = p.getFloat("c", 217.84);
  String calMsg = "Using calibration factor: ";
  calMsg += String(calFactor);
  log_msg(calMsg);
  lc.setCalFactor(calFactor);
  
  log_msg("Starting load cell...");
  lc.start(2000);
  log_msg("Waiting for first valid reading...");
  while(!lc.update());
  log_msg("Load cell initialized successfully");
  
  delay(100);
  mn();
}

void loop(){
  static bool nd=0;
  static bool ledState=0;
  if(lc.update())nd=1;

  if(nd&&millis()>tm+20){
    nd=0;
    tm=millis();
    t0=micros()/1e6;
    f=lc.getData()*.009806;

    if(m==1&&run){
      String d=String(t0,3)+","+String(lc.getData(),2)+","+String(f,3);
      if(!plot){
        Serial.println(d);
        if(bt_on)bt.println(d);
      }else{
        String p="g:"+String(lc.getData())+" f:"+String(f);
        Serial.println(p);
        if(bt_on)bt.println(p);
      }
      if(sd&&df){
        df.println(d);
        static byte c=0;
        if(++c>=20){
          df.flush();
          c=0;
          log_msg("Data flushed to SD card");
        }
      }
    }
    // LED blink logic based on HX711 input
    if(run){
      ledState = !ledState;
      digitalWrite(LED, ledState);
    } else {
      digitalWrite(LED, LOW);
    }
  }

  if(!run){
    digitalWrite(LED, LOW);
  }

  if(Serial.available())cm(Serial.read());
  if(bt.available())cm(bt.read());

  if(lc.getTareStatus()){
    log_msg("Tare completed successfully");
  }
}

void cm(char c){
  String cmd = "Received command: ";
  cmd += c;
  log_msg(cmd);
  
  switch(c){
    case't':
      log_msg("Starting tare operation...");
      lc.tareNoDelay();
      break;
    case'r':
      log_msg("Starting full calibration...");
      cl();
      break;
    case'c':
      log_msg("Starting manual calibration...");
      cc();
      break;
    case's':
      log_msg("Starting test...");
      st();
      break;
    case'x':
      log_msg("Stopping test...");
      sp();
      break;
    case'p':
      plot=!plot;
      log_msg(plot ? "Plot mode enabled" : "Plot mode disabled");
      break;
    case'd':
      log_msg("Testing SD card...");
      ts();
      break;
    case'b':
      bt_on=!bt_on;
      log_msg(bt_on ? "Bluetooth enabled" : "Bluetooth disabled");
      break;
    case'm':
      log_msg("Displaying menu...");
      mn();
      break;
  }
}

void st(){
  t0=micros()/1e6;
  if(sd) {
    // Try to reinitialize SD card if needed
    if(!SD.begin(5)) {
      log_msg("WARNING: SD card reinitialization needed");
      initSDCard();
      if(!sd) {
        log_msg("ERROR: SD card initialization failed!");
        return;
      }
    }

    strcpy(fn_buf, gn(fn));
    String fileMsg = "Creating new test file: ";
    fileMsg += fn_buf;
    log_msg(fileMsg);
    
    // First try to remove any existing file
    if(SD.exists(fn_buf)) {
      log_msg("Removing existing file...");
      SD.remove(fn_buf);
    }
    
    df = SD.open(fn_buf, FILE_WRITE);
    if(!df) {
      log_msg("ERROR: Failed to create data file!");
      log_msg("Check:");
      log_msg("- SD card write protection");
      log_msg("- Available space");
      log_msg("- File system errors");
      
      // Try to diagnose the issue
      uint64_t freeSpace = SD.totalBytes() - SD.usedBytes();
      String space = "Available space: ";
      space += String((int)(freeSpace / 1024));
      space += "KB";
      log_msg(space);
      
      // Try to write in root directory
      File test = SD.open("/test.txt", FILE_WRITE);
      if(!test) {
        log_msg("ERROR: Cannot write to root directory!");
    } else {
        test.close();
        SD.remove("/test.txt");
        log_msg("Root directory is writable");
      }
      return;
    }
    
    log_msg("Test file created successfully");
    if(!df.println("t,w,f")) {
      log_msg("ERROR: Failed to write header!");
      df.close();
      return;
    }
    df.flush();
    fn++;
  } else {
    log_msg("WARNING: SD card not available, proceeding without logging");
  }
  m=1;
  run=1;
  log_msg("Test started");
}

void sp(){
  run=0;
  if(df){
    df.close();
    String stopMsg = "Test stopped and file saved: ";
    stopMsg += fn_buf;
    log_msg(stopMsg);
  } else {
    log_msg("Test stopped (no file was open)");
  }
  m=0;
}

void cl(){
  log_msg("Starting full calibration procedure");
  log_msg("Remove all weight and send 't' when ready");
  
  while(!lc.getTareStatus()){
    lc.update();
    if(Serial.available()&&Serial.read()=='t')lc.tareNoDelay();
    if(bt.available()&&bt.read()=='t')lc.tareNoDelay();
  }
  
  log_msg("Tare complete. Place known weight and enter weight in grams:");
  
  while(!Serial.available()&&!bt.available())lc.update();
  float ms=Serial.available()?Serial.parseFloat():bt.parseFloat();
  if(ms<=0){
    log_msg("Invalid weight entered, calibration aborted");
    return;
  }
  
  log_msg("Calculating new calibration factor...");
  lc.refreshDataSet();
  float c=lc.getNewCalibration(ms);
  String calMsg = "Proposed calibration factor: ";
  calMsg += String(c);
  log_msg(calMsg);
  log_msg("Accept? (y/n)");
  
  while(!Serial.available()&&!bt.available())delay(10);
  if((Serial.available()&&Serial.read()=='y')||(bt.available()&&bt.read()=='y')){
    p.putFloat("c",c);
    lc.setCalFactor(c);
    String newCalMsg = "New calibration factor saved: ";
    newCalMsg += String(c);
    log_msg(newCalMsg);
  } else if((Serial.available()&&Serial.read()=='n')||(bt.available()&&bt.read()=='n')) {
    log_msg("Calibration cancelled");
  }
  m=0;
}

void cc(){
  String currentCal = "Current calibration factor: ";
  currentCal += String(lc.getCalFactor());
  log_msg(currentCal);
  log_msg("Enter new calibration factor:");
  
  while(!Serial.available()&&!bt.available())delay(10);
  float c=Serial.available()?Serial.parseFloat():bt.parseFloat();
  if(c<=0){
    log_msg("Invalid calibration factor, operation cancelled");
    return;
  }
  
  lc.setCalFactor(c);
  log_msg("Test new calibration factor. Accept? (y/n)");
  
  while(!Serial.available()&&!bt.available())delay(10);
  if((Serial.available()&&Serial.read()=='y')||(bt.available()&&bt.read()=='y')){
    p.putFloat("c",c);
    String newCalMsg = "New calibration factor saved: ";
    newCalMsg += String(c);
    log_msg(newCalMsg);
  } else {
    log_msg("Calibration change cancelled");
  }
  m=0;
}

void ts(){
  log_msg("Starting SD card test...");
  
  if(!sd && !SD.begin(5)) {
    log_msg("ERROR: SD initialization failed!");
    log_msg("Check SD card connections");
      return;
  }
  
  log_msg("Creating test file...");
  File f = SD.open("/test.txt", FILE_WRITE);
  if(!f) {
    log_msg("ERROR: File creation failed!");
    log_msg("Check:");
    log_msg("- SD card write protection");
    log_msg("- Available space");
    log_msg("- File system errors");
    return;
  }
  f.println("SD Card Test OK");
  f.close();
    
    // Try to read the file back
  f = SD.open("/test.txt");
  if(!f) {
    log_msg("ERROR: Failed to read test file!");
    return;
  }
  
  String content = f.readString();
  f.close();
  
  // Delete test file
  SD.remove("/test.txt");
  
  log_msg("SD card test completed successfully");
  log_msg("Read test content: " + content);
}

void mn(){
  log_msg("\n=== MENU ===");
  log_msg("t - Tare load cell");
  log_msg("r - Run full calibration");

  log_msg("s - Start test");
  log_msg("x - Stop test");
  log_msg("d - Test SD card");

  log_msg("m - Show this menu");
  log_msg("===========");
}

const char* gn(int n){
  static char n_[13];
  sprintf(n_,"/T%d.CSV",n);  // Add leading slash for root directory
  return n_;
}
