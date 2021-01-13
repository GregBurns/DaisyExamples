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
#include <stdio.h>
#include "audioout.h"

void AudioOut::Callback()
{
    EnterCriticalSection(&cs);
    ++numFreeHdrs;
    LeaveCriticalSection(&cs);
}

static void CALLBACK WaveOutCB(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    /*
     * ignore calls that occur due to opening and closing the device.
     */
    if (uMsg != WOM_DONE) {
        return;
    }
    AudioOut* audioOut = reinterpret_cast<AudioOut*>(dwInstance);
    audioOut->Callback();
}

void AudioOut::OpenAudioDevice(int numChannels, int sampleRate)
{
    WAVEFORMATEX wfx;

    wfx.nSamplesPerSec = sampleRate; 
    wfx.wBitsPerSample = 16;
    wfx.nChannels = numChannels;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    // Open the default wave device.
    if (waveOutOpen(&hAudio, WAVE_MAPPER, &wfx, (DWORD_PTR)WaveOutCB, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        printf("Unable to open default audio device\r\n");
        ExitProcess(1);
    }
	printf("Audio device opened\r\n");
}

AudioOut::AudioOut(int frameSize, int numChannels, int sampleRate) :
    audioBufSize(numChannels * frameSize * sizeof(int16_t)), numHdrs(sampleRate / (8 * frameSize))
{
    // Allocate an array of wave headers each with its own buffer
    waveHdrs = new WAVEHDR[numHdrs];
    for (int i = 0; i < numHdrs; i++) {
        memset(&waveHdrs[i], 0, sizeof(WAVEHDR));
        waveHdrs[i].dwBufferLength = audioBufSize;
        waveHdrs[i].lpData = (LPSTR)malloc(audioBufSize);
    }
    numFreeHdrs = numHdrs;
    currentHdr = 0;
    InitializeCriticalSection(&cs);
    OpenAudioDevice(numChannels, sampleRate);
}

AudioOut::~AudioOut()
{
    // Wait for audio to flush
    EnterCriticalSection(&cs);
    while (numFreeHdrs < numHdrs) {
        LeaveCriticalSection(&cs);
        Sleep(2);
        EnterCriticalSection(&cs);
    }
    LeaveCriticalSection(&cs);
    DeleteCriticalSection(&cs);

    for (int i = 0; i < numHdrs; i++) {
        if (waveHdrs[i].dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(hAudio, &waveHdrs[i], sizeof(WAVEHDR));
        }
        free(waveHdrs[i].lpData);
    }
    delete waveHdrs;
    waveOutClose(hAudio);
}

void AudioOut::WriteFrame(int16_t* audio)
{
    WAVEHDR* hdr = &waveHdrs[currentHdr];
    /* 
     * first make sure the header we're going to use is unprepared
     */
    if (hdr->dwFlags & WHDR_PREPARED) {
        waveOutUnprepareHeader(hAudio, hdr, sizeof(WAVEHDR));
    }
    memcpy(hdr->lpData, audio, audioBufSize);
    waveOutPrepareHeader(hAudio, hdr, sizeof(WAVEHDR));
    waveOutWrite(hAudio, hdr, sizeof(WAVEHDR));
    EnterCriticalSection(&cs);
    --numFreeHdrs;
    /*
     * wait for a block to become free
     */
    while (!numFreeHdrs) {
        LeaveCriticalSection(&cs);
        Sleep(2);
        EnterCriticalSection(&cs);
    }
    LeaveCriticalSection(&cs);
    /*
     * point to the next block
     */
    currentHdr = (currentHdr + 1) % numHdrs;
}

size_t AudioOut::Write(float* outBuf[4], size_t size)
{
    bool clipping = false;
    int16_t* samplesOut = new int16_t[2 * size];
    // Write interleaved output channels
    int16_t* samples = samplesOut;
    for (size_t i = 0; i < size; ++i) {
        for (int c = 0; c < 4; c += 2) {
            // Down mix to two channels 0/1 -> L  2/3 -> R
            int out = (int)(outBuf[c][i] * 16384.0f) + (int)(outBuf[c + 1][i] * 16384.0f);
            if (out > 32767) {
                out = 32767;
                clipping = true;
            } else if (out < -32767) {
                out = -32767;
                clipping = true;
            }
            *samples++ = (int16_t)out;
        }
    }
    WriteFrame(samplesOut);
    delete(samplesOut);
    if (clipping) {
        printf("Clipping\n");
    }
    return size;
}
