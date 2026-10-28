#include "AudioProcessor.h"
#include <cassert>
#include <cmath>
static AudioProcessor processor;
int main() {
    processor.Init(48000);
    float energy=0;
    for(int scenario=0; scenario<9; ++scenario)
        for(int i=0;i<24000;++i) {
            const float in=.1f*std::sin(i*.035f);
            float l,r;
            processor.Process(in,-in,scenario/8.f,-20+scenario*7.5f,scenario*12.5f,.65f,.35f,true,false,l,r);
            assert(std::isfinite(l)&&std::isfinite(r));
            assert(std::fabs(l)<=.9951f&&std::fabs(r)<=.9951f);
            energy+=l*l+r*r;
        }
    assert(energy>1.f);
}
