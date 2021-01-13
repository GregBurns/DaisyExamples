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
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "audioin.h"
#include <stdio.h>
#include <string.h>

bool AudioIn::OpenWavFile(const char* fileName, bool loop, int& sampleRate)
{
    if (!(sf = sf_open(fileName, SFM_READ, &sfInfo))) {
        printf("Could not open %s to read\n", fileName);
        return false;
    }
    // Check audio file has the required format
    if ((sfInfo.channels > 2) || ((sfInfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16)) {
        printf("Audio must be 16 bit PCM mono or stereo\n");
        sf_close(sf);
        return false;
    }
    this->loop = loop;
    sampleRate = sfInfo.samplerate;
    return true;
}

size_t AudioIn::Read(float* samples[4], size_t size)
{
    if (waveform != WAVE_NONE) {
        Generate(samples[0], size);
        memcpy(samples[1], samples[0], size * sizeof(float));
    } else if (sf == NULL) {
        memset(samples[0], 0, size * sizeof(float));
        memset(samples[1], 0, size * sizeof(float));
    } else {
        int16_t* pcm = new int16_t[size * sfInfo.channels];
        size_t rd = (size_t)sf_readf_short(sf, pcm, size);
        if (rd < size) {
            if (loop) {
                // Rewind to start of file and read missing chunk
                sf_seek(sf, 0, SEEK_SET);
                rd += (size_t)sf_readf_short(sf, pcm + (size = rd) * sfInfo.channels, size - rd);
            }
            // Repeat check in case looping and seek didn't work
            if (rd < size) {
                memset(pcm + rd, 0, (size - rd) * sizeof(int16_t) * sfInfo.channels);
                sf_close(sf);
                sf = NULL;
            }
        }
        // De-interleave and scale input samples
        int16_t* ptr = pcm;
        for (size_t i = 0; i < size; ++i) {
            samples[0][i] = (float)*ptr / 32768.0f;
            if (sfInfo.channels == 2) {
                ++ptr;
            }
            samples[1][i] = (float)*ptr++ / 32768.0f;
        }
    }
    // Copy channels 0/1 to 2/3
    memcpy(samples[2], samples[0], size * sizeof(float));
    memcpy(samples[3], samples[1], size * sizeof(float));
    return size;
}

size_t AudioIn::Generate(float* samples, size_t size)
{
    static const float scale = 0.6f;
    static float val = -scale;

    switch (waveform) {
        case WAVE_SIN:
            for (size_t i = 0; i < size; ++i) {
                samples[i] = scale * (float)sin(2.0 * M_PI * pos / cycleLen);
                pos = fmodf(pos + step, cycleLen);
            }
            break;
        case WAVE_SQUARE:
            for (size_t i = 0; i < size; ++i) {
                if (pos < cycleLen / 2) {
                    val = scale;
                } else  {
                    val = -scale;
                }
                samples[i] = val;
                pos = fmodf(pos + step, cycleLen);
            }
            break;
        case WAVE_SAW:
            for (size_t i = 0; i < size; ++i) {
                float n = fmodf(pos + step, cycleLen);
                if (n < pos) {
                    val = -scale;
                } else  {
                    val = 2.0f * scale * (pos / cycleLen - 1.0f);
                }
                samples[i] = val;
                pos = n;
            }
            break;
        case WAVE_TRIANGLE:
            for (size_t i = 0; i < size; ++i) {
                if (pos < cycleLen / 2) {
                    val = 2.0f * scale * pos / cycleLen;
                } else  {
                    val = 2.0f * scale * (1.0f - pos / cycleLen);
                }
                samples[i] = val - scale;
                pos = fmodf(pos + step, cycleLen);
            }
            break;
        default:
            break;
    }
    return size;
}

bool AudioIn::InitWaveform(Waveform waveform, int sampleRate, float freq)
{
    if (sf) {
        return false;
    }
    this->waveform = waveform;
    step = 1.0f / (float)sampleRate;
    cycleLen = (float)sampleRate / freq * step;
    pos = 0.0f;
    return true;
}
