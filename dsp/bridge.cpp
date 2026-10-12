#include "AudioProcessor.h"
#include <cmath>
static AudioProcessor processor;
static float input[256], output[256];
extern "C" {
void ultra_init(float rate) { processor.Init(rate); }
float* ultra_input() { return input; }
float* ultra_output() { return output; }
void ultra_process(float light, float temperature, float humidity, float mix, float volume) {
    if(!std::isfinite(light) || !std::isfinite(temperature) || !std::isfinite(humidity)
       || !std::isfinite(mix) || !std::isfinite(volume)) {
        for(auto& sample : output) sample = 0;
        return;
    }
    for(int i=0; i<128; ++i)
        processor.Process(input[i], input[i+128], light, temperature, humidity,
                          mix, volume, true, false, output[i], output[i+128]);
}
}
