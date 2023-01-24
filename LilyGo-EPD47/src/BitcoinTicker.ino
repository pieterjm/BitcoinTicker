#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif


#include <WiFi.h>
#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <FS.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <arduino-timer.h>
#include "manjaribold16.h"
#include "manjaribold32.h"
#include "manjaribold100.h"
#include "Button2.h"
#include <qrcode.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include "serialconfig.h"
#include <mbedtls/md.h>

#define CONFIG_SSID "ssid"
#define CONFIG_WIFIPASSWORD "wifipassword"
#define CONFIG_GERTYURL "gertyurl"
#define CONFIG_UPDATEINTERVAL "updateinterval"
#define CONFIG_HASH "contenthash"

#define DISPLAY_PRICE 0
#define DISPLAY_BLOCKHEIGHT 1
#define DISPLAY_SATSUSD 2
#define DISPLAY_MSCW 3
#define DISPLAY_HASHRATE 4
#define DISPLAY_MEMPOOL_TRANSACTIONS 5
#define DISPLAY_MEMPOOL_BLOCKS 6
#define DISPLAY_GERTY 7
#define DISPLAY_MAX 8

#define BUTTON_1            34
#define BUTTON_2            35
#define BUTTON_3            39

Button2  btn1(BUTTON_1);
Button2  btn2(BUTTON_2);
Button2  btn3(BUTTON_3);


#define TEXT_COLOR 15
#define BG_COLOR 0

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540
#define SCREEN_MARGIN 50
#define AREA_PADDING 20
#define LINE_PADDING 10
#define YPOS_LEGEND 150
#define YPOS_TICKER 3800
#define XPOS_LEGEND 480
#define XPOS_TICKER 0
#define FONT_FACTOR  1.6

Preferences preferences;
int display = DISPLAY_GERTY;
int screenNumber = 0;
int nextScreenNumber = 0;


#if defined(CONFIG_IDF_TARGET_ESP32)
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15
#else
#error "Platform not supported"
#endif

//WebServer webServer;
//AutoConnect portal(webServer);
//AutoConnectConfig config;
auto timer = timer_create_default();

String blockheight = "";
String price = "";
String satsperusd = "";
String moscowtime = "";
String hashrate = "";
String mempool_transactions = "";
String mempool_blocks = "";

// python3 fontconvert.py --compress ManjariBold 32 /usr/share/fonts/opentype/malayalam/Manjari-Bold.otf > manjaribold.h

uint8_t *framebuffer;

int32_t get_cursor_x(GFXfont *font,String l,String position) {
  if ( position == "center") {
    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t w = 0;
    int32_t h = 0;
    get_text_bounds(font, l.c_str(), &x, &y, &x1, &y1, &w, &h, NULL);
    return 480 - w / 2;
  } else {
    return SCREEN_MARGIN;
  }
}

int getfontsize(int size) {
  switch ( size ) {
    case 1:
    case 15:
      return 16;
    case 5:
    case 32:
      return 32;
    case 80:
      return 100;
    default:
      return 16;
  }
}

GFXfont *getFont(int size) {
  switch(size) {
    case 16:
      return (GFXfont *)&ManjariBold16;
    case 32:
      return (GFXfont *)&ManjariBold32;
    case 100:
      return (GFXfont *)&ManjariBold100;
    default:
      return (GFXfont *)&ManjariBold16;
  }
}

