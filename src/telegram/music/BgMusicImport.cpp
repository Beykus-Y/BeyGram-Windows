#include "BgMusicImport.h"

#include <QFileInfo>

namespace beygram::telegram {
namespace {

constexpr qint64 kMaxImportBytes = 2LL * 1024 * 1024;
constexpr int kMaxExportBaseLength = 80;

ImportDecision rejected(const QString& reason)
{
    ImportDecision decision;
    decision.reason = reason;
    return decision;
}

bool isSafeExportCharacter(QChar character)
{
    const ushort code = character.unicode();
    return (code >= 'A' && code <= 'Z')
        || (code >= 'a' && code <= 'z')
        || (code >= '0' && code <= '9')
        || code == '.'
        || code == '_'
        || code == '-';
}

} // namespace

ImportDecision planBgMusicImport(const QString& fileName,
                                 qint64 sizeBytes,
                                 const QString& content)
{
    if (sizeBytes >= kMaxImportBytes)
        return rejected(QStringLiteral("too-large"));
    if (sizeBytes <= 0 || content.trimmed().isEmpty())
        return rejected(QStringLiteral("empty"));

    const QString suffix = QFileInfo(fileName.trimmed()).suffix().toLower();
    BgMusicPlaylist playlist;
    if (suffix == QStringLiteral("json")) {
        playlist = bgMusicFromJson(content);
    } else if (suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8")) {
        playlist = bgMusicFromM3u(content);
    } else {
        return rejected(QStringLiteral("unsupported-extension"));
    }

    if (playlist.entries.isEmpty())
        return rejected(QStringLiteral("no-entries"));

    ImportDecision decision;
    decision.accepted = true;
    decision.playlistId = playlist.id;
    decision.entryCount = static_cast<int>(playlist.entries.size());
    return decision;
}

QString exportBgMusicFileName(const BgMusicPlaylist& playlist)
{
    const QString title = playlist.title.trimmed();

    QString base;
    base.reserve(title.size());
    for (const QChar character : title)
        base.append(isSafeExportCharacter(character) ? character : QChar(QLatin1Char('_')));

    if (base.size() > kMaxExportBaseLength)
        base.truncate(kMaxExportBaseLength);

    while (base.startsWith(QLatin1Char('.')))
        base.remove(0, 1);
    while (base.endsWith(QLatin1Char('.')))
        base.chop(1);

    if (base.isEmpty())
        base = QStringLiteral("playlist");

    return base + QStringLiteral(".json");
}

} // namespace beygram::telegram
