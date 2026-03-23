#include "Arduino.h"
#include "communication.h"

void setup()
{
    Serial.begin(115200);
    delay(500);
    communication_setup(nullptr);  // nullptr = no playback callback needed for this test
}

void loop()
{
    // nothing — self test runs once inside communication_setup()
    delay(1000);
}

/*
#include "Arduino.h"
#include "communication.h"
#include "driver/i2s.h"

#define PTT_PIN       33
#define I2S_MIC_PORT  I2S_NUM_1
#define SAMPLES       80   // matches PAYLOAD_MAX_SAMPLES

// Called by communication layer when audio arrives decrypted
// Teammate writes this — sends samples to DAC / I2S speaker
void on_audio_received(const int16_t *samples, size_t num_samples)
{
    size_t bytes_written;
    i2s_write(I2S_NUM_0, samples,
              num_samples * sizeof(int16_t),
              &bytes_written, portMAX_DELAY);
}

void setup()
{
    pinMode(PTT_PIN, INPUT_PULLUP);
    communication_setup(on_audio_received);
}

void loop()
{
    // PTT held = transmit
    if (digitalRead(PTT_PIN) == LOW) {
        int16_t buffer[SAMPLES];
        size_t bytes_read = 0;
        i2s_read(I2S_MIC_PORT, buffer, sizeof(buffer),
                 &bytes_read, portMAX_DELAY);

        size_t samples_read = bytes_read / sizeof(int16_t);
        if (samples_read > 0) {
            communication_send(buffer, samples_read);
        }
    }

    communication_loop();
    delay(2);
}
    */