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
#include "daisy_patch.h"
#include "cmdline.h"
#include "audioin.h"
#include "audioout.h"
#include <windows.h>
#include <commctrl.h>

using namespace daisy;

#define DBGONLY(s)  do { if (CommandLine::debug) { s; } } while(0);

// Pointer to the patch instance
DaisyPatch* DaisyPatch::patchSingleton = NULL;

static HANDLE audioThread = 0;
static HANDLE windowThread = 0;

static CONDITION_VARIABLE controlEvent;
static CRITICAL_SECTION controlCS;
static CRITICAL_SECTION displayCS;

typedef enum {
    ControlNone,
    EncoderForward,
    EncoderBackward,
    EncoderClick,
    Gate1Trigger,
    Gate1Release,
    Gate2Trigger,
    Gate2Release,
    Ctrl1,
    Ctrl2,
    Ctrl3,
    Ctrl4
} Controls;

static Controls control = ControlNone;
static float controlVal[4];

static uint8_t bmpBits[SSD1309_WIDTH * daisy::SSD1309_HEIGHT / 8];

static void DrawBitmap(HDC hdc, int scale, int yoffset)
{
    int xoffset = 20;
    RECT rect{ xoffset, yoffset, xoffset + (20 + SSD1309_WIDTH) * scale, yoffset + (20 + daisy::SSD1309_HEIGHT) * scale };
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    HBITMAP hBitmap = CreateBitmap(SSD1309_WIDTH, daisy::SSD1309_HEIGHT, 1, 1, bmpBits); 
    HDC hdcMem = CreateCompatibleDC(hdc);
    HGDIOBJ hObj = SelectObject(hdcMem, hBitmap);
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    if (!StretchBlt(hdc, xoffset + 10 * scale, yoffset + 10 * scale, scale * bm.bmWidth, scale * bm.bmHeight, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY)) {
        printf("StretchBlt failed %d\n", GetLastError());
    }
    SelectObject(hdcMem, hObj);
    DeleteDC(hdcMem);
    DeleteObject(hBitmap);
}

// Handles all of the window drawing and control events
static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    static HWND hTrig[2];
    static bool gate[2] = { false, false };
    HDC hdc;
    PAINTSTRUCT ps;
    Controls ctrl = ControlNone;
    int idx;
    LRESULT res = 0;
    static const int CtrlRange = 5000;

    switch (message) {
        case WM_CREATE:
            for (int ctrl = 0; ctrl < 4; ++ctrl) {
                CHAR label[16];
                DWORD yOffset = 10 + ctrl * 45;
                sprintf_s(label, "CTRL %d    ", ctrl + 1);
                HWND hLabel = CreateWindow("Static", label, WS_CHILD | WS_VISIBLE, 50, yOffset, 50, 15, hwnd, (HMENU)(10 + ctrl), NULL, NULL);
                HWND hTrack = CreateWindow(TRACKBAR_CLASS, "Trackbar Control", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 70, yOffset, 260, 30, hwnd, (HMENU)ctrl, NULL, NULL);
                SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELONG(0, CtrlRange));
                SendMessage(hTrack, TBM_SETPAGESIZE, 0, 200);
                SendMessage(hTrack, TBM_SETTICFREQ, CtrlRange / 20, 0);
                SendMessage(hTrack, TBM_SETPOS, FALSE, 0);
                SendMessage(hTrack, TBM_SETBUDDY, TRUE, (LPARAM)hLabel);
            }
            hTrig[0] = CreateWindow("BUTTON", "GATE 1", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_NOTIFY, 350, 20, 65, 30, hwnd, (HMENU)1, NULL, NULL); 
            hTrig[1] = CreateWindow("BUTTON", "GATE 2", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_NOTIFY, 350, 70, 65, 30, hwnd, (HMENU)2, NULL, NULL);
            break;
        case WM_CHAR:
            if (wparam == VK_ESCAPE) {
                DestroyWindow(hwnd);
            }
            /* Falling through */
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            EnterCriticalSection(&displayCS);
            DrawBitmap(hdc, 2, 200);
            LeaveCriticalSection(&displayCS);
            EndPaint(hwnd, &ps);
            return 0;
        case WM_HSCROLL:
            idx = (int)GetMenu((HWND)lparam);
            controlVal[idx] = (float)SendMessage((HWND)lparam, TBM_GETPOS, 0, 0) / (float)CtrlRange;
            switch (idx) {
                case 0:
                    ctrl = Ctrl1;
                    break;
                case 1:
                    ctrl = Ctrl2;
                    break;
                case 2:
                    ctrl = Ctrl3;
                    break;
                case 3:
                    ctrl = Ctrl4;
                    break;
                default:
                    break;
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
            ctrl = EncoderClick;
            break;
        case WM_MOUSEWHEEL:
            ctrl = (short)HIWORD(wparam) >= 0 ? EncoderForward : EncoderBackward;
            break;
        default:
            // A standard windows push button sends a CLICK event when the button
            // is released and won't return a CLICK is the mouse moves away from
            // the button while it is pressed. For the GATE behavior we want a
            // a TRIGGER when the button is pressed and a RELEASE when the button
            // is released.
            for (int g = 0; g < 2; ++g) {
                if (SendMessage(hTrig[g], BM_GETSTATE, 0, 0) & BST_PUSHED) {
                    if (!gate[g]) {
                        gate[g] = true;
                        ctrl = (g == 0) ? Gate1Trigger : Gate2Trigger;
                    }
                } else {
                    if (gate[g]) {
                        gate[g] = false;
                        ctrl = (g == 0) ? Gate1Release : Gate2Release;
                    }
                }
            }
            res = DefWindowProc(hwnd, message, wparam, lparam);
    }
    // Report control event
    if (ctrl) {
        EnterCriticalSection(&controlCS);
        control = ctrl;
        LeaveCriticalSection(&controlCS);
        WakeConditionVariable(&controlEvent);
    }
    return res;
}

