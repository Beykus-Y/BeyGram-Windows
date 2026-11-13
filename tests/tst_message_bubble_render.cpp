// Renders qml/components/MessageBubble.qml through QQmlEngine and inspects the
// live item tree (layout sizes, visibility, alignment, background colour).
// Nothing here reads or pattern-matches the QML source.
//
// Build note: like tst_qml_ui, this target must link the QML module and Quick:
//     beygram_qml beygram_qmlplugin Qt6::Quick
// so that qrc:/qt/qml/BeyGram/... and the BeyGram module types exist. The
// loader below also imports the BeyGram module and, as a last resort, tries a
// QML file relative to the test binary.

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>
#include <QtTest>

#include <functional>
#include <memory>

// Named (not anonymous) namespace: moc generates the meta-object definitions
// for these Q_OBJECT classes, and a named namespace keeps that unambiguous.
namespace test_stubs {

// Minimal stand-ins for the two singleton modules MessageBubble imports. They
// keep the test self-contained: no beygram_ui headers, no AppController, no
// TDLib, and no appearance settings file. The values are deliberately fixed so
// colours and geometry are deterministic.
class StubTheme final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString modeName READ modeName CONSTANT)
    Q_PROPERTY(QString presetName READ presetName CONSTANT)
    Q_PROPERTY(bool darkMode READ darkMode CONSTANT)
    Q_PROPERTY(QColor accent READ accent CONSTANT)
    Q_PROPERTY(QColor accentInk READ accentInk CONSTANT)
    Q_PROPERTY(QColor accentSoft READ accentSoft CONSTANT)
    Q_PROPERTY(QColor bg READ bg CONSTANT)
    Q_PROPERTY(QColor bubbleOut READ bubbleOut CONSTANT)
    Q_PROPERTY(QColor danger READ danger CONSTANT)
    Q_PROPERTY(QColor line READ line CONSTANT)
    Q_PROPERTY(QColor muted READ muted CONSTANT)
    Q_PROPERTY(QColor ok READ ok CONSTANT)
    Q_PROPERTY(QColor okInk READ okInk CONSTANT)
    Q_PROPERTY(QColor onAccent READ onAccent CONSTANT)
    Q_PROPERTY(QColor surface READ surface CONSTANT)
    Q_PROPERTY(QColor surface2 READ surface2 CONSTANT)
    Q_PROPERTY(QColor text READ text CONSTANT)
    Q_PROPERTY(QColor warn READ warn CONSTANT)
    Q_PROPERTY(QColor warnInk READ warnInk CONSTANT)

public:
    explicit StubTheme(QObject *parent = nullptr) : QObject(parent) {}

    QString modeName() const { return QStringLiteral("Light"); }
    QString presetName() const { return QStringLiteral("Warm"); }
    bool darkMode() const { return false; }
    QColor accent() const { return QColor(0x2E, 0x7D, 0x6B); }
    QColor accentInk() const { return QColor(0x1F, 0x5C, 0x4F); }
    QColor accentSoft() const { return QColor(0xCF, 0xE8, 0xE1); }
    QColor bg() const { return QColor(0xF6, 0xF4, 0xEF); }
    QColor bubbleOut() const { return QColor(0xDD, 0xF3, 0xE9); }
    QColor danger() const { return QColor(0xB3, 0x26, 0x1E); }
    QColor line() const { return QColor(0xD8, 0xD2, 0xC7); }
    QColor muted() const { return QColor(0x6B, 0x6B, 0x6B); }
    QColor ok() const { return QColor(0x2E, 0x7D, 0x32); }
    QColor okInk() const { return QColor(0x1B, 0x5E, 0x20); }
    QColor onAccent() const { return QColor(Qt::white); }
    QColor surface() const { return QColor(Qt::white); }
    QColor surface2() const { return QColor(0xEF, 0xEC, 0xE5); }
    QColor text() const { return QColor(0x1B, 0x1B, 0x1B); }
    QColor warn() const { return QColor(0xB2, 0x6A, 0x00); }
    QColor warnInk() const { return QColor(0x7A, 0x4A, 0x00); }
};

