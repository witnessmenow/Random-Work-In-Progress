/*******************************************************************
    Tetris clock using a 64x32 RGB Matrix that fetches its
    time over WiFi using the EzTimeLibrary.

    For use with my I2S Matrix Shield.

    Parts Used:
    ESP32 D1 Mini * - https://s.click.aliexpress.com/e/_dSi824B
    ESP32 I2S Matrix Shield (From my Tindie) = https://www.tindie.com/products/brianlough/esp32-i2s-matrix-shield/

      = Affilate

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/

    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

// ----------------------------
// #Defines for libraries - Need to be set before importing the library
// ----------------------------

#define USE_SPIFFS            true
#define ESP_DRD_USE_EEPROM    true


// ----------------------------
// Standard Libraries - Already Installed if you have ESP32 set up
// ----------------------------

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <ESP_WiFiManager.h>
// Captive portal for configuring the WiFi

// Can be installed from the library manager (Search for "ESP_WiFiManager")
// https://github.com/khoih-prog/ESP_WiFiManager

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
// This is the library for interfacing with the display

// Can be installed from the library manager (Search for "ESP32 MATRIX DMA")
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

#include <TetrisMatrixDraw.h>
// This library draws out characters using a tetris block
// amimation
// Can be installed from the library manager
// https://github.com/toblum/TetrisAnimation

#include <ezTime.h>
// Library used for getting the time and adjusting for DST
// Search for "ezTime" in the Arduino Library manager
// https://github.com/ropg/ezTime

#include <ArduinoJson.h>
// ArduinoJson is used for parsing and creating the config file.
// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

// ----------------------------
// Dependency Libraries - each one of these will need to be installed.
// ----------------------------

// Adafruit GFX library is a dependancy for the matrix Library
// Can be installed from the library manager
// https://github.com/adafruit/Adafruit-GFX-Library


// -------------------------------------
// ------- Replace the following! ------
// -------------------------------------

// Set a timezone using the following list
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
#define MY_TIME_ZONE "Europe/Dublin"

// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

// -------------------------------------
// -------   Clock Config   ------
// -------------------------------------

// Sets whether the clock should be 12 hour format or not.
bool twelveHourFormat = true;

// If this is set to false, the number will only change if the value behind it changes
// e.g. the digit representing the least significant minute will be replaced every minute,
// but the most significant number will only be replaced every 10 minutes.
// When true, all digits will be replaced every minute.
bool forceRefresh = true;

// -------------------------------------
// -------   Other Config   ------
// -------------------------------------

#define DRD_TIMEOUT             10
#define DRD_ADDRESS             0
const int PIN_LED       = 2;

#define TETRIS_CONFIG_JSON "/tetris_config.json"

// -----------------------------

MatrixPanel_I2S_DMA *dma_display = nullptr;

//WiFiManager wm; // global wm instance

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * animationTimer = NULL;

unsigned long animationDue = 0;
unsigned long animationDelay = 100; // Smaller number == faster

uint16_t myBLACK = dma_display->color565(0, 0, 0);

TetrisMatrixDraw tetris(*dma_display); // Main clock
TetrisMatrixDraw tetris2(*dma_display); // The "M" of AM/PM
TetrisMatrixDraw tetris3(*dma_display); // The "P" or "A" of AM/PM

Timezone myTZ;
unsigned long oneSecondLoopDue = 0;

bool showColon = true;
volatile bool finishedAnimating = false;
bool displayIntro = true;

String lastDisplayedTime = "";
String lastDisplayedAmPm = "";

//flag for saving data
bool shouldSaveConfig = false;

#define TIME_ZONE_LEN 100

#define TIME_ZONE_LABEL "mytimezone"
#define MATRIX_CLK_LABEL "clk"
#define MATRIX_DRIVER_LABEL "driver"
#define MATRIX_64_LABEL "is64"
#define TIME_24H_LABEL "is24h"
#define TIME_FORCE_LABEL "isForce"
#define TIME_ANIMATION_DELAY_LABEL

char timeZone[TIME_ZONE_LEN] = MY_TIME_ZONE;
bool matrixClk = false;
bool matrixDriver = false;
bool matrixIs64 = false;

// This method is for controlling the tetris library draw calls
void animationHandler()
{
  // Not clearing the display and redrawing it when you
  // dont need to improves how the refresh rate appears
  if (!finishedAnimating) {
    dma_display->flipDMABuffer();
    dma_display->fillScreen(myBLACK);
    if (displayIntro) {
      finishedAnimating = tetris.drawText(1, 21);
    } else {
      if (twelveHourFormat) {
        // Place holders for checking are any of the tetris objects
        // currently still animating.
        bool tetris1Done = false;
        bool tetris2Done = false;
        bool tetris3Done = false;

        tetris1Done = tetris.drawNumbers(-6, 26, showColon);
        tetris2Done = tetris2.drawText(56, 25);

        // Only draw the top letter once the bottom letter is finished.
        if (tetris2Done) {
          tetris3Done = tetris3.drawText(56, 15);
        }

        finishedAnimating = tetris1Done && tetris2Done && tetris3Done;

      } else {
        finishedAnimating = tetris.drawNumbers(2, 26, showColon);
      }
    }
    dma_display->showDMABuffer();
  }

}

void drawIntro(int x = 0, int y = 0)
{
  dma_display->flipDMABuffer();
  dma_display->fillScreen(myBLACK);
  tetris.drawChar("P", x, y, tetris.tetrisCYAN);
  tetris.drawChar("o", x + 5, y, tetris.tetrisMAGENTA);
  tetris.drawChar("w", x + 11, y, tetris.tetrisYELLOW);
  tetris.drawChar("e", x + 17, y, tetris.tetrisGREEN);
  tetris.drawChar("r", x + 22, y, tetris.tetrisBLUE);
  tetris.drawChar("e", x + 27, y, tetris.tetrisRED);
  tetris.drawChar("d", x + 32, y, tetris.tetrisWHITE);
  tetris.drawChar(" ", x + 37, y, tetris.tetrisMAGENTA);
  tetris.drawChar("b", x + 42, y, tetris.tetrisYELLOW);
  tetris.drawChar("y", x + 47, y, tetris.tetrisGREEN);
  dma_display->showDMABuffer();
}

void drawConnecting(int x = 0, int y = 0)
{
  dma_display->flipDMABuffer();
  dma_display->fillScreen(myBLACK);
  tetris.drawChar("C", x, y, tetris.tetrisCYAN);
  tetris.drawChar("o", x + 5, y, tetris.tetrisMAGENTA);
  tetris.drawChar("n", x + 11, y, tetris.tetrisYELLOW);
  tetris.drawChar("n", x + 17, y, tetris.tetrisGREEN);
  tetris.drawChar("e", x + 22, y, tetris.tetrisBLUE);
  tetris.drawChar("c", x + 27, y, tetris.tetrisRED);
  tetris.drawChar("t", x + 32, y, tetris.tetrisWHITE);
  tetris.drawChar("i", x + 37, y, tetris.tetrisMAGENTA);
  tetris.drawChar("n", x + 42, y, tetris.tetrisYELLOW);
  tetris.drawChar("g", x + 47, y, tetris.tetrisGREEN);
  dma_display->showDMABuffer();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveConfigFile() {
  Serial.println(F("Saving config"));
  DynamicJsonDocument  json(200);
  json["timeZone"] = timeZone;
  JsonObject matrixJson = json.createNestedObject("matrix");
  matrixJson[MATRIX_CLK_LABEL] = matrixClk;
  matrixJson[MATRIX_DRIVER_LABEL] = matrixDriver;
  matrixJson[MATRIX_64_LABEL] = matrixIs64;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}

bool setupSpiffs() {
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(TETRIS_CONFIG_JSON)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/tetris_config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error) {
          Serial.println("\nparsed json");

          strcpy(timeZone, json[TIME_ZONE_LABEL]);
          matrixClk = json["matrix"][MATRIX_CLK_LABEL].as<bool>();
          matrixDriver = json["matrix"][MATRIX_DRIVER_LABEL].as<bool>();
          matrixIs64 = json["matrix"][MATRIX_64_LABEL].as<bool>();

          return true;

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  return false;
}

void configDisplay() {
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  if (matrixIs64) {
    mxconfig.mx_height = 64;
  }


  mxconfig.double_buff = true;
  mxconfig.gpio.e = 18;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  //mxconfig.clkphase = false;

  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->fillScreen(myBLACK);
}

void setup() {
  Serial.begin(115200);

  bool spiffsSetup = setupSpiffs();

  bool launchConfig = !spiffsSetup;
  ESP_WiFiManager ESP_wifiManager("Tetris-Clock");

  if (!launchConfig) {
    //Checking if there is Wifi Creds stored.
    if (ESP_wifiManager.WiFi_SSID() == "") {
      Serial.println(F("No AP credentials"));
      launchConfig = true;
    }
  }

  ESP_WMParameter p_time_zone(TIME_ZONE_LABEL, "Time Zone label", timeZone, TIME_ZONE_LEN);
  ESP_WMParameter p_clk(MATRIX_CLK_LABEL, "Set clkphase false", "T", 2, "type=\"checkbox\"");
  ESP_WMParameter p_driver(MATRIX_DRIVER_LABEL, "Set Driver to FM6126A", "T", 2, "type=\"checkbox\"");
  ESP_WMParameter p_is64(MATRIX_64_LABEL, "64x64 panel", "T", 2, "type=\"checkbox\"");

  ESP_wifiManager.addParameter(&p_time_zone);
  ESP_wifiManager.addParameter(&p_clk);
  ESP_wifiManager.addParameter(&p_driver);
  ESP_wifiManager.addParameter(&p_is64);

  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);


  if (launchConfig) {
    Serial.println(F("Starting Config Portal"));
    digitalWrite(PIN_LED, LOW);
    if (!ESP_wifiManager.startConfigPortal("WifiTetris", "clock123")) {
      Serial.println(F("Not connected to WiFi"));
    }
    else {
      Serial.println(F("connected"));
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("connected. Local IP: "));
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
  }

  //read updated parameters
  strncpy(timeZone, p_time_zone.getValue(), sizeof(timeZone));
  Serial.print("p_clk.getValue()");
  Serial.println(p_clk.getValue());
  matrixClk = (strncmp(p_clk.getValue(), "T", 1) == 0);
  matrixDriver = (strncmp(p_driver.getValue(), "T", 1) == 0);
  matrixIs64 = (strncmp(p_is64.getValue(), "T", 1) == 0);

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }

  digitalWrite(PIN_LED, HIGH);

  configDisplay();

  tetris.display = dma_display; // Main clock
  tetris2.display = dma_display; // The "M" of AM/PM
  tetris3.display = dma_display; // The "P" or "A" of AM/PM

  // "connecting"
  drawConnecting(5, 10);

  // Setup EZ Time
  setDebug(INFO);
  waitForSync();

  Serial.println();
  Serial.println("UTC:             " + UTC.dateTime());

  myTZ.setLocation(timeZone);
  Serial.print(F("Time in your set timezone:         "));
  Serial.println(myTZ.dateTime());

  // "Powered By"
  drawIntro(6, 12);
  delay(2000);

  // Start the Animation Timer
  tetris.setText("TRINITY");

  // Wait for the animation to finish
  while (!finishedAnimating)
  {
    delay(animationDelay);
    animationHandler();
  }
  delay(2000);
  finishedAnimating = false;
  displayIntro = false;
  tetris.scale = 2;
  //tetris.setTime("00:00", true);
}

void setMatrixTime() {
  String timeString = "";
  String AmPmString = "";
  if (twelveHourFormat) {
    // Get the time in format "1:15" or 11:15 (12 hour, no leading 0)
    // Check the EZTime Github page for info on
    // time formatting
    timeString = myTZ.dateTime("g:i");

    //If the length is only 4, pad it with
    // a space at the beginning
    if (timeString.length() == 4) {
      timeString = " " + timeString;
    }

    //Get if its "AM" or "PM"
    AmPmString = myTZ.dateTime("A");
    if (lastDisplayedAmPm != AmPmString) {
      Serial.println(AmPmString);
      lastDisplayedAmPm = AmPmString;
      // Second character is always "M"
      // so need to parse it out
      tetris2.setText("M", forceRefresh);

      // Parse out first letter of String
      tetris3.setText(AmPmString.substring(0, 1), forceRefresh);
    }
  } else {
    // Get time in format "01:15" or "22:15"(24 hour with leading 0)
    timeString = myTZ.dateTime("H:i");
  }

  // Only update Time if its different
  if (lastDisplayedTime != timeString) {
    Serial.println(timeString);
    lastDisplayedTime = timeString;
    tetris.setTime(timeString, forceRefresh);

    // Must set this to false so animation knows
    // to start again
    finishedAnimating = false;
  }
}

void handleColonAfterAnimation() {

  // It will draw the colon every time, but when the colour is black it
  // should look like its clearing it.
  uint16_t colour =  showColon ? tetris.tetrisWHITE : tetris.tetrisBLACK;
  // The x position that you draw the tetris animation object
  int x = twelveHourFormat ? -6 : 2;
  // The y position adjusted for where the blocks will fall from
  // (this could be better!)
  int y = 26 - (TETRIS_Y_DROP_DEFAULT * tetris.scale);
  tetris.drawColon(x, y, colour);
  dma_display->showDMABuffer();
}


void loop() {
  unsigned long now = millis();
  if (now > oneSecondLoopDue) {
    // We can call this often, but it will only
    // update when it needs to
    setMatrixTime();
    showColon = !showColon;

    // To reduce flicker on the screen we stop clearing the screen
    // when the animation is finished, but we still need the colon to
    // to blink
    if (finishedAnimating) {
      handleColonAfterAnimation();
    }
    oneSecondLoopDue = now + 1000;
  }
  now = millis();
  if (now > animationDue) {
    animationHandler();
    animationDue = now + animationDelay;
  }
}
