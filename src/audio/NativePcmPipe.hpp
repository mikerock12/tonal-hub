#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace tonal::audio
{
// Anonymous OS pipe: the reader is paced by OpenAL. QProcess never buffers PCM.
// This provides bounded backpressure to librespot's unpaced pipe sink.
class NativePcmPipe
{
  public:
    NativePcmPipe();
    ~NativePcmPipe();
    bool valid() const;
    uintptr_t writer() const;
    void closeWriter();
    int read(std::vector<float> &, int maxFrames); // bytes stay aligned across partial reads
    void discard();
    // The patched librespot writes one marker frame before the first sample of
    // each track (packaging/prepare-librespot-windows.py). read() stops right
    // before it and removes it: after a read, this says a new track starts at
    // the next frame, and from which position in ms.
    bool takeBoundary(uint32_t &positionMs);
    bool markersSeen() const { return m_markersSeen; }

  private:
    void *m_read = nullptr, *m_write = nullptr;
    std::vector<char> m_pending;
    // Absolute byte position modulo one F32 stereo frame. A flush must not
    // reinterpret the middle of a float/channel pair as a new frame.
    size_t m_streamOffset = 0, m_discardBytes = 0;
    bool m_boundary = false, m_markersSeen = false;
    uint32_t m_boundaryPositionMs = 0;
};
} // namespace tonal::audio