class StubAppearance final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bubbleStyle READ bubbleStyle CONSTANT)
    Q_PROPERTY(QString designStyle READ designStyle CONSTANT)
    Q_PROPERTY(QString listDensity READ listDensity CONSTANT)
    Q_PROPERTY(qreal messageTextSize READ messageTextSize CONSTANT)
    Q_PROPERTY(bool reduceMotion READ reduceMotion CONSTANT)

public:
    explicit StubAppearance(QObject *parent = nullptr) : QObject(parent) {}

    // The application defaults keep the bubble colours opaque (no LiquidGlass
    // alpha); reduceMotion disables the hover animations.
    QString bubbleStyle() const { return QStringLiteral("Rounded"); }
    QString designStyle() const { return QStringLiteral("Default"); }
    QString listDensity() const { return QStringLiteral("Comfortable"); }
    qreal messageTextSize() const { return 14.0; }
    bool reduceMotion() const { return true; }
};

class StubApp final : public QObject
{
    Q_OBJECT

public:
    explicit StubApp(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void openLink(const QString &url) { Q_UNUSED(url) }
    Q_INVOKABLE void copyText(const QString &text) { Q_UNUSED(text) }
};

} // namespace test_stubs

namespace {

bool waitFor(const std::function<bool()> &predicate, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() >= timeoutMs)
            return false;
        QTest::qWait(10);
    }
    return true;
}

// QML Text objects are QQuickText instances; inherits() avoids pulling in the
// private qquicktext_p.h header just to identify them.
QQuickItem *findTextItem(QQuickItem *root, const QString &text)
{
    const auto items = root->findChildren<QQuickItem *>();
    QQuickItem *firstMatch = nullptr;
    for (QQuickItem *item : items) {
        if (!item->inherits("QQuickText") || item->property("text").toString() != text)
            continue;
        // Some messages render the same body twice (inline meta row vs the
        // multi-line body); the visible copy is the one the user sees.
        if (item->isVisible())
            return item;
        if (!firstMatch)
            firstMatch = item;
    }
    return firstMatch;
}

QQuickItem *findVisibleTextItem(QQuickItem *root, const QString &text)
{
    QQuickItem *item = findTextItem(root, text);
    return (item && item->isVisible()) ? item : nullptr;
}

// The bubble background is the direct child of the MessageBubble root that
// contains the given descendant (the body Text, sticker, or media placeholder).
QQuickItem *bubbleFrameFor(QQuickItem *root, QQuickItem *descendant)
{
    QQuickItem *item = descendant;
    while (item && item->parentItem() != root)
        item = item->parentItem();
    return item;
}

QString describeItem(const QQuickItem *item)
{
    if (!item)
        return QStringLiteral("null");
    const QQuickItem *parent = item->parentItem();
    return QStringLiteral("%1{x=%2,w=%3,parent=%4}")
        .arg(QString::fromLatin1(item->metaObject()->className()))
        .arg(item->x())
        .arg(item->width())
        .arg(parent ? QString::fromLatin1(parent->metaObject()->className())
                    : QStringLiteral("null"));
}

QString ancestorChain(const QQuickItem *item, const QQuickItem *root)
{
    QStringList parts;
    for (const QQuickItem *it = item; it && it != root; it = it->parentItem())
        parts << describeItem(it);
    return parts.join(QStringLiteral(" -> "));
}

} // namespace

class MessageBubbleRenderTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void incomingTextBubbleRendersBody();
    void outgoingTextBubbleAlignsRight();
    void stickerOnlyBubbleHasTransparentBackground();
    void emojiStickerRendersGlyph();
    void photoBubbleShowsPlaceholderLabel();

private:
    QQuickItem *newHost() const;
    QQuickItem *createBubble(QQuickItem *host, const QVariantMap &properties);
    QQmlComponent *loadComponent(QString *error);
    QStringList fileCandidates() const;

    // The stubs must outlive the engine (singleton instances are not owned by
    // it), hence they are declared before the engine/window members.
    test_stubs::StubTheme m_theme;
    test_stubs::StubAppearance m_appearance;
    test_stubs::StubApp m_app;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QQuickWindow> m_window;
};

