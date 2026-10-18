#pragma once
#include "audio/NativePcmPipe.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
inline void checkPcm(bool ok, const std::string &message)
{
    if (!ok) throw std::runtime_error(message);
}
using tonal::audio::NativePcmPipe;
inline void pipeDiscardAlignmentTest()
{
#ifdef _WIN32
    // stdout/OS pipes are byte streams: a read/flush may split a float or
    // stereo frame, even when the decoder originally writes complete frames.
    const float pcm[] = {.125f, -.25f, .375f, -.5f, .625f, -.75f};
    const auto *bytes = reinterpret_cast<const char *>(pcm);
    for (int offset = 1; offset < 8; ++offset)
        for (bool pending : {false, true})
            for (bool repeated : {false, true})
            {
                NativePcmPipe pipe;
                auto write = [&](const char *data, DWORD size) {
                    DWORD written = 0;
                    checkPcm(WriteFile(reinterpret_cast<HANDLE>(pipe.writer()), data, size,
                                    &written, nullptr) && written == size, "fragmented PCM write");
                };
                std::vector<float> samples;
                write(bytes, offset);
                if (pending)
                    checkPcm(pipe.read(samples, 512) == 0, "partial frame retained before flush");
                pipe.discard();
                if (repeated)
                {
                    pipe.discard(); // no new bytes must not forget the boundary
                    write(bytes + offset, 1);
                    pipe.discard();
                }
                const int consumed = offset + int(repeated);
                write(bytes + consumed, DWORD(sizeof(pcm) - consumed));
                checkPcm(pipe.read(samples, 512) == 2 &&
                          samples == std::vector<float>(pcm + 2, pcm + 6),
                      "PCM alignment after discard offset=" + std::to_string(offset) +
                          " pending=" + std::to_string(pending) + " repeated=" + std::to_string(repeated));
            }
    std::cout << "PASS 28 partial-frame flush cases: stereo floats stay bit-identical\n";
#endif
}
