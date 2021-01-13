/*
The MIT License (MIT)

Copyright (c) 2020/2021 Gregory Burns  @gregsbrain

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _AUDIOIN_H
#define _AUDIOIN_H

#include "sndfile.h"

class AudioIn {

    public:

        typedef enum {
            WAVE_NONE,
            WAVE_SQUARE,
            WAVE_TRIANGLE,
            WAVE_SAW,
            WAVE_SIN
        } Waveform;

        AudioIn() : sf(NULL), waveform(WAVE_NONE) {}

        // Returns true and the sample rate if file was successfully opened
        bool OpenWavFile(const char* fileName, bool loop, int& sampleRate);

        // Returns true if waveform was successfully initialized
        bool InitWaveform(Waveform waveform, int sampleRate, float freq = 880.0f);

        size_t Read(float* samples[4], size_t size);

    private:

        size_t Generate(float* samples, size_t size);

        SNDFILE* sf;
        SF_INFO sfInfo;
        Waveform waveform;
        float cycleLen;
        float step;
        float pos;
        bool loop;

};

#endif