void MessageBubbleRenderTest::initTestCase()
{
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &m_theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &m_appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &m_app);

    m_engine = std::make_unique<QQmlEngine>();

    // A test executable in build/Release finds the module qmldir in build/.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList importPaths = {
        appDir,
        QDir(appDir).absoluteFilePath(QStringLiteral("..")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../..")),
    };
    for (const QString &path : importPaths)
        m_engine->addImportPath(QDir::cleanPath(path));

    m_window = std::make_unique<QQuickWindow>();
    m_window->setColor(Qt::white);
    m_window->resize(480, 320);
    m_window->show();
    QTest::qWait(50);

    QString error;
    m_component = std::unique_ptr<QQmlComponent>(loadComponent(&error));
    QVERIFY2(m_component, qPrintable(error));
}

void MessageBubbleRenderTest::cleanupTestCase()
{
    m_window.reset();
    m_component.reset();
    m_engine.reset();
}

QQuickItem *MessageBubbleRenderTest::newHost() const
{
    auto *host = new QQuickItem(m_window->contentItem());
    host->setWidth(400);
    host->setHeight(240);
    return host;
}

QQuickItem *MessageBubbleRenderTest::createBubble(QQuickItem *host,
                                                  const QVariantMap &properties)
{
    QObject *object = m_component->create();
    if (!object)
        return nullptr;

    auto *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        delete object;
        return nullptr;
    }

    item->setParentItem(host);
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        item->setProperty(it.key().toUtf8().constData(), it.value());
    return item;
}

QStringList MessageBubbleRenderTest::fileCandidates() const
{
    QStringList candidates;

    // Source tree relative to this test's own file (works for in-tree runs).
    const QString sourceRelative = QFINDTESTDATA("../qml/components/MessageBubble.qml");
    if (!sourceRelative.isEmpty())
        candidates << QFileInfo(sourceRelative).absoluteFilePath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList relativePaths = {
        QStringLiteral("qml/components/MessageBubble.qml"),
        QStringLiteral("../qml/components/MessageBubble.qml"),
        QStringLiteral("../../qml/components/MessageBubble.qml"),
        QStringLiteral("BeyGram/qml/components/MessageBubble.qml"),
        QStringLiteral("../BeyGram/qml/components/MessageBubble.qml"),
        QStringLiteral("../../BeyGram/qml/components/MessageBubble.qml"),
    };
    for (const QString &relative : relativePaths) {
        const QString path = QDir::cleanPath(QDir(appDir).absoluteFilePath(relative));
        if (QFileInfo::exists(path) && !candidates.contains(path))
            candidates << path;
    }
    return candidates;
}

QQmlComponent *MessageBubbleRenderTest::loadComponent(QString *error)
{
    QStringList attempts;

    const auto tryUrl = [&](const QUrl &url) -> QQmlComponent * {
        auto component = std::make_unique<QQmlComponent>(m_engine.get(), url);
        if (component->isReady())
            return component.release();
        attempts << QStringLiteral("%1\n    %2").arg(url.toString(), component->errorString());
        return nullptr;
    };

    // 1. The canonical resource URL. This is what the application itself uses.
    const QUrl qrcUrl(QStringLiteral("qrc:/qt/qml/BeyGram/qml/components/MessageBubble.qml"));
    if (auto *component = tryUrl(qrcUrl))
        return component;

    // 2. Resolve the same file through the BeyGram module's qmldir/plugin.
    {
        auto component = std::make_unique<QQmlComponent>(m_engine.get(),
                                                         QStringLiteral("BeyGram"),
                                                         QStringLiteral("MessageBubble"));
        if (component->isReady())
            return component.release();
        attempts << QStringLiteral("module BeyGram/MessageBubble\n    %1")
                        .arg(component->errorString());
    }

    // 3. A QML tree deployed next to the test binary. The sibling ds/ directory
    //    is added to the import path so the Ds* components can be found.
    const QStringList candidates = fileCandidates();
    for (const QString &path : candidates) {
        const QDir dir = QFileInfo(path).absoluteDir();
        m_engine->addImportPath(dir.absolutePath());
        m_engine->addImportPath(QDir::cleanPath(dir.absoluteFilePath(QStringLiteral("../ds"))));
        m_engine->addImportPath(QDir::cleanPath(dir.absoluteFilePath(QStringLiteral(".."))));
    }
    for (const QString &path : candidates) {
        if (auto *component = tryUrl(QUrl::fromLocalFile(path)))
            return component;
    }

    if (error) {
        *error = QStringLiteral("MessageBubble.qml could not be loaded.\n%1")
                     .arg(attempts.join(QLatin1Char('\n')));
    }
    return nullptr;
}

