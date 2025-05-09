#include <Arduino.h>
#include <Adafruit_TinyUSB.h> // Included in the Pi Pico Arduino board setup, select from Tools->USB Stack // Don't install this library separately
#include <EEPROM.h>

/* Settings

150Mhz Overclock
-Ofast optimise
Adafruit TinyUSB
*/

#define MISO D0
#define CS1 D1
#define SCK D2
#define MOSI D3
#define LATCH_DAC D4

SPISettings spisettings(24000000, MSBFIRST, SPI_MODE0);

const int TABLE_SIZE = 100;
float table[] = {0.0, 0.06279051952931337, 0.12533323356430426, 0.1873813145857246, 0.2486898871648548, 0.3090169943749474, 0.3681245526846779, 0.4257792915650727, 0.4817536741017153, 0.5358267949789967, 0.5877852522924731, 0.6374239897486896, 0.6845471059286886, 0.7289686274214116, 0.7705132427757893, 0.8090169943749475, 0.8443279255020151, 0.8763066800438637, 0.9048270524660196, 0.9297764858882513, 0.9510565162951535, 0.9685831611286311, 0.9822872507286886, 0.9921147013144779, 0.9980267284282716, 1.0, 0.9980267284282716, 0.9921147013144778, 0.9822872507286886, 0.9685831611286312, 0.9510565162951536, 0.9297764858882515, 0.9048270524660195, 0.8763066800438635, 0.844327925502015, 0.8090169943749475, 0.7705132427757893, 0.7289686274214114, 0.6845471059286888, 0.6374239897486899, 0.5877852522924732, 0.535826794978997, 0.4817536741017156, 0.4257792915650729, 0.36812455268467814, 0.3090169943749475, 0.24868988716485482, 0.18738131458572502, 0.12533323356430454, 0.06279051952931358, 1.2246467991473532e-16, -0.06279051952931335, -0.12533323356430429, -0.18738131458572477, -0.24868988716485502, -0.30901699437494773, -0.3681245526846783, -0.42577929156507227, -0.481753674101715, -0.5358267949789964, -0.587785252292473, -0.6374239897486896, -0.6845471059286887, -0.7289686274214113, -0.7705132427757894, -0.8090169943749473, -0.8443279255020153, -0.8763066800438636, -0.9048270524660198, -0.9297764858882511, -0.9510565162951535, -0.968583161128631, -0.9822872507286887, -0.9921147013144778, -0.9980267284282716, -1.0, -0.9980267284282716, -0.9921147013144779, -0.9822872507286887, -0.9685831611286311, -0.9510565162951536, -0.9297764858882512, -0.9048270524660199, -0.8763066800438638, -0.8443279255020155, -0.8090169943749476, -0.7705132427757896, -0.7289686274214116, -0.684547105928689, -0.6374239897486896, -0.5877852522924734, -0.5358267949789963, -0.4817536741017153, -0.4257792915650722, -0.3681245526846787, -0.3090169943749477, -0.24868988716485535, -0.18738131458572468, -0.12533323356430465, -0.06279051952931326};
int idx = 0;
uint16_t value = 0;

uint8_t tx[2] = {0};
uint8_t rx[2] = {0};

float measuredRms = 0.0f;
float scalerFloat = 2047.0f;
float scalerFloatFiltered = 2047.0f;

const float FULL_SCALE_2VPP = 115.5f;
float targetRms = FULL_SCALE_2VPP;
float targetRmsFiltered = FULL_SCALE_2VPP;
float filteredOutputValue = 0.0f;
float outputSum = 0.0f;
float scaleCompensation = 0.0f;

int iterations = 0;

const int32_t CLOCK_PER_SAMPLE = rp2040.f_cpu() / (TABLE_SIZE * 1000);
const int32_t SYSTICK_THRES = 0xFFFFFF - CLOCK_PER_SAMPLE - 238;


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

    //while(!Serial) {}
    systick_hw->cvr = 0; // reset systick

    EEPROM.get(0, scaleCompensation);
    //Serial.print("Loaded compensation value from EEPROM: ");
    //Serial.println(scaleCompensation);
    if (!(scaleCompensation < 3 && scaleCompensation > -3))
    {
        scaleCompensation = 0; // most likely unprogrammed unit
        //Serial.println("Resetting compensation to zero");
    }

    delay(100);
}

void setup1()
{
    delay(350);
}


void loop1()
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

    iterations++;

    /*if (iterations % 10000 == 0)
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
    }*/

    if (iterations == 1000000)
    {
        //Serial.print("Saving Scale compensation factor to EEPROM: ");
        //Serial.println(scaleCompensation);
        EEPROM.put(0, scaleCompensation);
        EEPROM.commit();
    }
}

uint64_t lastSampleTime = 0;

void loop()
{
    while(systick_hw->cvr >= SYSTICK_THRES)
    { }

    systick_hw->cvr = 0;
    value = int(table[idx] * scalerFloatFiltered) + 2048;
    idx = (idx + 1) % TABLE_SIZE;

    // REGISTER 5-1 in datasheet. Channel A, 1x gain, activate channel
    tx[0] = 0b00110000 | (value >> 8);
    tx[1] = value & 0xFF;

    gpio_put(CS1, false);
    
    SPI.beginTransaction(spisettings);
    SPI.transfer(tx, 2);
    SPI.endTransaction();

    gpio_put(CS1, true);
    gpio_put(LATCH_DAC, false);
    gpio_put(LATCH_DAC, false);
    gpio_put(LATCH_DAC, true);
}
