#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // Included in the Pi Pico Arduino board setup, select from Tools->USB Stack // Don't install this library separately
#include <EEPROM.h>
#include "control_TLV320AIC3204.h"
#include <I2S.h>

I2S i2s(OUTPUT);

#define SDA D0
#define SCL D1

#define MISO D2
#define MOSI D3
#define BCLK D4
#define LRCLK D5
#define MCLK D6

#define GPIO_OUT D11
#define CODEC_RESET D12

#define SLIDE3 D22
#define SLIDE2 D23
#define SLIDE1 D24

#define ADC0 A0

float scaler = 1.0f;

AudioControlTLV320AIC3204 codec;

const float inc = 2 * M_PI * 1000.0 / 48000.0;
const int32_t samplerate = 48000;
const float ITERMAX = 2 * M_PI;
float iter = 0.0f;
volatile float amplitude = 1e9;
bool noiseMode = false;
const float RAND_SCALER = (float)(2.0 / RAND_MAX);

void setup() 
{
    // Initial delay ensures successful startup
    delay(100);

    Serial.begin(115200);
    EEPROM.begin(512);
    
    pinMode(MISO, INPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(BCLK, OUTPUT);
    pinMode(LRCLK, OUTPUT);
    pinMode(MCLK, OUTPUT);

    pinMode(GPIO_OUT, OUTPUT);
    pinMode(CODEC_RESET, OUTPUT);

    pinMode(SLIDE3, INPUT);
    pinMode(SLIDE2, INPUT);
    pinMode(SLIDE1, INPUT);

    pinMode(ADC0, INPUT);

    codec.start();
    digitalWrite(CODEC_RESET, 0);
    delay(10);
    digitalWrite(CODEC_RESET, 1);
    delay(10);

    codec.init();
    i2s.setBCLK(BCLK);
    i2s.setDATA(MOSI);
    i2s.setBitsPerSample(32);

    // start I2S at the sample rate with 16-bits per sample
    if (!i2s.begin(samplerate)) 
    {
        Serial.println("Failed to initialize I2S!");
        while (1); // do nothing
    }
    else
    {
      Serial.println("I2S started!");
    }

    delay(100);
}

void setup1()
{
    delay(1000);
}

int slideCounters[3] = {0};
int activeCounter = 0;

void updateSlideCounters()
{
    auto s0 = digitalRead(SLIDE1);
    auto s1 = digitalRead(SLIDE2);
    auto s2 = digitalRead(SLIDE3);

    if (s0)
    {
      slideCounters[0]++;
      slideCounters[1]--;
      slideCounters[2]--;
      if (slideCounters[0] > 10) slideCounters[0] = 10;
      if (slideCounters[1] < 0) slideCounters[1] = 0;
      if (slideCounters[2] < 0) slideCounters[2] = 0;
    }
    else if (s1)
    {
      slideCounters[0]--;
      slideCounters[1]++;
      slideCounters[2]--;
      if (slideCounters[0] < 0) slideCounters[0] = 0;
      if (slideCounters[1] > 10) slideCounters[1] = 10;
      if (slideCounters[2] < 0) slideCounters[2] = 0;
    }
    else if (s2)
    {
      slideCounters[0]--;
      slideCounters[1]--;
      slideCounters[2]++;
      if (slideCounters[0] < 0) slideCounters[0] = 0;
      if (slideCounters[1] < 0) slideCounters[1] = 0;
      if (slideCounters[2] > 10) slideCounters[2] = 10;
    }
    else
    {
      slideCounters[0]--;
      slideCounters[1]--;
      slideCounters[2]--;
      if (slideCounters[0] < 0) slideCounters[0] = 0;
      if (slideCounters[1] < 0) slideCounters[1] = 0;
      if (slideCounters[2] < 0) slideCounters[2] = 0;
    }

    if (slideCounters[0] == 10) activeCounter = 0;
    if (slideCounters[1] == 10) activeCounter = 1;
    if (slideCounters[2] == 10) activeCounter = 2;
}

void loop1()
{
    float trimValue = analogRead(ADC0);
    float newScaler;
    if (trimValue > 900)
        newScaler = 1.0f;
    else
        newScaler = trimValue * 0.0011111111111111f; // / 900;

    scaler = scaler * 0.99f + newScaler * 0.01f;
    updateSlideCounters();
    
    float slider = 0.0f;
    if (activeCounter == 2) slider = 1.391e9;
    else if (activeCounter == 1) slider = 0.795e9;
    else if (activeCounter == 0) slider = 0.3565e9;

    amplitude = slider * scaler;
    delay(1);
}


void loop()
{
    float s;

    if (noiseMode)
    {
        s = (rand() * RAND_SCALER - 1) * amplitude;
    }
    else
    {
        s = sinf(iter) * amplitude;
        iter += inc;
        if (iter >= ITERMAX) 
          iter -= ITERMAX;
    }
    // write the same sample twice, once for left and once for the right channel
    i2s.write((int32_t)s);
    i2s.write((int32_t)s);
}
