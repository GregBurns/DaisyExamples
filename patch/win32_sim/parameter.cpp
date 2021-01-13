#include "daisy_patch.h"
#include <math.h>

using namespace daisy;

void Parameter::Init(AnalogControl input, float min, float max, Curve curve)
{
    pmin_   = min;
    pmax_   = max;
    pcurve_ = curve;
    in_     = input;
    lmin_   = logf(min < 0.0000001f ? 0.0000001f : min);
    lmax_   = logf(max);
}

float Parameter::Process()
{
    float knob = DaisyPatch::patchSingleton->GetKnobValue((DaisyPatch::Ctrl)in_.ctrl);
    switch(pcurve_)
    {
        case LINEAR:
            val_ = (knob * (pmax_ - pmin_)) + pmin_;
            break;
        case EXPONENTIAL:
            val_ = ((knob * knob) * (pmax_ - pmin_)) + pmin_;
            break;
        case LOGARITHMIC:
            val_ = expf((knob * (lmax_ - lmin_)) + lmin_);
            break;
        case CUBE:
            val_ = ((knob * (knob * knob)) * (pmax_ - pmin_)) + pmin_;
            break;
        default: break;
    }
    return val_;
}
