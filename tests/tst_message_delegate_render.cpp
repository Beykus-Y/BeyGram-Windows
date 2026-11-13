#include "telegram/MessageListModel.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>

namespace test_stubs {

class StubTheme final : public QObject {
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
    explicit StubTheme(QObject* parent = nullptr) : QObject(parent) {}

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

class StubAppearance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString bubbleStyle READ bubbleStyle CONSTANT)
    Q_PROPERTY(QString designStyle READ designStyle CONSTANT)
    Q_PROPERTY(QString listDensity READ listDensity CONSTANT)
    Q_PROPERTY(qreal messageTextSize READ messageTextSize CONSTANT)
    Q_PROPERTY(bool reduceMotion READ reduceMotion CONSTANT)

public:
    explicit StubAppearance(QObject* parent = nullptr) : QObject(parent) {}

    QString bubbleStyle() const { return QStringLiteral("Rounded"); }
    QString designStyle() const { return QStringLiteral("Default"); }
    QString listDensity() const { return QStringLiteral("Comfortable"); }
    qreal messageTextSize() const { return 14.0; }
    bool reduceMotion() const { return true; }
};

class StubApp final : public QObject {
    Q_OBJECT

public:
    explicit StubApp(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void openLink(const QString& url) { Q_UNUSED(url) }
    Q_INVOKABLE void copyText(const QString& text) { Q_UNUSED(text) }
};

} // namespace test_stubs

namespace {

bool waitFor(const std::function<bool()>& predicate, int timeoutMs = 3000)
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

void collectVisualItems(QQuickItem* root, QList<QQuickItem*>& out)
{
    const auto children = root->childItems();
    for (QQuickItem* child : children) {
        out.append(child);
        collectVisualItems(child, out);
    }
}

QQuickItem* findTextItem(QQuickItem* root, const QString& text)
{
    QList<QQuickItem*> items;
    collectVisualItems(root, items);
    QQuickItem* firstMatch = nullptr;
    for (QQuickItem* item : items) {
        if (!item->inherits("QQuickText") || item->property("text").toString() != text)
            continue;
        if (item->isVisible())
            return item;
        if (!firstMatch)
            firstMatch = item;
    }
    return firstMatch;
}

} // namespace

class MessageDelegateRenderTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void directInstantiationRenders();
    void rendersTextRole();
    void passesOutgoingRole();

private:
    QQuickItem* createHost();
    QVariantMap roleProperties(const QString& text, bool outgoing) const;
    beygram::telegram::MessageData message(const QString& text, bool outgoing);

    test_stubs::StubTheme m_theme;
    test_stubs::StubAppearance m_appearance;
    test_stubs::StubApp m_app;
    beygram::telegram::MessageListModel m_model;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQmlComponent> m_hostComponent;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem* m_host = nullptr;
};

void MessageDelegateRenderTest::initTestCase()
{
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &m_theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &m_appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &m_app);

    m_engine = std::make_unique<QQmlEngine>();
    const QString appDir = QCoreApplication::applicationDirPath();
    m_engine->addImportPath(appDir);
    m_engine->addImportPath(QDir(appDir).absoluteFilePath(QStringLiteral("..")));
    m_engine->addImportPath(QDir(appDir).absoluteFilePath(QStringLiteral("../..")));
    m_engine->rootContext()->setContextProperty(
        QStringLiteral("testModel"), static_cast<QAbstractItemModel*>(&m_model));

    m_window = std::make_unique<QQuickWindow>();
    m_window->resize(480, 320);
    m_window->show();
    QTest::qWait(50);

    m_hostComponent = std::make_unique<QQmlComponent>(m_engine.get());
    m_hostComponent->setData(QByteArrayLiteral(R"(
        import QtQuick
        import BeyGram
        Item {
            width: 480
            height: 320
            ListView {
                id: view
                anchors.fill: parent
                model: testModel
                delegate: MessageDelegate {}
            }
        }
    )"), QUrl(QStringLiteral("qrc:/message-delegate-host.qml")));
    QVERIFY2(m_hostComponent->status() == QQmlComponent::Ready,
             qPrintable(m_hostComponent->errorString()));
}

void MessageDelegateRenderTest::cleanupTestCase()
{
    m_window.reset();
    m_hostComponent.reset();
    m_engine.reset();
}

beygram::telegram::MessageData MessageDelegateRenderTest::message(const QString& text,
                                                                  bool outgoing)
{
    beygram::telegram::MessageData data;
    data.id = 1;
    data.chatId = 1;
    data.date = QDateTime::currentSecsSinceEpoch();
    data.text = text;
    data.contentType = QStringLiteral("text");
    data.outgoing = outgoing;
    return data;
}

