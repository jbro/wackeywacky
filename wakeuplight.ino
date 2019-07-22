#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
WiFiManager wifiManager;
const char deviceName[] = "WakeyWakey";

#include <ArduinoOTA.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#define TIME_DRIFT_INFO
#include <Time.h>
#include <ESP8266HTTPClient.h>
WiFiUDP ntpUDP;
static const char ntpServer[] = "dk.pool.ntp.org";
NTPClient timeClient(ntpUDP, ntpServer);

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
NeoPixelAnimator sunrise(1, NEO_SECONDS);
NeoPixelBus<NeoGrbFeature, NeoEsp8266DmaWs2812xMethod> strip(116);
RgbColor OrangeRed(255, 69, 0);
RgbColor DarkGreen(0, 100, 0);
RgbColor Red(255, 0, 0);
RgbColor MidnightBlue(25, 25, 112);
RgbColor Black(0, 0, 0);
RgbColor White(255, 255, 255);

#include "secret.h"

#define SENSLED D2
#define SENSPTR A0

#define AUTOOFFAFTER 30 * SECS_PER_MIN

#define LOGTO WEBSOCKET

#if LOGTO == WEBSOCKET
#include <WebSocketsServer.h>
WebSocketsServer webSocket = WebSocketsServer(80);
#endif

String timeToString(time_t dt) {
    return String(year(dt)) + String("-") +
    (month(dt) < 10 ? String("0") : String("")) + String(month(dt)) + String("-") +
    (day(dt) < 10 ? String("0") : String("")) + String(day(dt)) + String("T") +
    (hour(dt) < 10 ? String("0") : String("")) + String(hour(dt)) + String(":") +
    (minute(dt) < 10 ? String("0") : String("")) + String(minute(dt)) + String(":") +
    (second(dt) < 10 ? String("0") : String("")) + String(second(dt)) + String("Z");
}

inline void log(String msg) {
#if LOGTO == WEBSOCKET
  webSocket.broadcastTXT(msg);
#endif
  ;
}

void setup()
{
  pinMode(SENSLED, OUTPUT);
  digitalWrite(SENSLED,LOW);

  delay(250);

  strip.Begin();
  strip.ClearTo(MidnightBlue);
  strip.Show();

  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
      blinkLEDs(OrangeRed, 3);
      strip.ClearTo(OrangeRed);
      strip.Show();
  });
  wifiManager.setSaveConfigCallback([]() {
      strip.ClearTo(Black);
      strip.Show();
  });

  if(!wifiManager.autoConnect(deviceName)) {
    ESP.reset();
    delay(1000);
  }

  timeClient.begin();
  setSyncProvider(syncTime);
  setSyncInterval(900);

  ArduinoOTA.onEnd([]() {
      blinkLEDs(MidnightBlue, 3);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      strip.ClearTo(HsbColor((progress / (total / 1.0f)), 1.0f, 0.1f));
      strip.Show();
  });
  ArduinoOTA.onError([](ota_error_t error) {
      blinkLEDs(Red, 3);
  });
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword(otaPass);
  ArduinoOTA.begin();

#if LOGTO == WEBSOCKET
  webSocket.begin();
#endif

  blinkLEDs(DarkGreen, 3);
}

struct Sunrise {
  time_t startTime;
  int dur;
};

