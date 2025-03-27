#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // Included in the Pi Pico Arduino board setup, select from Tools->USB Stack // Don't install this library separately
#include <EEPROM.h>
#include "control_TLV320AIC3204.h"
#include <I2S.h>
#include "randomgen.h"

I2S i2s(OUTPUT);

#define SDA D0
#define SCL D1

#define MISO D2
#define MOSI D3
#define BCLK D4
#define LRCLK D5
#define MCLK D6

//#define GPIO_OUT D11
#define CODEC_RESET D12

#define SLIDE3 D22
#define SLIDE2 D23
#define SLIDE1 D24

#define ADC0 A0

float trimGain = 1.0f;

AudioControlTLV320AIC3204 codec;

const float inc = 2 * M_PI * 1000.0 / 48000.0;
const int32_t samplerate = 48000;
const float ITERMAX = 2 * M_PI;
float iter = 0.0f;
volatile int32_t amplitude = 30000;
bool noiseMode = false;
float adjustment = 1.0f;

int slideCounters[3] = {0};
int sliderPos = 0;

void setup() 
{
    // Initial delay ensures successful startup
    delay(100);

    Serial.begin(115200);
    EEPROM.begin(512);

    //while(!Serial) {}

    // load adjustment value from EEPROM
    uint16_t adjVal = *((uint16_t*)&(EEPROM[0]));
    if (adjVal >= 800 && adjVal <= 1200)
    {
        adjustment = adjVal * 0.001f;
        Serial.print("Setting adjument from EEPROM to: ");
        Serial.println(adjustment, 3);
        Serial.println("To change it, send Serial command 'adj=XXXX' where XXXX is between 800 and 1200");
    }
    
    pinMode(MISO, INPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(BCLK, OUTPUT);
    pinMode(LRCLK, OUTPUT);
    pinMode(MCLK, OUTPUT);

    //pinMode(GPIO_OUT, OUTPUT);
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

    delay(500);
}

void setup1()
{
    delay(200);
}

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

    if (slideCounters[0] == 10) sliderPos = 0;
    if (slideCounters[1] == 10) sliderPos = 1;
    if (slideCounters[2] == 10) sliderPos = 2;
}

float trimpotToGain(float value)
{
    if (value >= 512.0f)
    {
        value = (value - 512.0f) / 450.0f;
        if (value < 0.1f) value = 0.1f;
        if (value > 1.0f) value = 1.0f;
    }
    else // value < 512.0f
    {
        value = 511 - value;
        value = value / 450.0f;
        if (value < 0.1f) value = 0.1f;
        if (value > 1.0f) value = 1.0f;
    }

    value = value * value;
    return value;
}

char serialBuffer[32];
int serialCount = 0;

void processSerialInput()
{
    serialBuffer[serialCount] = '\0';
    auto str = String(serialBuffer);
    str.trim();
    Serial.print("Serial data received: ");
    Serial.println(str);
    if (str.startsWith("adj="))
    {
        uint16_t value = str.substring(4).toInt();
        if (value >= 800 && value <= 1200)
        {
            Serial.print("Adjustment value set to: ");
            Serial.println(value);
            EEPROM.put(0, value);
            EEPROM.commit();
            adjustment = value * 0.001f;
            Serial.print("Setting adjument from EEPROM to: ");
            Serial.println(adjustment, 3);
        }
    }
}

const int32_t OUTPUT_SCALER = 15000;

void loop1()
{
    float trimValue = analogRead(ADC0);
    float newTrimGain = trimpotToGain(trimValue);

    if (trimValue < 512 - 20) noiseMode = true;
    else if (trimValue > 512 + 20) noiseMode = false;

    trimGain = trimGain * 0.99f + newTrimGain * 0.01f;
    updateSlideCounters();
    
    float selectedGain = 0.0f;
    if (sliderPos == 2)
        selectedGain = 1.228f;
    else if (sliderPos == 1)
        selectedGain = 0.707f;
    else if (sliderPos == 0)
    {
        if (noiseMode) // in this mode, we want 1vPP pink noise
            selectedGain = 0.707f * 6.8f;
        else
            selectedGain = 0.316f;
    }

    amplitude = selectedGain * trimGain * adjustment * OUTPUT_SCALER;

    while (Serial.available() > 0)
    {
        char ch = Serial.read();
        if (ch == '\n')
        {
            processSerialInput();
            serialCount = 0;
        }
        else if (serialCount < 31)
        {
          serialBuffer[serialCount] = ch;
          serialCount++;
        }
    }

    delay(1);
}

RandGen rng;

void loop()
{
    int32_t sample_out;

    if (noiseMode)
    {
        if (sliderPos == 0)
        {
            sample_out = rng.getPinkNoise() * amplitude;
        }
        else
        {
            sample_out = rng.getWhiteNoise() * amplitude;
        }
    }
    else
    {
        sample_out = (sinf(iter) * 32767.0f) * amplitude;
        iter += inc;
        if (iter >= ITERMAX) 
          iter -= ITERMAX;
    }
    // write the same sample twice, once for left and once for the right channel
    i2s.write(sample_out);
    i2s.write(sample_out);
}