void MessageBubbleRenderTest::incomingTextBubbleRendersBody()
{
    const std::unique_ptr<QQuickItem> host(newHost());
    QQuickItem *bubble = createBubble(host.get(), {
        {QStringLiteral("text"), QStringLiteral("hello")},
        {QStringLiteral("outgoing"), false},
        {QStringLiteral("date"),
         QVariant::fromValue<qint64>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("contentType"), QStringLiteral("text")},
    });
    QVERIFY2(bubble, qPrintable(m_component->errorString()));

    QVERIFY2(waitFor([&] { return bubble->implicitHeight() > 0.0; }),
             "MessageBubble never produced an implicit height");
    QVERIFY(bubble->implicitHeight() > 0.0);

    QQuickItem *label = nullptr;
    QVERIFY2(waitFor([&] {
                 label = findTextItem(bubble, QStringLiteral("hello"));
                 return label && label->width() > 0.0;
             }),
             "no laid-out Text item renders the message body");
    QVERIFY(label->property("visible").toBool());
    QVERIFY(label->isVisible());
    QVERIFY(label->width() > 0.0);
    QVERIFY(label->height() > 0.0);

    QQuickItem *frame = bubbleFrameFor(bubble, label);
    QVERIFY2(frame, "the body Text is not inside the bubble background");
    QVERIFY(frame->width() > 0.0);
    QVERIFY(frame->height() > 0.0);
    QVERIFY(frame->x() >= 0.0);
    QVERIFY(frame->x() < host->width() / 2.0);
}

void MessageBubbleRenderTest::outgoingTextBubbleAlignsRight()
{
    const std::unique_ptr<QQuickItem> host(newHost());
    QQuickItem *bubble = createBubble(host.get(), {
        {QStringLiteral("text"), QStringLiteral("hello")},
        {QStringLiteral("outgoing"), true},
        {QStringLiteral("date"),
         QVariant::fromValue<qint64>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("contentType"), QStringLiteral("text")},
    });
    QVERIFY2(bubble, qPrintable(m_component->errorString()));

    QQuickItem *label = nullptr;
    QVERIFY2(waitFor([&] {
                 label = findTextItem(bubble, QStringLiteral("hello"));
                 return label && label->width() > 0.0;
             }),
             "no laid-out Text item renders the outgoing message body");

    QQuickItem *frame = bubbleFrameFor(bubble, label);
    QVERIFY2(frame, "the outgoing text is not inside the bubble background");

    // Design.space4 == 16 px is the right margin used by MessageBubble.
    const qreal expectedRight = host->width() - 16.0;
    QVERIFY2(waitFor([&] {
                 return frame->width() > 0.0 && frame->x() > 0.0
                        && qAbs((frame->x() + frame->width()) - expectedRight) < 1.5;
             }),
             "the outgoing bubble never anchored to the right edge");

    QVERIFY(frame->x() > 0.0);
    // Incoming bubbles start at the 16 px left margin; outgoing ones sit past
    // it, with their right edge on the 16 px right margin (checked above).
    QVERIFY2(frame->x() > 17.0,
             qPrintable(QStringLiteral("chain=%1 host=%2 root w=%3 max=%4 avail=%5")
                            .arg(ancestorChain(label, bubble))
                            .arg(host->width())
                            .arg(bubble->width())
                            .arg(bubble->property("maxBubbleWidth").toReal())
                            .arg(bubble->property("availableWidth").toReal())));
    QVERIFY(qAbs((frame->x() + frame->width()) - expectedRight) < 1.5);
    QVERIFY(label->isVisible());
    QVERIFY(label->width() > 0.0);
}