time_t lastCalUpdate = 0;
struct Sunrise nextEvent;
void loop() {
  time_t watch = now();

  ArduinoOTA.handle();
  webSocket.loop();
  sunrise.UpdateAnimations();

  // Stop all animations and turn light off
  if(sunrise.IsAnimationActive(0) && buttonWasHeldFor(2000)) {
    sunrise.StopAnimation(0);
    strip.ClearTo(Black);
    strip.Show();
    log("Turn off");
  }

  if(watch - lastCalUpdate >= 15 * SECS_PER_MIN) {
    lastCalUpdate = watch;
    nextEvent = getNextEvent();
    log("Time is: " + timeToString(watch));
    log("Next event at " + timeToString(nextEvent.startTime) + " for "
        + String(nextEvent.dur) + "s");
    log("Animation state: " + String(sunrise.IsAnimationActive(0)));
  }

  if(!sunrise.IsAnimationActive(0) && watch >= nextEvent.startTime) {
    int dur = nextEvent.dur - (watch - nextEvent.startTime);
    log("Starting animation at " + timeToString(nextEvent.startTime) + " for "
        + String(dur) + "s" + " ("  + String(nextEvent.dur) +"s)");
    sunrise.StartAnimation(0, dur, animateSunrise);
  }

  // Start demo
  if(!sunrise.IsAnimationActive(0) && buttonWasHeldFor(2000)) {
    sunrise.StartAnimation(0, 3, animateDemo);
    log("Demo");
  }
}

// This is not safe to invoke more than once per interation
unsigned long buttonHoldTime = 0;
bool buttonWasHeldFor(unsigned long t) {
  if(!buttonIsPressed()) {
    buttonHoldTime = millis();
  }
  else {
    if(abs(millis() - buttonHoldTime) >= t) {
      buttonHoldTime = millis();
      return true;
    }
  }
  return false;
}

unsigned long buttonHyst = 0;
bool buttonDown = false;
bool buttonIsPressed() {
  if(abs(millis() - buttonHyst) >= 500) {
    unsigned int buttonAmbient = 0;
    unsigned int buttonSense = 0;
    buttonAmbient = analogRead(A0);
    digitalWrite(SENSLED,HIGH);
    buttonSense = analogRead(A0);
    digitalWrite(SENSLED,LOW);

    if(abs(buttonSense - buttonAmbient) > 500) {
      buttonDown = true;
    }
    else {
      buttonDown = false;
    }
    buttonHyst = millis();
  }
  return buttonDown;
}

time_t syncTime() {
  while(!timeClient.forceUpdate());

  return timeClient.getEpochTime();
}

// Don't call in the mainloop!
void blinkLEDs(RgbColor color, int count) {
  for(int i = 0; i < count; i++) {
    strip.ClearTo(color);
    strip.Show();
    delay(500);
    strip.ClearTo(Black);
    strip.Show();
    delay(500);
  }
}

void animateDemo(const AnimationParam& param) {
  int p = round(param.progress * 4);
  if(param.state == AnimationState_Started) {
    strip.ClearTo(MidnightBlue);
  }
  else {
    if(p % 2) {
      strip.ClearTo(Black);
    }
    else {
      strip.ClearTo(MidnightBlue);
    }
  }
  strip.Show();

  if(param.state == AnimationState_Completed) {
    sunrise.StartAnimation(0, 120, animateSunrise);
  }
}

void Panic(String msg) {
#if LOGTO == WEBSOCKET
  webSocket.broadcastTXT(msg);
#endif
  while(true) {
    blinkLEDs(Red, 1);
  }
}

void animateSunrise(const AnimationParam& param) {
  float progress = NeoEase::CubicInOut(param.progress);
  float sunriseFraction = 0.8;

  // Sky
  HsbColor sColor;
  // Ground
  HsbColor gColor;
  // Horisont
  HsbColor hColor;

  if(param.progress < sunriseFraction) {
    sColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.0f, 1.0f, 0.0f), HsbColor(0.6667, 0.3f, 1.0f), progress);
    gColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.6667f, 1.0f, 0.0f), HsbColor(0.5f, 0.3f, 1.0f), progress);
    hColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.0f, 1.0f, 0.0f), HsbColor(0.1111f, 0.3f, 1.0f), progress);
  }
  else {
    sColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.6667, 0.3f, 1.0f), HsbColor(0.6667, 1.0f, 1.0f), progress);
    gColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.5f, 0.3f, 1.0f), HsbColor(0.5f, 1.0f, 1.0f), progress);
    hColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(HsbColor(0.1111f, 0.3f, 1.0f), HsbColor(0.1111f, 1.0f, 1.0f), progress);
  }

  /* 0-28 Sky */
  /* 29-57 East */
  /* 58-86 Hor */
  /* 87-115 West */
  strip.ClearTo(sColor, 0, 28);
  strip.ClearTo(hColor, 29, 57);
  strip.ClearTo(gColor, 58, 86);
  strip.ClearTo(hColor, 87, 115);
  strip.Show();

  if(param.state == AnimationState_Completed) {
    sunrise.StartAnimation(0, AUTOOFFAFTER, [](const AnimationParam& param) {
      if(param.state == AnimationState_Completed) {
        strip.ClearTo(Black);
        strip.Show();
      }
    });
  }
}

