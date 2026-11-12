#pragma once

#include <QHash>
#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// The network the client is currently using, one bucket per row of the
/// auto-download settings screen.
enum class NetworkKind { WiFi, Mobile, Roaming };

/// Per-network size ceilings for automatic downloads, one per media kind.
/// A ceiling of zero means "never download this kind", which is also what the
/// settings screen shows when the user turns a kind off. Sizes are bytes.
struct DownloadLimits {
    int photoBytes = 0;
    int videoBytes = 0;
    int fileBytes = 0;

    bool operator==(const DownloadLimits& other) const = default;
};

/// Auto-download settings for the three networks.
///
/// The policy is a plain value holder: it never talks to TDLib, never reads the
/// clock and performs no I/O, so the tests stay deterministic. The caller reads
/// the settings from storage, installs them with setLimits() and forwards the
/// resulting values to the downloader; every decision is made here, without
/// logging and without side effects beyond changed().
///
/// Invariants:
///
///  - Every network always has a limit set. The constructor installs the
///    defaults and limits() falls back to them for a network that was never
///    written, so a network can never be reported as unlimited by accident.
///  - Negative ceilings are clamped to zero, which means "never". A negative
///    value can therefore not turn into a budget that lets a file through.
///  - allows() is the kind-less gate: it takes only a size, so it compares
///    against the widest ceiling configured for that network. A size that a
///    per-kind rule would accept is never refused here; deciding between
///    photo, video and file ceilings is MediaCachePolicy's job, because that
///    decision needs the media kind.
///  - changed() is emitted only when a value really differs. Writing the value
///    already stored, or resetting an untouched policy, is silent, so a refresh
///    that brings the same settings does not repaint the screen or re-trigger a
///    binding loop.
class AutoDownloadPolicy final : public QObject {
    Q_OBJECT

public:
    explicit AutoDownloadPolicy(QObject* parent = nullptr);

    /// Stores the ceilings for `network`. Negative ceilings are clamped to zero
    /// ("never") before they are stored. changed() is emitted only when the
    /// stored ceilings differ from `limits`.
    void setLimits(NetworkKind network, DownloadLimits limits);

    /// The ceilings configured for `network`, or the defaults when the network
    /// has never been written. A copy is returned, so the caller cannot change
    /// the policy behind changed()'s back.
    [[nodiscard]] DownloadLimits limits(NetworkKind network) const;

    /// Turns automatic downloading on or off for every network. While it is
    /// off, allows() refuses everything regardless of the ceilings.
    void setAutoDownload(bool enabled);

    /// Whether automatic downloading is enabled. It is on by default.
    [[nodiscard]] bool autoDownload() const;

    /// Whether a transfer of `sizeBytes` may start without the user asking for
    /// it on `network`. All of the following must hold: auto-download is
    /// enabled, the size is known (not negative) and it fits the widest ceiling
    /// of that network, which must itself be positive.
    [[nodiscard]] bool allows(NetworkKind network, qint64 sizeBytes) const;

    /// Whether stories are preloaded automatically. Off by default, because a
    /// story is downloaded before the user has shown any interest in it.
    void setPreloadStories(bool enabled);

    /// Whether stories are preloaded automatically.
    [[nodiscard]] bool preloadStories() const;

    /// Restores the defaults: WiFi 10/50/20 MB, Mobile 1/5/2 MB, Roaming off,
    /// with auto-download on and story preloading off. changed() is emitted
    /// only when something actually differed.
    void resetToDefaults();

signals:
    void changed();

private:
    QHash<int, DownloadLimits> m_limits;
    bool m_enabled = true;
    bool m_stories = false;
};

} // namespace beygram::telegram