void MessageBubbleRenderTest::stickerOnlyBubbleHasTransparentBackground()
{
    const std::unique_ptr<QQuickItem> host(newHost());
    QQuickItem *bubble = createBubble(host.get(), {
        {QStringLiteral("text"), QStringLiteral("")},
        {QStringLiteral("outgoing"), false},
        {QStringLiteral("date"),
         QVariant::fromValue<qint64>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("contentType"), QStringLiteral("sticker")},
    });
    QVERIFY2(bubble, qPrintable(m_component->errorString()));

    // StickerBubble's placeholder is what a sticker without a downloaded file
    // actually draws; use it to reach the enclosing bubble Rectangle.
    QQuickItem *placeholder = nullptr;
    QVERIFY2(waitFor([&] {
                 placeholder = findTextItem(bubble, QStringLiteral("[Sticker]"));
                 return placeholder && placeholder->isVisible() && placeholder->width() > 0.0;
             }),
             "the StickerBubble placeholder never became visible");

    QQuickItem *frame = bubbleFrameFor(bubble, placeholder);
    QVERIFY2(frame, "the sticker is not inside the bubble background");

    const QVariant colorProperty = frame->property("color");
    QVERIFY(colorProperty.isValid());
    QCOMPARE(colorProperty.value<QColor>().alpha(), 0);
    QVERIFY2(bubble->implicitHeight() > 0.0, "the sticker bubble collapsed");
}

void MessageBubbleRenderTest::emojiStickerRendersGlyph()
{
    const std::unique_ptr<QQuickItem> host(newHost());
    const QString emoji = QString::fromUtf8("\xF0\x9F\x99\x82"); // U+1F642
    QQuickItem *bubble = createBubble(host.get(), {
        {QStringLiteral("text"), emoji},
        {QStringLiteral("outgoing"), false},
        {QStringLiteral("date"),
         QVariant::fromValue<qint64>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("contentType"), QStringLiteral("sticker")},
    });
    QVERIFY2(bubble, qPrintable(m_component->errorString()));

    QQuickItem *glyph = nullptr;
    QVERIFY2(waitFor([&] {
                 glyph = findTextItem(bubble, emoji);
                 return glyph && glyph->width() > 0.0;
             }),
             "the emoji sticker Text never rendered");
    QVERIFY(glyph->property("visible").toBool());
    QVERIFY(glyph->isVisible());
    QVERIFY(glyph->width() > 0.0);
    QVERIFY(glyph->height() > 0.0);

    // A sticker never shows the photo/video/document placeholder frame.
    QVERIFY(!findVisibleTextItem(bubble, QStringLiteral("Photo")));

    // Note: MessageBubble's _stickerOnly also requires displayText() to be
    // empty. A sticker carrying a caption/emoji text therefore keeps the
    // normal bubble surface today; the transparent-surface contract is
    // covered by stickerOnlyBubbleHasTransparentBackground above.
}

void MessageBubbleRenderTest::photoBubbleShowsPlaceholderLabel()
{
    const std::unique_ptr<QQuickItem> host(newHost());
    QQuickItem *bubble = createBubble(host.get(), {
        {QStringLiteral("text"), QStringLiteral("")},
        {QStringLiteral("outgoing"), false},
        {QStringLiteral("date"),
         QVariant::fromValue<qint64>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("contentType"), QStringLiteral("photo")},
        {QStringLiteral("mediaFileId"), 0},
    });
    QVERIFY2(bubble, qPrintable(m_component->errorString()));

    QQuickItem *label = nullptr;
    QVERIFY2(waitFor([&] {
                 label = findTextItem(bubble, QStringLiteral("Photo"));
                 return label && label->isVisible() && label->width() > 0.0;
             }),
             "the photo placeholder label never became visible");
    QVERIFY(label->property("visible").toBool());
    QVERIFY(label->isVisible());
    QVERIFY(label->width() > 0.0);
}

// Qt Quick needs a QGuiApplication: QTEST_GUILESS_MAIN creates a
// QCoreApplication, which has no platform integration or font database, so
// QQuickItem text metrics and layout polish would stay at zero. This is the
// headless equivalent: offscreen platform, software scene graph, no visible
// desktop window, and the normal QTest::qExec runner.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("software"));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tst_message_bubble_render"));

    MessageBubbleRenderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_message_bubble_render.moc"
