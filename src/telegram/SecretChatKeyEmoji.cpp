#include "SecretChatKeyEmoji.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonValue>

#include <iterator>

namespace beygram::telegram {
namespace {

constexpr int kEmojiCount = 4;
constexpr int kFingerprintBytes = 4;

struct EmojiChoice {
    const char16_t* emoji;
    const char* label;
};

// The first 64 entries of Telegram for Android's EmojiData.emojiSecret, in
// their original order (SecretChatKeyEmoji.kt). The index is part of the
// comparison, so the list is never sorted, deduplicated or reordered. This
// module indexes the table with SHA-256 digest bytes (see keyEmojiFromHash),
// not with TDLib's key-hash bytes as the Android client does, so only the
// table itself is shared.
constexpr EmojiChoice kEmojiTable[] = {
    {u"\U0001F609", "Winking face"},
    {u"\U0001F60D", "Smiling face with heart-eyes"},
    {u"\U0001F61B", "Face with tongue"},
    {u"\U0001F62D", "Loudly crying face"},
    {u"\U0001F631", "Face screaming in fear"},
    {u"\U0001F621", "Enraged face"},
    {u"\U0001F60E", "Smiling face with sunglasses"},
    {u"\U0001F634", "Sleeping face"},
    {u"\U0001F635", "Dizzy face"},
    {u"\U0001F608", "Smiling face with horns"},
    {u"\U0001F62C", "Grimacing face"},
    {u"\U0001F607", "Smiling face with halo"},
    {u"\U0001F60F", "Smirking face"},
    {u"\U0001F46E", "Police officer"},
    {u"\U0001F477", "Construction worker"},
    {u"\U0001F482", "Guard"},
    {u"\U0001F476", "Baby"},
    {u"\U0001F468", "Man"},
    {u"\U0001F469", "Woman"},
    {u"\U0001F474", "Old man"},
    {u"\U0001F475", "Old woman"},
    {u"\U0001F63B", "Smiling cat with heart-eyes"},
    {u"\U0001F63D", "Kissing cat"},
    {u"\U0001F640", "Weary cat"},
    {u"\U0001F47A", "Goblin"},
    {u"\U0001F648", "See-no-evil monkey"},
    {u"\U0001F649", "Hear-no-evil monkey"},
    {u"\U0001F64A", "Speak-no-evil monkey"},
    {u"\U0001F480", "Skull"},
    {u"\U0001F47D", "Alien"},
    {u"\U0001F4A9", "Pile of poo"},
    {u"\U0001F525", "Fire"},
    {u"\U0001F4A5", "Collision"},
    {u"\U0001F4A4", "Zzz"},
    {u"\U0001F442", "Ear"},
    {u"\U0001F440", "Eyes"},
    {u"\U0001F443", "Nose"},
    {u"\U0001F445", "Tongue"},
    {u"\U0001F444", "Mouth"},
    {u"\U0001F44D", "Thumbs up"},
    {u"\U0001F44E", "Thumbs down"},
    {u"\U0001F44C", "OK hand"},
    {u"\U0001F44A", "Oncoming fist"},
    {u"\u270C", "Victory hand"},
    {u"\u270B", "Raised hand"},
    {u"\U0001F450", "Open hands"},
    {u"\U0001F446", "Backhand index pointing up"},
    {u"\U0001F447", "Backhand index pointing down"},
    {u"\U0001F449", "Backhand index pointing right"},
    {u"\U0001F448", "Backhand index pointing left"},
    {u"\U0001F64F", "Folded hands"},
    {u"\U0001F44F", "Clapping hands"},
    {u"\U0001F4AA", "Flexed biceps"},
    {u"\U0001F6B6", "Person walking"},
    {u"\U0001F3C3", "Person running"},
    {u"\U0001F483", "Woman dancing"},
    {u"\U0001F46B", "Woman and man holding hands"},
    {u"\U0001F46A", "Family"},
    {u"\U0001F46C", "Men holding hands"},
    {u"\U0001F46D", "Women holding hands"},
    {u"\U0001F485", "Nail polish"},
    {u"\U0001F3A9", "Top hat"},
    {u"\U0001F451", "Crown"},
    {u"\U0001F452", "Woman's hat"},
};

constexpr int kEmojiTableSize = static_cast<int>(std::size(kEmojiTable));
static_assert(kEmojiTableSize >= 64,
              "The secret-chat emoji table must keep at least 64 entries.");

// Returns the trimmed key hash when it is well-formed base64, or an empty
// string otherwise. The decoded bytes are not needed: the mapping hashes the
// base64 text, so decoding only validates the value.
QString validatedKeyHash(const QString& keyHashBase64)
{
    const QString keyHash = keyHashBase64.trimmed();
    if (keyHash.isEmpty())
        return QString();

    const auto decoded = QByteArray::fromBase64Encoding(
        keyHash.toLatin1(),
        QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.isEmpty())
        return QString();

    return keyHash;
}

QByteArray keyHashDigest(const QString& keyHash)
{
    return QCryptographicHash::hash(keyHash.toUtf8(), QCryptographicHash::Sha256);
}

} // namespace

QVector<KeyEmoji> keyEmojiFromState(const QJsonObject& secretChat)
{
    const QJsonValue keyHash = secretChat.value(QStringLiteral("key_hash"));
    if (!keyHash.isString())
        return QVector<KeyEmoji>();

    return keyEmojiFromHash(keyHash.toString());
}

QVector<KeyEmoji> keyEmojiFromHash(QString keyHashBase64)
{
    const QString keyHash = validatedKeyHash(keyHashBase64);
    if (keyHash.isEmpty())
        return QVector<KeyEmoji>();

    const QByteArray digest = keyHashDigest(keyHash);

    QVector<KeyEmoji> emoji;
    emoji.reserve(kEmojiCount);
    for (int i = 0; i < kEmojiCount; ++i) {
        const int byte = static_cast<unsigned char>(digest.at(i));
        const EmojiChoice& choice = kEmojiTable[byte % kEmojiTableSize];
        emoji.append(KeyEmoji{QString::fromUtf16(choice.emoji),
                              QString::fromLatin1(choice.label)});
    }

    return emoji;
}

QString keyHashFingerprint(QString keyHashBase64)
{
    const QString keyHash = validatedKeyHash(keyHashBase64);
    if (keyHash.isEmpty())
        return QString();

    const QByteArray digest = keyHashDigest(keyHash);
    return QString::fromLatin1(digest.left(kFingerprintBytes).toHex());
}

bool keyEmojiMatches(const QVector<KeyEmoji>& left,
                     const QVector<KeyEmoji>& right)
{
    // An empty sequence means "no key", so two of them are not a match: a
    // missing key must never look verified.
    if (left.isEmpty() || left.size() != right.size())
        return false;

    for (qsizetype i = 0; i < left.size(); ++i) {
        const QString& emoji = left.at(i).emoji;
        if (emoji.isEmpty() || emoji != right.at(i).emoji)
            return false;
    }

    return true;
}

} // namespace beygram::telegram
