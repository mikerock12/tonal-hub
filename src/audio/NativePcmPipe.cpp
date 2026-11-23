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
    if (frames <= 0)
        return 0;
#ifdef _WIN32
    DWORD available = 0;
    if (!PeekNamedPipe(m_read, nullptr, 0, nullptr, &available, nullptr))
        return -1;
    if (m_discardBytes && available)
    {
        char tail[8];
        DWORD got = 0;
        const auto take = DWORD(std::min<size_t>(available, m_discardBytes));
        if (!ReadFile(m_read, tail, take, &got, nullptr))
            return -1;
        m_streamOffset = (m_streamOffset + got) % 8;
        m_discardBytes -= got;
        available -= got;
    }
    if (m_discardBytes)
        return 0; // finish dropping the split old frame on the next read
    const auto take = std::min<size_t>(available, size_t(frames) * 8 - m_pending.size());
    if (take)
    {
        const auto old = m_pending.size();
        m_pending.resize(old + take);
        DWORD got = 0;
        if (!ReadFile(m_read, m_pending.data() + old, DWORD(take), &got, nullptr))
            return -1;
        m_pending.resize(old + got);
        m_streamOffset = (m_streamOffset + got) % 8;
    }
    auto bytes = m_pending.size() / 8 * 8;
    size_t skip = 0;
    if (m_event == EventKind::None)
    {
        // Sinalizadores NaN nunca produzidos pela conversão f64->f32. O
        // marcador 1B começa uma faixa; 1C é transporte aplicado pelo player.
        for (size_t at = 0; at < bytes; at += 8)
        {
            uint32_t left, right;
            std::memcpy(&left, m_pending.data() + at, 4);
            std::memcpy(&right, m_pending.data() + at + 4, 4);
            if ((left == 0x7FA07A1Bu || left == 0x7FA07A1Cu) &&
                (right & 0xFFC00000u) == 0x7FC00000u)
            {
                const uint32_t payload = right & 0x3FFFFFu;
                if (left == 0x7FA07A1Bu)
                {
                    m_event = EventKind::TrackBoundary;
                    m_markersSeen = true;
                    m_boundaryPositionMs = payload;
                }
                else if (payload >= 1 && payload <= 3)
                {
                    m_event = payload == 1 ? EventKind::UserLoad
                              : payload == 2 ? EventKind::UserSeek : EventKind::UserStop;
                    m_boundaryPositionMs = 0;
                }
                else
                    continue;
                bytes = at;
                skip = 8;
                break;
            }
        }
    }
    samples.resize(bytes / 4);
    if (bytes)
        std::memcpy(samples.data(), m_pending.data(), bytes);
    if (bytes + skip)
        m_pending.erase(m_pending.begin(), m_pending.begin() + ptrdiff_t(bytes + skip));
#endif
    return int(samples.size() / 2);
}
bool NativePcmPipe::takeEvent(Event &event)
{
    if (m_event == EventKind::None)
        return false;
    event.kind = m_event;
    event.positionMs = m_boundaryPositionMs;
    m_event = EventKind::None;
    m_boundaryPositionMs = 0;
    return true;
}
bool NativePcmPipe::takeBoundary(uint32_t &positionMs)
{
    if (m_event != EventKind::TrackBoundary)
        return false;
    Event event;
    takeEvent(event);
    positionMs = event.positionMs;
    return true;
}
void NativePcmPipe::discard()
{
    m_pending.clear();
    m_event = EventKind::None;
#ifdef _WIN32
    DWORD n = 0;
    char data[8192];
    // Bound draining so a producer cannot monopolize this thread.
    for (int i = 0; i < 16 && PeekNamedPipe(m_read, nullptr, 0, nullptr, &n, nullptr) && n; ++i)
    {
        DWORD got = 0;
        if (!ReadFile(m_read, data, std::min<DWORD>(n, sizeof(data)), &got, nullptr))
            break;
        m_streamOffset = (m_streamOffset + got) % 8;
    }
    // A partial frame may still be inside the producer's stdout buffer. Drop
    // its remaining bytes when they arrive, also across repeated flushes.
    m_discardBytes = (8 - m_streamOffset) % 8;
#endif
}
} // namespace tonal::audio
