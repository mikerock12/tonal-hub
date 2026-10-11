#ifdef _WIN32
// As interfaces do Windows entram antes do Qt, para as macros do Qt (signals, slots, emit) não chegarem
// aos cabeçalhos delas. São as interfaces COM do Windows SDK (WRL), não o C++/WinRT: o C++/WinRT exige
// corrotinas, que no MSVC atual só existem em C++20.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <SystemMediaTransportControlsInterop.h>
#include <roapi.h>
#include <shobjidl_core.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>
#endif

#include "MediaControls.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QWindow>

#include "Player.h"

Q_LOGGING_CATEGORY(lcMedia, "tonal.media", QtInfoMsg)

#ifdef _WIN32

namespace wrl = Microsoft::WRL;
namespace wf = ABI::Windows::Foundation;
namespace media = ABI::Windows::Media;
namespace streams = ABI::Windows::Storage::Streams;

namespace {

// Os eventos do SMTC chegam numa thread do sistema. Eles só postam o pedido para a thread principal, que
// confere se a instância ainda existe: a thread do sistema nunca toca no objeto.
MediaControls *s_instance = nullptr;

using ButtonHandler = wf::ITypedEventHandler<media::SystemMediaTransportControls *,
                                             media::SystemMediaTransportControlsButtonPressedEventArgs *>;
using SeekHandler = wf::ITypedEventHandler<media::SystemMediaTransportControls *,
                                           media::PlaybackPositionChangeRequestedEventArgs *>;

bool ok(HRESULT hr, const char *what)
{
    if (SUCCEEDED(hr))
        return true;
    qCWarning(lcMedia).noquote() << what << QString::number(ulong(hr), 16);
    return false;
}

// O QString termina em zero, como o Windows exige de uma referência de HSTRING. A referência vale até o fim
// da expressão em que é criada, e o Windows copia o texto se precisar guardá-lo.
wrl::Wrappers::HStringReference hstring(const QString &text)
{
    return wrl::Wrappers::HStringReference(reinterpret_cast<const wchar_t *>(text.utf16()), UINT32(text.size()));
}

wf::TimeSpan timeSpan(qint64 ms)
{
    return wf::TimeSpan { ms * 10000 }; // unidades de 100 ns
}

wrl::ComPtr<streams::IRandomAccessStreamReference> thumbnailFromUrl(const QString &url)
{
    wrl::ComPtr<wf::IUriRuntimeClassFactory> uriFactory;
    wrl::ComPtr<wf::IUriRuntimeClass> uri;
    wrl::ComPtr<streams::IRandomAccessStreamReferenceStatics> references;
    wrl::ComPtr<streams::IRandomAccessStreamReference> thumbnail;
    if (ok(RoGetActivationFactory(wrl::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                                  IID_PPV_ARGS(&uriFactory)), "Uri:")
        && ok(uriFactory->CreateUri(hstring(url).Get(), &uri), "CreateUri:")
        && ok(RoGetActivationFactory(
                  wrl::Wrappers::HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                  IID_PPV_ARGS(&references)), "RandomAccessStreamReference:")
        && ok(references->CreateFromUri(uri.Get(), &thumbnail), "CreateFromUri:"))
        return thumbnail;
    return {};
}

} // namespace

struct MediaControls::Native
{
    wrl::ComPtr<media::ISystemMediaTransportControls> smtc;
    wrl::ComPtr<media::ISystemMediaTransportControls2> smtc2;
    EventRegistrationToken buttonToken {};
    EventRegistrationToken seekToken {};
};

MediaControls::MediaControls(Player *player, QObject *parent)
    : QObject(parent), m_player(player)
{
    // O Player emite trackChanged, stateChanged e positionChanged em sequência a cada consulta: junta tudo
    // numa atualização só.
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(0);
    connect(&m_syncTimer, &QTimer::timeout, this, &MediaControls::sync);
}

