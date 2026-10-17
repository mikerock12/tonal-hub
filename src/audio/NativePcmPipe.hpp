#pragma once
#include <cstdint>
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
};
} // namespace tonal::audio
