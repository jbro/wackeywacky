#include "EspMQTTClient.h"
#include "config.h"

#include <FastLED.h>
#define NUM_LEDS 60
#define DATA_PIN 3

const uint8_t gamma8[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

enum LampMode {
  ON,
  OFF,
  ANIMATE_START,
  ANIMATE_RUN
};

struct LampState {
  enum LampMode mode;
  CRGB color;
  uint8_t brightness;
  void (*effect)();
  unsigned long frameDelayMS;
  unsigned long previousFrame;
  CRGB leds[NUM_LEDS];
};

struct LampState lamp;

CRGB stringToCRGB(const String & color) {
  int firstComma = color.indexOf(',');
  int secondComma = color.indexOf(',', firstComma + 1);

  return CRGB(
      color.substring(0, firstComma).toInt(),
      color.substring(firstComma + 1, secondComma).toInt(),
      color.substring(secondComma + 1).toInt());
}

void lampShow() {
  switch (lamp.mode) {
    case ON:
      fill_solid(lamp.leds, NUM_LEDS, lamp.color);
      client.publish("bedroom/wakeuplight/status", "ON");
      client.publish("bedroom/wakeuplight/rgb/status",
          String(lamp.color.r) + "," +
          String(lamp.color.g) + "," +
          String(lamp.color.b));
      break;
    case OFF:
      fill_solid(lamp.leds, NUM_LEDS, CRGB::Black);
      client.publish("bedroom/wakeuplight/status", "OFF");
      break;
  }

  FastLED.setBrightness(gamma8[lamp.brightness]);
  client.publish("bedroom/wakeuplight/brightness/status", String(lamp.brightness));

  client.publish("bedroom/wakeuplight/effect/status", "none");

  FastLED.show();
}

void sunriseAnimation() {
  static uint8_t brightness;
  if (lamp.mode == ANIMATE_START) {
    lamp.frameDelayMS = 3947;
    fill_solid(lamp.leds, NUM_LEDS, CRGB(255, 128, 0));
    brightness = 28;
  }

  if (brightness < 255) {
    FastLED.setBrightness(gamma8[brightness++]);
    FastLED.show();
  } else {
    lamp.mode = OFF;
  }
}

void sunriseFastAnimation() {
  static uint8_t brightness;
  if (lamp.mode == ANIMATE_START) {
    lamp.frameDelayMS = 125;
    fill_solid(lamp.leds, NUM_LEDS, CRGB(255, 128, 0));
    brightness = 28;
  }

  if (brightness < 255) {
    FastLED.setBrightness(gamma8[brightness++]);
    FastLED.show();
  } else {
    lamp.mode = OFF;
  }
}

void rainbowAnimation() {
  static uint8_t hue;
  if (lamp.mode == ANIMATE_START) {
    lamp.frameDelayMS = 10;
    hue = 0;
  }
  fill_rainbow(lamp.leds, NUM_LEDS, hue++);
  FastLED.show();
}

void onConnectionEstablished() {
  client.subscribe("bedroom/wakeuplight/switch", [](const String& payload) {
    if (payload == "ON") {
      if(lamp.mode != ANIMATE_RUN && lamp.mode != ANIMATE_START) {
        lamp.mode = ON;
        lampShow();
      }
    } else {
      lamp.mode = OFF;
      lampShow();
    }
  });

  client.subscribe("bedroom/wakeuplight/brightness/set", [](const String &payload) {
      if(lamp.mode != ANIMATE_RUN && lamp.mode != ANIMATE_START) {
        lamp.brightness = payload.toInt();
        lampShow();
      }
  });

  client.subscribe("bedroom/wakeuplight/rgb/set", [](const String &payload) {
      if(lamp.mode != ANIMATE_RUN && lamp.mode != ANIMATE_START) {
        lamp.color = stringToCRGB(payload);
        lampShow();
      }
  });

  client.subscribe("bedroom/wakeuplight/effect/set", [] (const String & payload) {
      if (payload == "rainbow") {
        lamp.effect = &rainbowAnimation;
        lamp.mode = ANIMATE_START;
        client.publish("bedroom/wakeuplight/effect/status", "rainbow");
      }
      if (payload == "sunrise") {
        lamp.effect = &sunriseAnimation;
        lamp.mode = ANIMATE_START;
        client.publish("bedroom/wakeuplight/effect/status", "sunrise");
      }
      if (payload == "sunrise_fast") {
        lamp.effect = &sunriseFastAnimation;
        lamp.mode = ANIMATE_START;
        client.publish("bedroom/wakeuplight/effect/status", "sunrise_fast");
      }
  });

  lampShow();
}

void setup() {
  Serial.begin(115200);
  client.enableDebuggingMessages();

  lamp.mode = OFF;
  lamp.color = CRGB(255, 128, 0); // Sunrise orange
  lamp.brightness = 0xFF;
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(lamp.leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,1800);
  FastLED.setCorrection(TypicalPixelString);
  FastLED.setTemperature(Candle);

  fill_solid(lamp.leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  client.enableHTTPWebUpdater();
}

void animate() {
  switch (lamp.mode) {
    case ANIMATE_START:
      lamp.frameDelayMS = 40; // Default about 24 fps, can be overidden by animation function
      lamp.effect();
      lamp.previousFrame = millis();
      lamp.mode = ANIMATE_RUN;
    break;
    case ANIMATE_RUN:
      if (millis() - lamp.previousFrame >= lamp.frameDelayMS) {
        lamp.previousFrame = millis();
        lamp.effect();
      }
    break;
  }
}

void loop() {
  client.loop();
  animate();
}

