#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <AutoConnect.h> 
#include <arduino-timer.h>
#include "manjaribold16.h"
#include "manjaribold32.h"
#include "manjaribold100.h"
#include "Button2.h"
#include <qrcode.h>

#define DISPLAY_PRICE 0
#define DISPLAY_BLOCKHEIGHT 1
#define DISPLAY_SATSUSD 2
#define DISPLAY_MSCW 3
#define DISPLAY_HASHRATE 4
#define DISPLAY_MEMPOOL_TRANSACTIONS 5
#define DISPLAY_MEMPOOL_BLOCKS 6
#define DISPLAY_MAX 7

#define BUTTON_1            34
#define BUTTON_2            35
#define BUTTON_3            39

String legend[DISPLAY_MAX] = {
  "Market price of bitcoin",
  "# blocks in the blockchain",
  "One US dollar in Sats",
  "Moscow time",
  "Mining hashrate (EH/s)",
  "# transactions in mempool",
  "# blocks in mempool"
};


Button2  btn1(BUTTON_1);
Button2  btn2(BUTTON_2);
Button2  btn3(BUTTON_3);


#define TEXT_COLOR 15
#define BG_COLOR 0

#define SCREEN_WIDTH 960
#define YPOS_LEGEND 150
#define YPOS_TICKER 3800
#define XPOS_LEGEND 480
#define XPOS_TICKER 0

int display = DISPLAY_PRICE;  // the ticker to display initially

#if defined(CONFIG_IDF_TARGET_ESP32)
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15
#else
#error "Platform not supported"
#endif

WebServer webServer;
AutoConnect portal(webServer);
AutoConnectConfig config;
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

int32_t get_cursor_x(GFXfont *font,String l) {
    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t w = 0;
    int32_t h = 0;
    get_text_bounds(font, l.c_str(), &x, &y, &x1, &y1, &w, &h, NULL);
    return 480 - w / 2;
}

void deleteAllCredentials() {
  AutoConnectCredential credential;
  station_config_t cfg;
  uint8_t ent = credential.entries();

  while (ent--) {
    credential.load((int8_t)0, &cfg);    
    credential.del((const char*)&cfg.ssid[0]);
 
  }
}


void display_legend(String l) 
{
    int cursor_x = get_cursor_x((GFXfont *)&ManjariBold32,l);
    int cursor_y = YPOS_LEGEND;  

    writeln((GFXfont *)&ManjariBold32,l.c_str(),&cursor_x,&cursor_y,framebuffer);
}

void display_ticker()
{  
  int cursor_x = 0;
  int cursor_y = 400;
  switch ( display ) {
    case DISPLAY_PRICE:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,price);          
      writeln((GFXfont *)&ManjariBold100,price.c_str(),&cursor_x,&cursor_y,framebuffer);
      break;
     case DISPLAY_BLOCKHEIGHT:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,blockheight);
      writeln((GFXfont *)&ManjariBold100,blockheight.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
    case DISPLAY_SATSUSD:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,satsperusd);
      writeln((GFXfont *)&ManjariBold100,satsperusd.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
    case DISPLAY_MSCW:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,moscowtime);
      writeln((GFXfont *)&ManjariBold100,moscowtime.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
    case DISPLAY_HASHRATE:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,hashrate);
      writeln((GFXfont *)&ManjariBold100,hashrate.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
    case DISPLAY_MEMPOOL_TRANSACTIONS:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,mempool_transactions);
      writeln((GFXfont *)&ManjariBold100,mempool_transactions.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
    case DISPLAY_MEMPOOL_BLOCKS:
      cursor_x = get_cursor_x((GFXfont *)&ManjariBold100,mempool_blocks);
      writeln((GFXfont *)&ManjariBold100,mempool_blocks.c_str(),&cursor_x,&cursor_y,framebuffer);      
      break;
  }

}

void update_mempool_stats()
{
  HTTPClient http;
  http.begin("https://mempool.space/api/mempool"); 
  int httpCode = http.GET();
  if(httpCode > 0) {
  
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();                
      DynamicJsonDocument jsobj(1024);
      deserializeJson(jsobj,payload);
      mempool_transactions = String((int) jsobj["count"]);
      mempool_blocks = String((int)(round(0.5+((int)jsobj["vsize"])/1000000.0)));
    }
  } 
  http.end();
}

