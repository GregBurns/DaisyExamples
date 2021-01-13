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
#ifndef _AUDIOUT_H_
#define _AUDIOUT_H_

#include <Windows.h>
#include <mmsystem.h>

// These interfere with std::max and std::min
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <stdint.h>

class AudioOut {
    public:

        AudioOut(int frameSize, int numChannels, int sampleRate);

        ~AudioOut();

        // Write out 4 channels of audio
        size_t Write(float* audio[4], size_t size);

        void Callback();

    private:

        void OpenAudioDevice(int numChannels, int sampleRate = 44100);
        void WriteFrame(int16_t* audio);

        HWAVEOUT hAudio;
        CRITICAL_SECTION cs;
        WAVEHDR* waveHdrs;
        int audioBufSize;
        int numHdrs;
        volatile int numFreeHdrs;
        int currentHdr;

};

#endif