QVariantMap MessageDelegateRenderTest::roleProperties(const QString& text, bool outgoing) const
{
    return {
        {QStringLiteral("messageId"), 1},
        {QStringLiteral("text"), text},
        {QStringLiteral("textHtml"), QString()},
        {QStringLiteral("replyPreview"), QString()},
        {QStringLiteral("reactionsJson"), QString()},
        {QStringLiteral("linkUrl"), QString()},
        {QStringLiteral("linkTitle"), QString()},
        {QStringLiteral("linkDescription"), QString()},
        {QStringLiteral("linkSite"), QString()},
        {QStringLiteral("linkFileId"), 0},
        {QStringLiteral("linkVideo"), false},
        {QStringLiteral("pollQuestion"), QString()},
        {QStringLiteral("pollOptionsJson"), QString()},
        {QStringLiteral("pollTotal"), 0},
        {QStringLiteral("pollClosed"), false},
        {QStringLiteral("pollQuiz"), false},
        {QStringLiteral("pollMultiple"), false},
        {QStringLiteral("outgoing"), outgoing},
        {QStringLiteral("senderName"), QString()},
        {QStringLiteral("date"), QDateTime::currentSecsSinceEpoch()},
        {QStringLiteral("pending"), false},
        {QStringLiteral("failed"), false},
        {QStringLiteral("edited"), false},
        {QStringLiteral("contentType"), QStringLiteral("text")},
        {QStringLiteral("mediaFileId"), 0},
        {QStringLiteral("mediaFullFileId"), 0},
        {QStringLiteral("mediaWidth"), 0},
        {QStringLiteral("mediaHeight"), 0},
        {QStringLiteral("mediaReady"), false},
        {QStringLiteral("mediaFailed"), false},
        {QStringLiteral("mediaRevision"), 0},
    };
}

void MessageDelegateRenderTest::directInstantiationRenders()
{
    QQmlComponent delegate(m_engine.get(),
                           QUrl(QStringLiteral("qrc:/qt/qml/BeyGram/qml/components/"
                                               "MessageDelegate.qml")));
    QVERIFY2(delegate.status() == QQmlComponent::Ready, qPrintable(delegate.errorString()));
    QScopedPointer<QObject> object(
        delegate.createWithInitialProperties(roleProperties(QStringLiteral("direct body"), false)));
    QVERIFY2(object, qPrintable(delegate.errorString()));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(item);
    item->setParentItem(m_window->contentItem());

    QQuickItem* label = nullptr;
    QVERIFY2(waitFor([&] {
                 label = findTextItem(item, QStringLiteral("direct body"));
                 return label != nullptr;
             }),
             "direct MessageDelegate did not render its MessageBubble");
    QVERIFY(label->isVisible());
    QVERIFY(label->width() > 0.0);
}

QQuickItem* MessageDelegateRenderTest::createHost()
{
    if (m_host) {
        delete m_host;
        m_host = nullptr;
    }
    QScopedPointer<QObject> object(m_hostComponent->create());
    if (!object)
        return nullptr;
    auto* item = qobject_cast<QQuickItem*>(object.data());
    if (!item)
        return nullptr;
    object.take();
    item->setParentItem(m_window->contentItem());
    return item;
}

void MessageDelegateRenderTest::rendersTextRole()
{
    m_model.setMessages({message(QStringLiteral("delegate hello"), false)});
    m_host = createHost();
    QVERIFY2(m_host, qPrintable(m_hostComponent->errorString()));

    QVERIFY2(waitFor([this] {
                 return findTextItem(m_host, QStringLiteral("delegate hello")) != nullptr;
             }),
             qPrintable(QStringLiteral("MessageDelegate did not pass the text role to "
                                       "MessageBubble (model=%1, %2)")
                            .arg(m_model.count())
                            .arg([this] {
                                QString dump;
                                QList<QQuickItem*> items;
                                collectVisualItems(m_host, items);
                                for (QQuickItem* item : items) {
                                    const QString className =
                                        QString::fromLatin1(item->metaObject()->className());
                                    if (className.contains(QStringLiteral("Message"))
                                        || className.contains(QStringLiteral("Bubble"))) {
                                        dump.append(QStringLiteral("%1{text=%2,vis=%3} ")
                                                        .arg(className,
                                                             item->property("text").toString(),
                                                             item->isVisible()
                                                                 ? QStringLiteral("1")
                                                                 : QStringLiteral("0")));
                                    }
                                }
                                return dump;
                            }())));
    QQuickItem* label = findTextItem(m_host, QStringLiteral("delegate hello"));
    QVERIFY(label);
    QVERIFY(label->isVisible());
    QVERIFY(label->width() > 0.0);
}

void MessageDelegateRenderTest::passesOutgoingRole()
{
    m_model.setMessages({message(QStringLiteral("outgoing body"), true)});
    m_host = createHost();
    QVERIFY2(m_host, qPrintable(m_hostComponent->errorString()));

    QQuickItem* label = nullptr;
    QVERIFY2(waitFor([this, &label] {
                 label = findTextItem(m_host, QStringLiteral("outgoing body"));
                 return label && label->width() > 0.0;
             }),
             "MessageDelegate did not render the outgoing body");

    QQuickItem* bubbleRoot = nullptr;
    QList<QQuickItem*> items;
    collectVisualItems(m_host, items);
    for (QQuickItem* item : items) {
        if (QString::fromLatin1(item->metaObject()->className())
                .startsWith(QStringLiteral("MessageBubble"))) {
            bubbleRoot = item;
            break;
        }
    }
    QVERIFY2(bubbleRoot != nullptr, "no MessageBubble under the delegate");
    QCOMPARE(bubbleRoot->property("outgoing").toBool(), true);
    QCOMPARE(bubbleRoot->property("text").toString(), QStringLiteral("outgoing body"));
}

QTEST_MAIN(MessageDelegateRenderTest)
#include "tst_message_delegate_render.moc"
