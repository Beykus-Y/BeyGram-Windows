#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing list of the composer's attachment options.
///
/// The facade exposes the attachment sheet as two parallel, read-only lists:
/// kinds holds the stable names ("photo", "video", ...) in display order and
/// titles holds the matching labels at the same index. Both are built from
/// defaultAttachmentOptions(), so the sheet and the C++ helpers cannot disagree
/// about order, names or labels. The lists are fixed for the lifetime of the
/// facade; changed() exists as their notify signal but is not emitted after
/// construction.
///
/// Nothing here opens a file dialog, sends a message or talks to TDLib. A
/// tapped tile calls choose(), which turns the kind into exactly one request
/// signal; the owner decides what that means (open a picker, ask for a
/// location, ...). acceptFilter() supplies the QFileDialog filter for the
/// file-based kinds.
class AttachmentFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY changed)

public:
    explicit AttachmentFacade(QObject* parent = nullptr);

    /// User-facing labels in display order. These are the English defaults
    /// from AttachmentOptions; the view may translate them with qsTr().
    [[nodiscard]] QStringList titles() const;

    /// Stable kind names in the same order as titles(). Every entry is accepted
    /// by acceptFilter() and choose().
    [[nodiscard]] QStringList kinds() const;

    /// The file-dialog filter for a kind name, in the space-separated
    /// "*.jpg *.png" form QFileDialog expects.
    ///
    /// Matching ignores case and surrounding whitespace. The result is empty
    /// when the kind is unknown or is not chosen from a file (location,
    /// contact, poll, sticker); a caller must not pass an empty filter to a
    /// file dialog.
    Q_INVOKABLE QString acceptFilter(const QString& kind) const;

    /// Asks the owner to start the flow for a kind name by emitting exactly one
    /// of the *Requested() signals. Matching ignores case and surrounding
    /// whitespace. An unknown name is ignored: in particular it does not fall
    /// back to photoRequested(), so a typo cannot open the wrong picker.
    Q_INVOKABLE void choose(const QString& kind);

signals:
    /// The notify signal for titles() and kinds(). The lists never change after
    /// construction, so this is not emitted at runtime.
    void changed();

    void photoRequested();
    void videoRequested();
    void documentRequested();
    void audioRequested();
    void locationRequested();
    void contactRequested();
    void pollRequested();
    void stickerRequested();

private:
    QStringList m_titles;
    QStringList m_kinds;
};

} // namespace beygram::ui
