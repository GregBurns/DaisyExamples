/*
The MIT License (MIT)

Copyright (c) 2019 Electrosmith
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
#ifndef _DAISY_PATCH_H
#define _DAISY_PATCH_H

#include <stdio.h>
#include "parameter.h"

extern "C" {
#include "util\oled_fonts.h"
}

class AudioIn;
class AudioOut;

namespace daisy {
    
static const int SSD1309_HEIGHT = 64;
static const int SSD1309_WIDTH = 128;

class System {
    public:
        static uint32_t GetNow();
};

struct SaiHandle {
    struct Config {
        enum class SampleRate {
            SAI_8KHZ,
            SAI_16KHZ,
            SAI_32KHZ,
            SAI_48KHZ,
            SAI_96KHZ,
        };
    };
};

struct AudioHandle {
    /** Non-Interleaving Callback format. Both arrays arranged by float[chn][sample] */
    typedef void (*AudioCallback)(float** in, float** out, size_t size);
};

class Encoder {
    public:
        Encoder() {}
        void Debounce() {};
        int Increment();
        bool RisingEdge();
        bool FallingEdge();
        bool Pressed();
        float TimeHeldMs();
};

class GateIn {
    public:
        GateIn(int id): id(id) {}
        bool Trig();
        bool State();
    private:
        int id;
};

class DaisyPatch {
    public:
        DaisyPatch();

        void Init();

        void DelayMs(int ms);

        enum GateInput {
            GATE_IN_1,
            GATE_IN_2,
            GATE_IN_LAST
        };

        friend class GateIn;
        friend class Encoder;

        class Display {
            public:
                Display(){}
                void Fill(bool on);
                void SetCursor(int x, int y) { currentX = x; currentY = y; }
                void DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool on);
                void DrawPixel(uint8_t x, uint8_t y, bool on);
                char WriteChar(char ch, FontDef font, bool on);
                char WriteString(const char* txt, FontDef font, bool on);
                void DrawRect(int x1, int y1, int x2, int y2, bool on, bool fill);
                void Update();
            private:
                bool change;
                int currentX;
                int currentY;
                uint8_t buffer[SSD1309_WIDTH * SSD1309_HEIGHT / 8];
        };

        void StartAdc() {}
        void StopAdc() {}
        void ProcessAnalogControls() {}
        void ProcessDigitalControls();
        void ProcessAllControls() { ProcessDigitalControls(); ProcessAnalogControls(); }
        void DisplayControls(bool invert);

        enum Ctrl {
            CTRL_1,    /**< */
            CTRL_2,    /**< */
            CTRL_3,    /**< */
            CTRL_4,    /**< */
            CTRL_LAST, /**< */
        };

        AnalogControl controls[4];

        float GetKnobValue(Ctrl k);

        Encoder encoder;                  /**< Encoder object */
        GateIn  gate_input[GATE_IN_LAST]; /**< Gate inputs  */
        Display display;                  /**< & */

        void SetAudioBlockSize(size_t blockSize) { audioBlockSize = blockSize; }

        void SetAudioSampleRate(SaiHandle::Config::SampleRate sr) {
            if (sampleRateExternal) {
                // Sample rate was fixed by external audio source
                return;
            }
            switch (sr) {
                case SaiHandle::Config::SampleRate::SAI_8KHZ:
                    sampleRate = 8000;
                    break;
                case SaiHandle::Config::SampleRate::SAI_16KHZ:
                    sampleRate = 16000;
                    break;
                case SaiHandle::Config::SampleRate::SAI_32KHZ:
                    sampleRate = 32000;
                    break;
                case SaiHandle::Config::SampleRate::SAI_48KHZ:
                    sampleRate = 48000;
                    break;
                case SaiHandle::Config::SampleRate::SAI_96KHZ:
                    sampleRate = 96000;
                    break;
            }
        }

        float AudioSampleRate() { return (float)sampleRate; }

        void StartAudio(AudioHandle::AudioCallback callback);

        // Internal audio loop
        int AudioLoop();

        // Pointer to the single patch instance
        static DaisyPatch* patchSingleton;

    private:

        bool encoderUp;
        bool encoderDown;
        bool encoderState;
        uint32_t encoderDownTime;
        bool encoderForward;
        bool encoderBackward;
        size_t audioBlockSize;
        bool sampleRateExternal;
        int sampleRate;
        AudioHandle::AudioCallback audioCallback;

        AudioIn* audioIn;
        AudioOut* audioOut;
        bool gateTriggered[2];
        bool gateState[2];
        uint32_t screen_update_last;
        uint32_t screen_update_period;
};

};

#endif