MediaControls::~MediaControls()
{
    if (!d)
        return;
    s_instance = nullptr;
    d->smtc->remove_ButtonPressed(d->buttonToken);
    d->smtc2->remove_PlaybackPositionChangeRequested(d->seekToken);
    d->smtc->put_IsEnabled(false);
}

void MediaControls::setAppUserModelId()
{
    // O mesmo id vai no AppUserModelID dos atalhos em packaging/tonal.iss. Não mude: o painel de mídia e a
    // barra de tarefas deixam de achar o atalho.
    ok(SetCurrentProcessExplicitAppUserModelID(L"Tonal.Player"), "AppUserModelID:");
}

void MediaControls::attach(QWindow *window)
{
    if (!window || d)
        return;
    auto native = std::make_unique<Native>();
    wrl::ComPtr<ISystemMediaTransportControlsInterop> interop;
    if (!ok(RoGetActivationFactory(
                wrl::Wrappers::HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
                IID_PPV_ARGS(&interop)), "Controle de mídia do Windows indisponível:")
        || !ok(interop->GetForWindow(reinterpret_cast<HWND>(window->winId()), IID_PPV_ARGS(&native->smtc)),
               "Controle de mídia do Windows indisponível (GetForWindow):")
        || !ok(native->smtc.As(&native->smtc2), "Controle de mídia do Windows sem a linha do tempo:"))
        return;

    auto *smtc = native->smtc.Get();
    smtc->put_IsPlayEnabled(true);
    smtc->put_IsPauseEnabled(true);
    smtc->put_IsStopEnabled(true);
    smtc->put_IsNextEnabled(true);
    smtc->put_IsPreviousEnabled(true);
    native->smtc2->put_PlaybackRate(1.0);

    auto onButton = wrl::Callback<ButtonHandler>(
        [](media::ISystemMediaTransportControls *, media::ISystemMediaTransportControlsButtonPressedEventArgs *args) {
            media::SystemMediaTransportControlsButton button {};
            if (SUCCEEDED(args->get_Button(&button))) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), [button] {
                    if (s_instance)
                        s_instance->handleButton(int(button));
                }, Qt::QueuedConnection);
            }
            return S_OK;
        });
    auto onSeek = wrl::Callback<SeekHandler>(
        [](media::ISystemMediaTransportControls *, media::IPlaybackPositionChangeRequestedEventArgs *args) {
            wf::TimeSpan position {};
            if (SUCCEEDED(args->get_RequestedPlaybackPosition(&position))) {
                const qint64 ms = position.Duration / 10000;
                QMetaObject::invokeMethod(QCoreApplication::instance(), [ms] {
                    if (s_instance)
                        s_instance->handleSeek(ms);
                }, Qt::QueuedConnection);
            }
            return S_OK;
        });
    if (!ok(smtc->add_ButtonPressed(onButton.Get(), &native->buttonToken), "add_ButtonPressed:"))
        return;
    if (!ok(native->smtc2->add_PlaybackPositionChangeRequested(onSeek.Get(), &native->seekToken),
            "add_PlaybackPositionChangeRequested:")) {
        smtc->remove_ButtonPressed(native->buttonToken);
        return;
    }
    d = std::move(native);
    s_instance = this;

    for (auto signal : { &Player::trackChanged, &Player::stateChanged, &Player::positionChanged })
        connect(m_player, signal, &m_syncTimer, qOverload<>(&QTimer::start));
    sync();
}