time_t parseDateTime(String dateTime, String timeZone) {
  dateTime.trim();

  tmElements_t tm;
  tm.Year = CalendarYrToTm(dateTime.substring(0, 4).toInt());
  tm.Month = dateTime.substring(4, 6).toInt();
  tm.Day = dateTime.substring(6, 8).toInt();

  if(dateTime.length() == 8) {
    tm.Hour = 0;
    tm.Minute = 0;
    tm.Second = 0;
    timeZone = "UTC";
  }
  else if(dateTime.length() == 15) {
    tm.Hour = dateTime.substring(9, 11).toInt();
    tm.Minute = dateTime.substring(11, 13).toInt();
    tm.Second = dateTime.substring(13, 15).toInt();
  }
  else if(dateTime.length() == 16 && dateTime[15] == 'Z') {
    tm.Hour = dateTime.substring(9, 11).toInt();
    tm.Minute = dateTime.substring(11, 13).toInt();
    tm.Second = dateTime.substring(13, 15).toInt();
    timeZone = "UTC";
  }
  else {
    Panic("Invalid datetime string:" + dateTime);
  }

  time_t dt = makeTime(tm);

  if(timeZone == "Europe/Copenhagen") {
    tm.Hour = 1;
    tm.Minute = 0;
    tm.Second = 0;

    // Calculate dst start
    tm.Month = 3;
    tm.Day = 31;
    time_t dstStart = makeTime(tm);
    tm.Day -= weekday(dstStart) - 1;
    dstStart = makeTime(tm);

    // Calculate dst end
    tm.Month = 10;
    tm.Day = 31;
    time_t dstEnd = makeTime(tm);
    tm.Day -= weekday(dstEnd) - 1;
    dstEnd = makeTime(tm);

    if(dt >= dstStart && dt <= dstEnd) {
      dt -= SECS_PER_HOUR * 2;
    }
    else {
      dt -= SECS_PER_HOUR;
    }
  } 
  else if(timeZone == "UTC") {
    ;
  } else {
    Panic("Unknown time zone: " + timeZone);
  }

  return dt;
}

enum EventFrequency {
  NOFREQ,
  WEEKLY,
  DAILY
};

