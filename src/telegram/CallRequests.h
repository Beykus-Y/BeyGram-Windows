#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the TDLib call-control methods. They perform no logging,
/// no file access and add no @extra; the transport adds the correlation id.
///
/// A call id is a positive TDLib int32. Every builder that takes one returns an
/// empty object when the id is not positive, so a malformed id never reaches
/// the wire. acceptCallRequest copies protocol into the request unchanged: the
/// caller supplies the callProtocol negotiated with the peer.

/// Builds getCall for one call id.
[[nodiscard]] QJsonObject getCallRequest(qint64 callId);

/// Builds acceptCall with the caller's callProtocol object. The protocol is
/// passed through as-is; an empty object is still forwarded, so a caller that
/// has no negotiated protocol must resolve it before accepting.
[[nodiscard]] QJsonObject acceptCallRequest(qint64 callId, const QJsonObject& protocol);

/// Builds discardCall. duration is the call length in seconds and is clamped
/// to 0..86400, the range the schema accepts. isDisconnected marks a call that
/// dropped because the connection was lost rather than one the user ended.
[[nodiscard]] QJsonObject discardCallRequest(qint64 callId, bool isDisconnected,
                                             int duration, bool isVideo);

/// Builds sendCallRating. rating is clamped to 1..5. comment is trimmed and cut
/// to 4096 Unicode code points without splitting a surrogate pair; an empty
/// comment is still sent, because the schema allows it.
[[nodiscard]] QJsonObject sendCallRatingRequest(qint64 callId, int rating,
                                                const QString& comment);

/// Builds sendCallDebugInformation. debugInformation is sent verbatim, with no
/// trimming; empty text is valid.
[[nodiscard]] QJsonObject sendCallDebugInformationRequest(qint64 callId,
                                                          const QString& debugInformation);

/// Builds sendCallLog for the given log file. logFile is trimmed and carried in
/// log_file; an empty or whitespace-only value returns an empty object. The
/// builder never reads the file.
[[nodiscard]] QJsonObject sendCallLogRequest(const QString& logFile);

} // namespace beygram::telegram