void MediaControls::sync()
{
    const Player *p = m_player;
    if (p->trackUri() != m_shownTrack) {
        m_shownTrack = p->trackUri();
        m_shownStatus = -1; // faixa nova: manda o estado e a linha do tempo de novo
        wrl::ComPtr<media::ISystemMediaTransportControlsDisplayUpdater> updater;
        wrl::ComPtr<media::IMusicDisplayProperties> music;
        wrl::ComPtr<media::IMusicDisplayProperties2> music2;
        if (ok(d->smtc->get_DisplayUpdater(&updater), "DisplayUpdater:")) {
            updater->ClearAll();
            if (!m_shownTrack.isEmpty() && ok(updater->put_Type(media::MediaPlaybackType_Music), "put_Type:")
                && ok(updater->get_MusicProperties(&music), "MusicProperties:")) {
                music->put_Title(hstring(p->trackName()).Get());
                music->put_Artist(hstring(p->artistsText()).Get());
                if (SUCCEEDED(music.As(&music2)))
                    music2->put_AlbumTitle(hstring(p->albumName()).Get());
                // O Windows baixa a capa sozinho a partir da URL.
                if (!p->coverUrlLarge().isEmpty()) {
                    if (auto thumbnail = thumbnailFromUrl(p->coverUrlLarge()))
                        updater->put_Thumbnail(thumbnail.Get());
                }
            }
            ok(updater->Update(), "Update:");
        }
    }

    const auto status = !p->hasTrack() ? media::MediaPlaybackStatus_Closed
                        : p->isPlaying() ? media::MediaPlaybackStatus_Playing
                                         : media::MediaPlaybackStatus_Paused;
    const bool statusChanged = int(status) != m_shownStatus;
    if (statusChanged) {
        m_shownStatus = int(status);
        d->smtc->put_IsEnabled(p->hasTrack());
        ok(d->smtc->put_PlaybackStatus(status), "put_PlaybackStatus:");
    }
    if (!p->hasTrack())
        return;

    // Enquanto toca, o painel do Windows anda com a posição sozinho. Só manda de novo quando o estado muda
    // ou a posição pula (seek, ou a consulta ao Spotify corrigiu a interpolação).
    const int position = p->positionMs();
    if (!statusChanged) {
        const qint64 expected = m_shownPosition + (p->isPlaying() ? m_shownClock.elapsed() : 0);
        if (qAbs(position - expected) <= 1500)
            return;
    }
    wrl::ComPtr<IInspectable> instance;
    wrl::ComPtr<media::ISystemMediaTransportControlsTimelineProperties> timeline;
    if (ok(RoActivateInstance(wrl::Wrappers::HStringReference(
                                  RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
                              &instance), "TimelineProperties:")
        && ok(instance.As(&timeline), "TimelineProperties:")) {
        timeline->put_StartTime(timeSpan(0));
        timeline->put_EndTime(timeSpan(p->durationMs()));
        timeline->put_MinSeekTime(timeSpan(0));
        timeline->put_MaxSeekTime(timeSpan(p->durationMs()));
        timeline->put_Position(timeSpan(position));
        ok(d->smtc2->UpdateTimelineProperties(timeline.Get()), "UpdateTimelineProperties:");
    }
    m_shownPosition = position;
    m_shownClock.start();
}

void MediaControls::handleButton(int button)
{
    switch (media::SystemMediaTransportControlsButton(button)) {
    case media::SystemMediaTransportControlsButton_Play:
        if (!m_player->isPlaying())
            m_player->togglePlay();
        break;
    case media::SystemMediaTransportControlsButton_Pause:
    case media::SystemMediaTransportControlsButton_Stop:
        if (m_player->isPlaying())
            m_player->togglePlay();
        break;
    case media::SystemMediaTransportControlsButton_Next:
        m_player->next();
        break;
    case media::SystemMediaTransportControlsButton_Previous:
        m_player->previous();
        break;
    default:
        break;
    }
}

void MediaControls::handleSeek(qint64 ms)
{
    m_player->seek(int(qBound(qint64(0), ms, qint64(m_player->durationMs()))));
}

#else // Ainda sem equivalente nos outros sistemas (no Linux seria o MPRIS).

struct MediaControls::Native
{
};

MediaControls::MediaControls(Player *player, QObject *parent)
    : QObject(parent), m_player(player)
{
}

MediaControls::~MediaControls() = default;

void MediaControls::attach(QWindow *window)
{
    Q_UNUSED(window)
}

void MediaControls::setAppUserModelId()
{
}

#endif