void update_price()
{
  Serial.println("Update price");
  HTTPClient http;  
  http.begin("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd"); 
  int httpCode = http.GET();
  Serial.println(httpCode);
  if(httpCode > 0) {
    
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();                
      DynamicJsonDocument jsobj(1024);
      deserializeJson(jsobj,payload);
      price = String("$") + String((int) jsobj["bitcoin"]["usd"]);
      satsperusd = String(100000000 / (int) jsobj["bitcoin"]["usd"]);
      moscowtime = satsperusd.substring(0,2) + ":" + satsperusd.substring(2);
      
    }
  } else if ( httpCode == -1 ) {
    deleteAllCredentials();
    delay(5000);
    ESP.restart();
  }
  http.end();  
}



void update_blockheight()
{
  HTTPClient http;
  http.begin("https://mempool.space/api/blocks/tip/height"); 
  int httpCode = http.GET();
  if(httpCode > 0) {
  
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      blockheight = http.getString();                
    }
  } 
  http.end();
}

int find_in_string(String str,String pat)
{
    int found = 0;
    int index = 0;
    int len;

    len = str.length();
    
    if (pat.length() > len) {
        return 0;
    }
    while (index < len) {
        if (str.charAt(index) == pat.charAt(found)) {          
            found++;
            if (pat.length() == found) {
                return (index - found);
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}

void update_hashrate()
{
  HTTPClient http;
  http.begin("https://mempool.space/api/v1/mining/hashrate/3d"); 
  int httpCode = http.GET();
  if(httpCode > 0) {
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();                
      int start = find_in_string(payload,"currentHashrate");
      int stop = find_in_string(payload,"currentDifficulty");
      if ( start > 0 && stop > 0 ) {
        start += strlen("currentHashrate") + 3;
        stop -= 1;
        hashrate = payload.substring(start, stop);
        hashrate = hashrate.substring(0,hashrate.length() - 18);
      }
    }
  } 
  http.end();
}

void rootPage() {
  char content[] = "Hello, world";
  webServer.send(200, "text/plain", content);
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

void update_display() 
{
  epd_poweron();
  clearepd();
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  display_legend(legend[display]);
  display_ticker();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();
  epd_poweroff_all();

}

bool update_and_display(void *)
{
  esp_wifi_start();
  update_hashrate();
  update_price();
  update_blockheight();
  update_mempool_stats();
  esp_wifi_stop();
  
  update_display();
  return true;
}

void buttonPressed(Button2 &b)
{
  display++;
  if (display >= DISPLAY_MAX ) {
    display = 0;
  }
  update_display();    
} 


bool handlePortalOnDetect(IPAddress& softAPIP) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  String portalurl = "URL:http://" + softAPIP.toString() + "/_ac";
  String wifidata = "WIFI:S:"+config.apid+";T:WPA;P:"+config.psk+";;;";
  
  epd_poweron();
  clearepd();

  int cursor_x = 50;
  int cursor_y = 60;
  writeln((GFXfont *)&ManjariBold32,"Bitcoin ticker",&cursor_x,&cursor_y,framebuffer);

  cursor_x = 50;
  cursor_y = 110;
  writeln((GFXfont *)&ManjariBold16,"To configure, connect to the BitcoinTicker Wi-Fi network",&cursor_x,&cursor_y,framebuffer);
  cursor_x = 50;
  cursor_y = 150;
  writeln((GFXfont *)&ManjariBold16,"and wait for the configuration portal to appear. If that",&cursor_x,&cursor_y,framebuffer);
  cursor_x = 50;
  cursor_y = 190;
  writeln((GFXfont *)&ManjariBold16,"is not automatically loaded, scan the QR code below.",&cursor_x,&cursor_y,framebuffer);

  qrcode_initText(&qrcode, qrcodeData, 3, 0, portalurl.c_str());
  for (int32_t y = 0; y < qrcode.size; y++) {
    for (int32_t x = 0; x < qrcode.size; x++) {
      if ( qrcode_getModule(&qrcode, x, y) ) {
        epd_fill_rect(400+x*10,240+y*10,10,10,100,framebuffer);
      }
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff_all();
  return true;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("This is BitcoinTicker");
    delay(1000);

    config.apid = "BitcoinTicker";
    config.psk = "";
    config.hidden = 0;

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) {
        Serial.println("alloc memory failed !!!");
        while (1);
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    epd_init();


    

    webServer.on("/", rootPage);
    portal.config(config);
    portal.onDetect(handlePortalOnDetect);
    portal.begin();
    
    

    btn1.setPressedHandler(buttonPressed);
    btn2.setPressedHandler(buttonPressed);
    btn3.setPressedHandler(buttonPressed);

    
  
  display = DISPLAY_PRICE;  
  update_and_display((void *)0);
  timer.every(120000, update_and_display);
  epd_poweroff_all();
}

void loop()
{
    portal.handleClient();
    timer.tick();
    btn1.loop();
    btn2.loop();
    btn3.loop();

}