time_t getNextOccurrence(time_t startTime, String rRule) {
  rRule.trim();
  int start = 0;
  int end = 0;
  String part;

  time_t until = now() + SECS_PER_WEEK;

  EventFrequency freq = NOFREQ;
  bool days[7] = { 0 };

  do {
    start = end == 0 ? end : end + 1;
    end = rRule.indexOf(';', end + 1);

    part = rRule.substring(start, end);

    if(part.startsWith("FREQ=")) {
      if(part.endsWith("WEEKLY")) {
        freq = WEEKLY;
      }
      else if(part.endsWith("DAILY")) {
        freq = DAILY;
      }
      else {
        Panic("Unsupported frequncy in recurence rule");
      }
    }
    else if(part.startsWith("BYDAY=")) {
      days[0] = part.indexOf("SU") > 0 ? true : false;
      days[1] = part.indexOf("MO") > 0 ? true : false;
      days[2] = part.indexOf("TU") > 0 ? true : false;
      days[3] = part.indexOf("WE") > 0 ? true : false;
      days[4] = part.indexOf("TH") > 0 ? true : false;
      days[5] = part.indexOf("FR") > 0 ? true : false;
      days[6] = part.indexOf("SA") > 0 ? true : false;
    }
    else if(part.startsWith("UNTIL=")) {
      until = parseDateTime(part.substring(part.indexOf('=')+1), "UTC");
    }
    else if(part.startsWith("WKST")) {
      ; // Just ignore
    }
    else {
      Panic("Unsupported recurence rule");
    }

  } while(end != -1);

  
  time_t nowish = now();

  if(until < nowish) {
    startTime = until;
  } 
  else {
    time_t midnight;
    int dayOfWeek;
    int daysUntilNext;

    if(nowish < startTime) {
      midnight = previousMidnight(startTime);
    } else {
      midnight = previousMidnight(nowish);
    }
    switch(freq) {
      case DAILY:
        startTime = midnight + elapsedSecsToday(startTime);
        break;
      case WEEKLY:
        dayOfWeek = dayOfWeek(midnight);
        daysUntilNext= -1;

        for(int i = 0; i < 7; i++) {
          int testDay = (i + dayOfWeek - 1)  % 7;

          if(days[testDay]) {
            daysUntilNext = i;
            break;
          }
        }
        if(daysUntilNext < 0) Panic("No day set in weekly reccurrence");

        startTime = midnight + elapsedSecsToday(startTime) + (daysUntilNext * SECS_PER_DAY);
        break;
      case NOFREQ:
        Panic("Unknown frequency");
        break;
    }
  }
  return startTime;
}

struct Event {
  String dtStart;
  String dtEnd;
  String timeZone;
  String rRule;
  String sumary;
};

struct Sunrise getNextEvent() {
  struct Sunrise nextEvent;
  nextEvent.startTime = now() + SECS_PER_WEEK;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setBufferSizes(1024, 1024);
  client->setInsecure();

  bool mfln = client->probeMaxFragmentLength("calendar.google.com", 443, 1024);
  if (mfln) {
    client->setBufferSizes(1024, 1024);
  }

  HTTPClient https;

  if (https.begin(*client, String("https://calendar.google.com") + calendarPath)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {

        bool inEvent = false;
        struct Event calevent;

        while (client->available()) {
          String line = client->readStringUntil('\n');

          if(inEvent) {
            if(line.startsWith("SUMMARY:")) {
              calevent.sumary = line.substring(line.indexOf(':') + 1);
            }
            if(line.startsWith("DTSTART:")) {
              calevent.dtStart = line.substring(line.indexOf(':') + 1);
              calevent.timeZone = "UTC";
            }
            if(line.startsWith("DTSTART;")) {
              calevent.dtStart = line.substring(line.indexOf(':') + 1);
              calevent.timeZone = line.substring(line.indexOf('=') + 1, line.indexOf(':'));
            }
            if(line.startsWith("DTEND")) {
              calevent.dtEnd = line.substring(line.indexOf(':') + 1);
            }
            if(line.startsWith("RRULE:")) {
              calevent.rRule = line.substring(line.indexOf(':') + 1);
            }
          }

          if(line.startsWith("BEGIN:VEVENT")) {
            inEvent = true;
            calevent.rRule = "";
          }

          if(line.startsWith("END:VEVENT")) {
            inEvent = false;

            time_t startTime = parseDateTime(calevent.dtStart, calevent.timeZone);
            time_t endTime = parseDateTime(calevent.dtEnd, calevent.timeZone);

            if(calevent.rRule != "") {
              startTime = getNextOccurrence(startTime, calevent.rRule);
              endTime = getNextOccurrence(endTime, calevent.rRule);
            }

            if(startTime >= now() - endTime && startTime < nextEvent.startTime) {
              nextEvent.startTime = startTime;
              nextEvent.dur = endTime - startTime;
            }
          }
        }
      }
    }
    https.end();
  }
  log(String(__FUNCTION__) + ": Next event is at " + timeToString(nextEvent.startTime) + " for "
      + String(nextEvent.dur) + "s");
  return nextEvent;
}

