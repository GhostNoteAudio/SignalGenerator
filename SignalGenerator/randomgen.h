#include <stdint.h>

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    int32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint8_t ctz(uint32_t value)
{
    if ((value & 1) != 0) return 0;
    if ((value & 2) != 0) return 1;
    if ((value & 4) != 0) return 2;
    if ((value & 8) != 0) return 3;
    if ((value & 16) != 0) return 4;
    if ((value & 32) != 0) return 5;
    if ((value & 64) != 0) return 6;
    if ((value & 128) != 0) return 7;
    if ((value & 256) != 0) return 8;
    if ((value & 512) != 0) return 9;
    if ((value & 1024) != 0) return 10;
    if ((value & 2048) != 0) return 11;
    if ((value & 4096) != 0) return 12;
    if ((value & 8192) != 0) return 13;
    if ((value & 16384) != 0) return 14;
    if ((value & 32768) != 0) return 15;
    if ((value & 65536) != 0) return 16;
    return 16;
}

class RandGen
{
  uint16_t num_zeroes;
  uint32_t rand_ary[16];
  uint64_t sum = 0;
  pcg32_random_t rng;
  uint32_t count = 0;

public:
    RandGen()
    {
        // Initialize random number generator.
        rng.state = 69012; // Arbitrary starting point in random sequence.
        rng.inc = 0x12345; // Arbitrary choice of which sequence to generate.
        for (int i=0; i<16; i++)
            sum += (rand_ary[i] = pcg32_random_r(&rng));
    }

    int16_t getWhiteNoise()
    {
        return (pcg32_random_r(&rng) >> 16) - 32768;
    }

    int16_t getPinkNoise()
    {
        num_zeroes = ctz(((unsigned int)count)|0x4000); // Force num_zeroes to always be in range 0-14.
        sum -= rand_ary[num_zeroes];
        rand_ary[num_zeroes] = pcg32_random_r(&rng);
        sum += rand_ary[num_zeroes];
        uint64_t total = sum + pcg32_random_r(&rng);
        total = (sum >> 20) - 32768;
        int16_t sample16 = (int16_t)total;
        count++;
        return sample16;
    }
};