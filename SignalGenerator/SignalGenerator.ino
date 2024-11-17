#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // Included in the Pi Pico Arduino board setup, select from Tools->USB Stack // Don't install this library separately
#include <EEPROM.h>

#define MISO D0
#define CS1 D1
#define SCK D2
#define MOSI D3
#define LATCH_DAC D4

SPISettings spisettings(16000000, MSBFIRST, SPI_MODE0);

const int FS = 40000;
const float FREQ = 1000.0f;
const int MICROS_PER_SAMPLE = 1000000 / FS; // ensure this is an integer result!
const float FULL_SCALE_2VPP = 111.65;
float phase = 0; // 0 ... 1
float inc = FREQ / FS;
uint64_t lastTimestamp = 0;
uint16_t value = 0;

uint8_t tx[2] = {0};
uint8_t rx[2] = {0};

float measuredRms = 0.0f;
float scalerFloat = 2047.0f;
float scalerFloatFiltered = 2047.0f;

float targetRms = FULL_SCALE_2VPP;
float targetRmsFiltered = FULL_SCALE_2VPP;
float filteredOutputValue = 0.0f;
float outputSum = 0.0f;
float scaleCompensation = 0.0f;
uint32_t idx = 0;

void setup() 
{
    // Initial delay ensures successful startup
    delay(100);

    Serial.begin(115200);
    EEPROM.begin(512);
    
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
    pinMode(MISO, INPUT);

    pinMode(CS1, OUTPUT);
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(LATCH_DAC, OUTPUT);

    digitalWrite(CS1, HIGH);
    digitalWrite(LATCH_DAC, HIGH);

    SPI.setRX(MISO);
    SPI.setCS(CS1);
    SPI.setSCK(SCK);
    SPI.setTX(MOSI);
    SPI.begin(false);

    while(!Serial) {}

    EEPROM.get(0, scaleCompensation);
    Serial.print("Loaded compensation value from EEPROM: ");
    Serial.println(scaleCompensation);
    if (!(scaleCompensation < 3 && scaleCompensation > -3))
    {
      scaleCompensation = 0; // most likely unprogrammed unit
      Serial.println("Resetting compensation to zero");
    }

    delay(100);
}

void setup1()
{
    delay(350);
}


void loop()
{
    float outputValue = analogRead(A0);
    float trimValue = analogRead(A1);

    if (trimValue > 900)
        targetRms = FULL_SCALE_2VPP;
    else
        targetRms = FULL_SCALE_2VPP * trimValue * 0.0011111111111111f; // / 900;

    targetRmsFiltered = targetRmsFiltered * 0.999f + targetRms * 0.001f;

    filteredOutputValue = filteredOutputValue * 0.9999f + outputValue * 0.0001f;
    outputValue = outputValue - filteredOutputValue;

    outputSum = outputSum * 0.9999f + (outputValue*outputValue) * 0.0001f;
    float rms = sqrt(outputSum);

    float diff = rms - targetRmsFiltered;
    if (diff > 0.05) scaleCompensation -= 0.000001;
    else if (diff < -0.05) scaleCompensation += 0.000001;
    scalerFloat = (11.79 + scaleCompensation) * targetRms;
    if (scalerFloat > 2047) scalerFloat = 2047;
    if (scalerFloat < 0) scalerFloat = 0;
    scalerFloatFiltered = scalerFloatFiltered * 0.999f + scalerFloat * 0.001f;

    idx++;

    if (idx % 10000 == 0)
    {
        Serial.print("RMS Target: ");
        Serial.print(targetRmsFiltered, 4);

        Serial.print("   RMS value: ");
        Serial.print(rms, 4);

        Serial.print("   Scaler: ");
        Serial.print(scalerFloatFiltered, 4);

        Serial.print("   Diff: ");
        Serial.print(diff, 4);

        Serial.print("   Scale Compensation: ");
        Serial.println(scaleCompensation, 4);
    }

    if (idx == 1000000)
    {
        Serial.print("Saving Scale compensation factor to EEPROM: ");
        Serial.println(scaleCompensation);
        EEPROM.put(0, scaleCompensation);
        EEPROM.commit();
    }
}

void loop1()
{
    while (micros() - lastTimestamp < MICROS_PER_SAMPLE) // ensure samplerate
    {
        // wait until next sample
    }

    lastTimestamp = micros();

    value = sinf(phase * 2 * M_PI) * scalerFloatFiltered + 2048;
    phase += inc;
    if (phase >= 1.0f) phase -= 1.0f;

    // REGISTER 5-1 in datasheet. Channel A, 1x gain, activate channel
    tx[0] = 0b00110000 | (value >> 8);
    tx[1] = value & 0xFF;

    digitalWrite(CS1, LOW);
    //digitalWrite(CS1, LOW);
    //digitalWrite(CS1, LOW);
    //digitalWrite(CS1, LOW);

    SPI.beginTransaction(spisettings);
    SPI.transfer(tx, 2);
    SPI.endTransaction();

    digitalWrite(CS1, HIGH);
    //digitalWrite(CS1, HIGH);
    //digitalWrite(CS1, HIGH);
    //digitalWrite(CS1, HIGH);

    digitalWrite(LATCH_DAC, LOW);
    digitalWrite(LATCH_DAC, LOW);
    digitalWrite(LATCH_DAC, LOW);
    digitalWrite(LATCH_DAC, LOW);
    digitalWrite(LATCH_DAC, HIGH);
}
