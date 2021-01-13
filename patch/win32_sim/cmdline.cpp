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
#include <windows.h>
#include <stdio.h>
#include "cmdline.h"

AudioIn::Waveform CommandLine::waveform = AudioIn::WAVE_NONE;

const char* CommandLine::wavFile = NULL;
const char* CommandLine::appName = NULL;

bool CommandLine::loopAudio = false;
bool CommandLine::debug = false;

bool CommandLine::Usage(const char* msg)
{
    printf("Error: %s\n\r", msg);
    printf("Usage: %s [-d] [-w [sin|square|triangle|saw]] | [-f <wav-file>] [-l]\n\r", appName);
    return false;
}

#define MAX_ARGS 16

bool CommandLine::Parse()
{
    static char* args = _strdup(GetCommandLineA());
    // Tokenize the command line
    static char* argList[MAX_ARGS];
    char** argv = argList;
    int argc = 0;
    while (*args) {
        argv[argc++] = args;
        if (argc == MAX_ARGS) {
            break;
        }
        args = strchr(args, ' ');
        if (!args) {
            break;
        }
        while (*args == ' ') {
            *args = '\0';
            ++args;
        }
    }
    appName = argv[0];
    ++argv;
    --argc;
    while (argc && argv[0][0] == '-') {
        char opt = argv[0][1];
        ++argv;
        --argc;
        switch (opt) {
            case 'd':
                debug = true;
                break;
            case 'l':
                loopAudio = true;
                break;
            case 'w':
                if (argc == 0) {
                    return Usage();
                }
                if (strcmp(argv[0], "square") == 0) {
                    waveform = AudioIn::WAVE_SQUARE;
                } else if (strcmp(argv[0], "sin") == 0) {
                    waveform = AudioIn::WAVE_SIN;
                } else if (strcmp(argv[0], "saw") == 0) {
                    waveform = AudioIn::WAVE_SAW;
                } else if (strcmp(argv[0], "triangle") == 0) {
                    waveform = AudioIn::WAVE_TRIANGLE;
                } else {
                    return Usage("Waveform must square, sin, saw, or triangle");
                }
                ++argv;
                --argc;
                break;
            case 'f':
                if (argc == 0) {
                    return Usage();
                }
                wavFile = argv[0];
                ++argv;
                --argc;
                break;
            default:
                return Usage();
        }
    }
    if (waveform && wavFile) {
        return Usage("Only one of -w and -f allowed");
    }
    return true;
}