void update_gerty()
{
  HTTPClient http;  
  http.begin(preferences.getString(CONFIG_GERTYURL));
  int httpCode = http.GET();

  if(httpCode != HTTP_CODE_OK) {
    return;
  }
  
  String payload = http.getString();                
  http.end();

  // check if content has been changed and return if it hasn't
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) payload.c_str(), payload.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
 
  String hash = "";  
  for(int i= 0; i< sizeof(shaResult); i++)
  {
    char str[3];
    sprintf(str, "%02x", (int)shaResult[i]);
    hash += str;
  }
  if ( preferences.getString(CONFIG_HASH,"") == hash ) {
    return;
  }
  preferences.putString(CONFIG_HASH,hash);
  
  DynamicJsonDocument jsobj(2048);
  deserializeJson(jsobj,payload);
      
  JsonArray arr = jsobj["screen"]["areas"];
  int totalweight = 0;
  for (JsonArray grarr: arr) {
    for (JsonObject obj : grarr ) {        
      int size = getfontsize(obj["size"]);
      String value = obj["value"].as<String>();
      totalweight += FONT_FACTOR * size;
      for(int i=0;(i<value.length());i++) {
        if (value.charAt(i) == '\n') {
          totalweight += FONT_FACTOR * size;  
          totalweight += LINE_PADDING;
        }
      }
    }
    totalweight += AREA_PADDING;
  }

  int cursor_y = SCREEN_HEIGHT / 2 - totalweight / 2;
      
  epd_poweron();
  clearepd();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);    
  
  for (JsonArray grarr: arr) {    
    for (JsonObject obj : grarr ) {
    
      String value = obj["value"];
      String position = obj["position"];
      int size = getfontsize(obj["size"]);
      GFXfont *font = getFont(size);
          
      int index = value.indexOf("\n");
      while ( index != -1 ) {            
        String sub = value.substring(0,index);
        value = value.substring(index + 1);

        int cursor_x = get_cursor_x(font,sub,position);
        cursor_y += FONT_FACTOR * size;
        writeln(font,sub.c_str(),&cursor_x,&cursor_y,framebuffer);             
        cursor_y += LINE_PADDING;
        index = value.indexOf("\n");
      }
    
      int cursor_x = get_cursor_x(font,value,position);
      cursor_y += FONT_FACTOR * size;
      writeln(font,value.c_str(),&cursor_x,&cursor_y,framebuffer); 

      cursor_y += LINE_PADDING;
    }
    cursor_y += AREA_PADDING;
  }
  
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  //epd_poweroff();
  //epd_poweroff_all();
 }




void clearepd()
{
  Rect_t area = {
        .x = 0,
        .y = 0,
        .width = 960,
        .height = 540
  };
  epd_clear_area_cycles(area, 2, 40);

}

bool update_display(void *)
{
  update_gerty();  
  return true;
}

void readConfig()
{
  File file = SPIFFS.open(PARAM_FILE,"r");
  if ( ! file ) {
    return;
  }

  StaticJsonDocument<2000> doc;
  String content = file.readString();
  DeserializationError error = deserializeJson(doc, content);
  file.close();

  // return if deserialization failed
  if ( error.code() != DeserializationError::Ok ) {
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj: arr) {
    String name = obj["name"];
    String value = obj["value"];

    if ( name == CONFIG_SSID ) {
      preferences.putString(CONFIG_SSID,value);
    } else if ( name == CONFIG_WIFIPASSWORD ) {
      preferences.putString(CONFIG_WIFIPASSWORD,value);
    } else if ( name == CONFIG_GERTYURL ) {
      preferences.putString(CONFIG_GERTYURL,value); 
    } else if ( name == CONFIG_UPDATEINTERVAL ) {
      preferences.putInt(CONFIG_UPDATEINTERVAL,value.toInt());
    }
  }
}

void displayHelp(String msg) {
  Serial.println(msg);
  while ( true ) {
    handleSerial();
  }
}

void setup()
{
  // open serial port
  Serial.begin(115200);
  delay(1000);

  // open SD
  SPIFFS.begin(true);

  // initialize frame buffer
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  epd_init();


  // read config
  preferences.begin("BitcoinTicker", false);  
  readConfig();

  // start Wi-Fi
  if ( preferences.isKey(CONFIG_SSID) == false || preferences.isKey(CONFIG_WIFIPASSWORD) == false ) {
    displayHelp("Wi-Fi configuration absent");
  }

  // start Wi-Fi
  WiFi.begin(preferences.getString(CONFIG_SSID).c_str(),preferences.getString(CONFIG_WIFIPASSWORD).c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  
  update_display((void *)0);

  if ( preferences.isKey(CONFIG_UPDATEINTERVAL) == false ) {
    displayHelp("Update Interval is missing from config");
  }
  timer.every(preferences.getInt(CONFIG_UPDATEINTERVAL),update_display);
}


void loop()
{
  timer.tick();
  handleSerial();
}
