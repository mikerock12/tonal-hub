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

  private:
    void *m_read = nullptr, *m_write = nullptr;
    std::vector<char> m_pending;
    // Absolute byte position modulo one F32 stereo frame. A flush must not
    // reinterpret the middle of a float/channel pair as a new frame.
    size_t m_streamOffset = 0, m_discardBytes = 0;
};
} // namespace tonal::audio
