#include <FastLED.h>

#define DATA_PIN 5
#define NUM_LEDS 21
#define BRIGHTNESS 100

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<UCS1903B, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  random16_add_entropy(analogRead(0));
}

void loop() {

  // Soft background
  fill_solid(leds, NUM_LEDS, CRGB(15, 5, 0));

  for (int frame = 0; frame < 150; frame++) {

    // Fade existing sparkles
    fadeToBlackBy(leds, NUM_LEDS, 35);

    // Random bright sparkle
    int pixel = random(NUM_LEDS);

    leds[pixel] = CRGB::White;

    FastLED.show();

    delay(70);
  }

  FastLED.clear();
  FastLED.show();

  delay(1000);
}