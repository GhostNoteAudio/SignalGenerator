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

const int32_t frequency = 1000;
const int32_t amplitude = 1000000000; 
const int32_t sampleRate = 48000;
int32_t sample = amplitude; // current sample value
int32_t count = 0;

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
    if (!i2s.begin(sampleRate)) 
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


void loop1()
{
    float trimValue = analogRead(ADC0);
    float newScaler;
    if (trimValue > 900)
        newScaler = 1.0f;
    else
        newScaler = trimValue * 0.0011111111111111f; // / 900;

    //scaler = scaler * 0.999f + newScaler * 0.001f;
    scaler = newScaler;
    Serial.print("Scaler value: ");
    Serial.println(scaler);

    /*auto regResult = codec.readRegister(0x05, 0);
    Serial.print("Codec register: ");
    Serial.println(regResult, 2); // 0b11000100
    */
    delay(1000);
}


void loop()
{
    if (count % 100 == 0) 
    {
        sample = -sample;
    }

    // write the same sample twice, once for left and once for the right channel
    i2s.write(sample);
    i2s.write(sample);

    // increment the counter for the next sample
    count++;
}
