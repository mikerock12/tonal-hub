#pragma once
#include "audio/NativePcmPipe.hpp"
#include <iostream>
#include <cstring>
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


inline void pipeTransportEventTest()
{
#ifdef _WIN32
    NativePcmPipe pipe;
    auto write = [](NativePcmPipe &target, const char *data, DWORD size) {
        DWORD written = 0;
        checkPcm(WriteFile(reinterpret_cast<HANDLE>(target.writer()), data, size, &written, nullptr) && written == size,
                 "typed event write");
    };
    auto marker = [](uint32_t left, uint32_t payload) {
        std::vector<char> bytes(8);
        const uint32_t right = 0x7FC00000u | payload;
        std::memcpy(bytes.data(), &left, 4);
        std::memcpy(bytes.data() + 4, &right, 4);
        return bytes;
    };
    const float pcm[] = {.25f, -.25f};
    write(pipe, reinterpret_cast<const char *>(pcm), sizeof(pcm));
    const auto load = marker(0x7FA07A1Cu, 1);
    write(pipe, load.data(), 3);
    write(pipe, load.data() + 3, 5);
    std::vector<float> samples;
    checkPcm(pipe.read(samples, 512) == 1 && samples == std::vector<float>(std::begin(pcm), std::end(pcm)),
             "PCM before transport event stays intact");
    NativePcmPipe::Event event;
    checkPcm(pipe.takeEvent(event) && event.kind == NativePcmPipe::EventKind::UserLoad,
             "fragmented user load event");
    const auto boundary = marker(0x7FA07A1Bu, 1234);
    write(pipe, boundary.data(), DWORD(boundary.size()));
    checkPcm(pipe.read(samples, 512) == 0 && samples.empty(), "marker is not audible PCM");
    checkPcm(pipe.takeEvent(event) && event.kind == NativePcmPipe::EventKind::TrackBoundary && event.positionMs == 1234,
             "track boundary keeps its exact position");
    const auto seek = marker(0x7FA07A1Cu, 2);
    write(pipe, seek.data(), DWORD(seek.size()));
    pipe.read(samples, 512);
    pipe.discard();
    checkPcm(!pipe.takeEvent(event), "discard invalidates a pending transport event");
    std::cout << "PASS typed PCM transport events: split frames, order, boundary position and discard\n";
#endif
}