static HWND hwnd;

static DWORD WINAPI WindowThread(LPVOID lpParam)
{
    WNDCLASS wc = {0};
    wc.lpszClassName = "Daisy Patch";
    wc.hInstance = NULL;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style= CS_HREDRAW | CS_VREDRAW;

    RegisterClass(&wc);
    hwnd = CreateWindowA(wc.lpszClassName, "Daisy Patch Simulator", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 450, 450, 0, 0, 0, 0);
    ShowWindow(hwnd, SW_RESTORE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DeleteObject(hwnd);
    hwnd = 0;
    ExitProcess(0);
    return 0;
}

void DaisyPatch::ProcessDigitalControls()
{
    encoderForward = false;
    encoderBackward = false;
    encoderClick = false;
    EnterCriticalSection(&controlCS);
    SleepConditionVariableCS(&controlEvent, &controlCS, 0);
    switch (control) {
        case ControlNone:
            break;
        case EncoderForward:
            encoderForward = true;
            break;
        case EncoderBackward:
            encoderBackward = true;
            break;
        case EncoderClick:
            encoderClick = true;
            break;
        case Gate1Trigger:
            gateTriggered[GATE_IN_1] = true;
            gateState[GATE_IN_1] = true;
            DBGONLY(printf("GATE 1 trigger\n"));
            break;
        case Gate2Trigger:
            gateTriggered[GATE_IN_2] = true;
            gateState[GATE_IN_2] = true;
            DBGONLY(printf("GATE 2 trigger\n"));
            break;
        case Gate1Release:
            gateState[GATE_IN_1] = false;
            DBGONLY(printf("GATE 1 release\n"));
            break;
        case Gate2Release:
            gateState[GATE_IN_2] = false;
            DBGONLY(printf("GATE 2 release\n"));
            break;
        case Ctrl1:
            DBGONLY(printf("CTRL 1 = %f\n", controlVal[0]));
            break;
        case Ctrl2:
            DBGONLY(printf("CTRL 2 = %f\n", controlVal[1]));
            break;
        case Ctrl3:
            DBGONLY(printf("CTRL 3 = %f\n", controlVal[2]));
            break;
        case Ctrl4:
            DBGONLY(printf("CTRL 4 = %f\n", controlVal[3]));
            break;
        default:
            break;
    }
    control = ControlNone;
    LeaveCriticalSection(&controlCS);
}

DaisyPatch::DaisyPatch() :
    controls{{CTRL_1}, {CTRL_2}, {CTRL_3}, {CTRL_4} },
    gate_input{ {GATE_IN_1}, {GATE_IN_2} },
    audioBlockSize(48), sampleRateExternal(false), sampleRate(48000)
{
    InitializeConditionVariable(&controlEvent);
    InitializeCriticalSection(&controlCS);
    InitializeCriticalSection(&displayCS);

    windowThread = CreateThread(NULL, 0,  WindowThread, NULL, 0, NULL);
    // Expose this instance
    patchSingleton = this;
}

float DaisyPatch::GetKnobValue(Ctrl k)
{
    EnterCriticalSection(&controlCS);
    float val = controlVal[k];
    LeaveCriticalSection(&controlCS);
    return val;
}

int Encoder::Increment()
{
    if (DaisyPatch::patchSingleton->encoderForward) {
        DaisyPatch::patchSingleton->encoderForward = false;
        return 1;
    }
    if (DaisyPatch::patchSingleton->encoderBackward) {
        DaisyPatch::patchSingleton->encoderBackward = false;
        return -1;
    }
    return 0;
}

bool Encoder::RisingEdge()
{
    if (DaisyPatch::patchSingleton->encoderClick) {
        DaisyPatch::patchSingleton->encoderClick = false;
        return true;
    } else {
        return false;
    }
}

#define SetBit(p, b)  (p)[(b) / 8] |= 1 << (7 - (b) % 8)
#define ClrBit(p, b)  (p)[(b) / 8] &= ~(1 << (7 - (b) % 8))

void DaisyPatch::Display::DrawPixel(uint8_t x, uint8_t y, bool on)
{
    if (x < SSD1309_WIDTH && y < daisy::SSD1309_HEIGHT) {
        int bit = x + y * SSD1309_WIDTH;
        if (on) {
            SetBit(buffer, bit);
        } else {
            ClrBit(buffer, bit);
        }
    }
}

char DaisyPatch::Display::WriteChar(char ch, FontDef font, bool on)
{
    // Check if character is valid
    if (ch < 32 || ch > 126) {
        return 0;
    }
    // Check remaining space on current line
    if ((SSD1309_WIDTH < currentX + font.FontWidth) || SSD1309_HEIGHT < (currentY + font.FontHeight)) {
        // Not enough space on current line
        return 0;
    }
    // Use the font to write
    for (int i = 0; i < font.FontHeight; i++) {
        uint32_t b = font.data[(ch - 32) * font.FontHeight + i];
        for (int j = 0; j < font.FontWidth; j++) {
            if ((b << j) & 0x8000) {
                DrawPixel(currentX + j, currentY + i, on);
            } else {
                DrawPixel(currentX + j, currentY + i, !on);
            }
        }
    }
    // The current space is now taken
    currentX += font.FontWidth;
    // Return written char for validation
    return ch;
}

char DaisyPatch::Display::WriteString(const char* str, FontDef font, bool on)
{
    while (*str) {
        if (WriteChar(*str, font, on) != *str) {
            break;
        }
        str++;
    }
    return *str;
}

void DaisyPatch::Display::DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool on)
{
    int16_t deltaX = abs((int16_t)x2 - (int16_t)x1);
    int16_t deltaY = abs((int16_t)y2 - (int16_t)y1);
    int16_t signX  = ((x1 < x2) ? 1 : -1);
    int16_t signY  = ((y1 < y2) ? 1 : -1);
    int16_t error  = deltaX - deltaY;
    int16_t error2;

    DrawPixel(x2, y2, on);
    while ((x1 != x2) || (y1 != y2)) {
        DrawPixel(x1, y1, on);
        error2 = error * 2;
        if (error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        if (error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }
}

void DaisyPatch::Display::DrawRect(int x1, int y1, int x2, int y2, bool on, bool fill)
{
    if (fill) {
        for (uint8_t x = x1; x <= x2; x++) {
            for (uint8_t y = y1; y <= y2; y++) {
                DrawPixel(x, y, on);
            }
        }
    } else {
        DrawLine(x1, y1, x2, y1, on);
        DrawLine(x2, y1, x2, y2, on);
        DrawLine(x2, y2, x1, y2, on);
        DrawLine(x1, y2, x1, y1, on);
    }
}

void DaisyPatch::Display::Fill(bool on)
{
    memset(buffer, on ? 0xFF : 0, sizeof(buffer));
}

void DaisyPatch::Display::Update()
{
    Sleep(2);
    EnterCriticalSection(&displayCS);
    if (memcmp(bmpBits, buffer, sizeof(buffer)) != 0) {
        memcpy(bmpBits, buffer, sizeof(buffer));
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
    }
    LeaveCriticalSection(&displayCS);
}

void DaisyPatch::DelayMs(int ms)
{
    Sleep(ms);
}

static DWORD WINAPI AudioThread(LPVOID lpParam)
{
    DaisyPatch* patch = reinterpret_cast<daisy::DaisyPatch*>(lpParam);
    return patch->AudioLoop();
}

int DaisyPatch::AudioLoop()
{
    float* inBuf[4];
    float* outBuf[4];

    for (int c = 0; c < 4; ++c) {
        inBuf[c] = new float[audioBlockSize];
        outBuf[c] = new float[audioBlockSize];
    }
    while (1) {
        // Get next audio block from pre-configured audio source 
        int read = audioIn->Read(inBuf, audioBlockSize);
        if (read == 0) {
            return 0;
        }
        audioCallback(inBuf, outBuf, audioBlockSize);
        // Pacing is controlled by audioOut
        int wrote = audioOut->Write(outBuf, audioBlockSize);
    }
}

void DaisyPatch::StartAudio(AudioHandle::AudioCallback callback)
{
    // If generating a waveform we wait until we know the sample rate
    if (CommandLine::waveform != AudioIn::WAVE_NONE) {
        audioIn->InitWaveform(CommandLine::waveform, sampleRate);
    }
    audioOut = new AudioOut(audioBlockSize, 2, sampleRate);
    audioCallback = callback;
    audioThread = CreateThread(NULL, 0,  AudioThread, (void*)this, 0, NULL);
}

// This will render the display with the controls as vertical bars
void DaisyPatch::DisplayControls(bool invert)
{
    bool on = invert ? false : true;
    bool off = invert ? true : false;
    if (System::GetNow() - screen_update_last > screen_update_period) {
        // Graph Knobs
        size_t barwidth   = 15;
        size_t barspacing = 20;
        display.Fill(off);
        // Bars for all four knobs.
        for (size_t i = 0; i < DaisyPatch::CTRL_LAST; i++) {
            size_t curx = (barspacing * i + 1) + (barwidth * i);
            size_t cury = SSD1309_HEIGHT;
            float v = GetKnobValue(static_cast<DaisyPatch::Ctrl>(i));
            size_t dest = (size_t)(v * SSD1309_HEIGHT);
            for (size_t j = dest; j > 0; j--) {
                for (size_t k = 0; k < barwidth; k++) {
                    display.DrawPixel((uint8_t)(curx + k), (uint8_t)(cury - j), on);
                }
            }
        }
        display.Update();
        screen_update_last = System::GetNow();
    }
}

uint32_t System::GetNow()
{
    SYSTEMTIME time;
    GetSystemTime(&time);
    uint32_t time_ms = (time.wSecond * 1000) + time.wMilliseconds;
    return time_ms;
}

#define MAX_ARGS 16

static int Usage(const char* prog, const char* msg = "Missing arguments")
{
    printf("Error: %s\n\r", msg);
    printf("Usage: %s [-w <waveform>] | [-f <wav-file>]\n\r", prog);
    ExitProcess(-1);
}

void DaisyPatch::Init()
{
    if (!CommandLine::Parse()) {
        ExitProcess(-1);
    }
    audioIn = new AudioIn();
    // If we are opening a wav file do so now
    // so we can report the error early.
    if (CommandLine::wavFile) {
        // sampleRate is an OUT parameter
        if (!audioIn->OpenWavFile(CommandLine::wavFile, CommandLine::loopAudio, sampleRate)) {
            printf("Error: %s could not open %s\n\r", CommandLine::appName, CommandLine::wavFile);
            ExitProcess(-1);
        }
        // Need to use the sample rate from the input file
        // so ignore sample rate set by application
        sampleRateExternal = true;
    }
    // Set Screen update vars
    screen_update_period = 17; // roughly 60Hz
    screen_update_last = System::GetNow();

}

bool GateIn::Trig()
{
    if (DaisyPatch::patchSingleton->gateTriggered[id]) {
        DaisyPatch::patchSingleton->gateTriggered[id] = false;
        return true;
    } else {
        return false;
    }
}

bool GateIn::State()
{
    return DaisyPatch::patchSingleton->gateState[id];
}
