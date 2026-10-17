#include "NativePcmPipe.hpp"
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace tonal::audio
{
NativePcmPipe::NativePcmPipe()
{
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    if (CreatePipe(&m_read, &m_write, &sa, 32768))
        SetHandleInformation(m_read, HANDLE_FLAG_INHERIT, 0);
#endif
}
NativePcmPipe::~NativePcmPipe()
{
    closeWriter();
#ifdef _WIN32
    if (m_read)
        CloseHandle(m_read);
#endif
}
bool NativePcmPipe::valid() const
{
    return m_read && m_write;
}
uintptr_t NativePcmPipe::writer() const
{
    return reinterpret_cast<uintptr_t>(m_write);
}
void NativePcmPipe::closeWriter()
{
#ifdef _WIN32
    if (m_write)
        CloseHandle(m_write);
#endif
    m_write = nullptr;
}
int NativePcmPipe::read(std::vector<float> &samples, int frames)
{
    samples.clear();
#ifdef _WIN32
    DWORD available = 0;
    if (!PeekNamedPipe(m_read, nullptr, 0, nullptr, &available, nullptr))
        return -1;
    const auto take = std::min<size_t>(available, size_t(frames) * 8 - m_pending.size());
    if (take)
    {
        const auto old = m_pending.size();
        m_pending.resize(old + take);
        DWORD got = 0;
        if (!ReadFile(m_read, m_pending.data() + old, DWORD(take), &got, nullptr))
            return -1;
        m_pending.resize(old + got);
    }
    const auto bytes = m_pending.size() / 8 * 8;
    samples.resize(bytes / 4);
    if (bytes)
    {
        std::memcpy(samples.data(), m_pending.data(), bytes);
        m_pending.erase(m_pending.begin(), m_pending.begin() + bytes);
    }
#endif
    return int(samples.size() / 2);
}
void NativePcmPipe::discard()
{
    m_pending.clear();
#ifdef _WIN32
    DWORD n = 0;
    char data[8192];
    // Bound draining so a producer cannot monopolize this thread.
    for (int i = 0; i < 16 && PeekNamedPipe(m_read, nullptr, 0, nullptr, &n, nullptr) && n; ++i)
    {
        DWORD got;
        ReadFile(m_read, data, std::min<DWORD>(n, sizeof(data)), &got, nullptr);
    }
#endif
}
} // namespace tonal::audio
