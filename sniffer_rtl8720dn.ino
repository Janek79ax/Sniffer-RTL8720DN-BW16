#include "vector"
#include "wifi_conf.h"
#include "map"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "debug.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"
#include <list>

#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h>  // 4 display ST7735/ST7789

/*PINS
SDA - PA12

A0 - PB2
RESET - PB1

CS - PA15
SCK - PA14

*/
// SPI pins:
#define TFT_CS SPI_SS  // Chip select
#define TFT_RST 4      // Reset
#define TFT_DC 5       // Data/Command

uint8_t target[2][6] = {
  { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
  { 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1 }
};
int noOfTargets = 2;

int noOfChannels = 2;
uint8_t channels[2] = { 11, 44 };

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


struct WiFiSignal {
  unsigned char addr[6];
  signed char rssi;
  unsigned long lastTime;
  unsigned long firstTime;
};


std::list<WiFiSignal> _signals;

unsigned long currentTime = 0;
unsigned long deltaTime = 0;

void printMac(const unsigned char mac[6]) {
  for (u8 i = 0; i < 6; i++) {
    if (i > 0) {
      tft.print(":");
    }
    if (mac[i] < 16) {
      tft.print("0");
    }
    tft.print(mac[i], HEX);
  }
}

void debugMac(const unsigned char mac[6]) {
  for (u8 i = 0; i < 6; i++) {
    if (i > 0) {
      Serial.print(":");
    }
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
  }
}



void formatTimeDifference(long milliseconds) {
  if (milliseconds < 1000) {
    tft.print("NOW; ");
    return;
  }

  long seconds = milliseconds / 1000;
  milliseconds %= 1000;

  long days = seconds / 86400;
  seconds %= 86400;

  long hours = seconds / 3600;
  seconds %= 3600;

  long minutes = seconds / 60;
  seconds %= 60;

  if (days > 0) {
    tft.print(days);
    tft.print("d ");
    if (hours > 0) {
      tft.print(hours);
      tft.print("h ");
    }
    return;
  }

  if (hours > 0) {
    tft.print(hours);
    tft.print("h ");
    if (minutes > 0) {
      tft.print(minutes);
      tft.print("min ");
    }
    return;
  }

  if (minutes > 0) {
    tft.print(minutes);
    tft.print("min ");
  }

  if (seconds > 0) {
    tft.print(seconds);
    tft.print("s");
  }
}





void printRSSIDescription(int rssi) {
  if (rssi >= -50) {
    tft.print("Excellent)");
  } else if (rssi >= -60) {
    tft.print("Very good)");
  } else if (rssi >= -70) {
    tft.print("Good)");
  } else if (rssi >= -80) {
    tft.print("Poor)");
  } else {
    tft.print("Crap)");
  }
}

void printSignals() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  std::list<WiFiSignal>::reverse_iterator next = _signals.rbegin();
  int counter = 1;
  while (next != _signals.rend()) {
    if (counter > 1) {
      tft.print("\n\n");
    }
    tft.print(counter);
    tft.print(")");
    printMac(next->addr);
    tft.print("\n");
    deltaTime = currentTime - next->firstTime;
    formatTimeDifference(deltaTime);
    tft.print(" / ");
    deltaTime = currentTime - next->lastTime;
    formatTimeDifference(deltaTime);

    //printRSSIDescription(next->rssi);
    next++;
    counter++;
  }
  tft.print("\r\n");
}

static void promisc_callback(unsigned char *buf, unsigned int len, void *userdata) {
  const ieee80211_frame_info_t *frameInfo = (ieee80211_frame_info_t *)userdata;

  currentTime = millis();

  if (frameInfo->rssi == 0) {
    return;
  }

  WiFiSignal wifisignal;
  wifisignal.rssi = frameInfo->rssi;
  memcpy(&wifisignal.addr, &frameInfo->i_addr2, 6);
  wifisignal.lastTime = currentTime;
  wifisignal.firstTime = currentTime;

  uint8_t dest[6];
  memcpy(dest, frameInfo->i_addr1, 6);
  //check if we're interested in this package:
  int is_in_array = 0;
  for (int i = 0; i < noOfTargets; i++) {
    if (memcmp(dest, target[i], 6) == 0) {
      is_in_array = 1;
      break;
    }
  }

  if (is_in_array == 1) {
    std::list<WiFiSignal>::iterator next = _signals.begin();
    unsigned long deletedElementFirstSeenTime = 0;
    while (next != _signals.end()) {
      if (next->addr[0] == wifisignal.addr[0] && next->addr[1] == wifisignal.addr[1] && next->addr[2] == wifisignal.addr[2] && next->addr[3] == wifisignal.addr[3] && next->addr[4] == wifisignal.addr[4] && next->addr[5] == wifisignal.addr[5]) {
        deletedElementFirstSeenTime = next->firstTime;
        next = _signals.erase(next);
      }
      next++;
    }
    if (deletedElementFirstSeenTime != 0) {
      wifisignal.firstTime = deletedElementFirstSeenTime;
    }
    _signals.push_back(wifisignal);
  }
}


void scanChannels(u8 *channels, u8 numberOfChannels, u32 scanTimePerChannel) {
  _signals.clear();
  for (u8 ch = 0; ch < numberOfChannels; ch++) {
    // Serial.print("Channel set: ");
    // Serial.println(channels[ch]);
    wifi_set_channel(channels[ch]);
    delay(scanTimePerChannel);
  }
}



void setup() {
  Serial.begin(115200);

  tft.initR(INITR_BLACKTAB);       
  tft.setTextColor(ST77XX_WHITE); 
  tft.setTextSize(0.4);          
  tft.setCursor(0, 0);          

  wifi_on(RTW_MODE_PROMISC);
  wifi_enter_promisc_mode();
  wifi_set_channel(44);
  wifi_set_promisc(RTW_PROMISC_ENABLE_2, promisc_callback, 0);
}

unsigned long lastScreenRefresh = 0;
unsigned long lastChannelSwitch = 0;

int channelIndex = 0;

void loop() {

  currentTime = millis();
  unsigned long screenDiff = currentTime - lastScreenRefresh;
  unsigned long channelDiff = currentTime - lastChannelSwitch;

  if (screenDiff > 10000) {
    printSignals();
    lastScreenRefresh = currentTime;
  }

  if (channelDiff > 1000) {
    lastChannelSwitch = currentTime;
    wifi_set_channel(channels[channelIndex++]);
    if (channelIndex >= noOfChannels) { channelIndex = 0; }
  }
}
