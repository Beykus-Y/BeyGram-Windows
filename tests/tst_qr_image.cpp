#include "ui/QrCodeImageProvider.h"

#include <QtTest>

using beygram::ui::QrCodeImageProvider;

class QrCodeImageProviderTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyLinkProducesNoImage();
    void encodesTelegramLoginLink();
    void updatesWhenLinkChanges();
};

void QrCodeImageProviderTest::emptyLinkProducesNoImage()
{
    QrCodeImageProvider provider;
    QSize size;
    const QImage image = provider.requestImage(QStringLiteral("current"), &size, QSize(280, 280));
    QVERIFY(image.isNull());
    QVERIFY(!size.isValid());
}

void QrCodeImageProviderTest::encodesTelegramLoginLink()
{
    QrCodeImageProvider provider;
    provider.setLink(QStringLiteral("tg://login?token=0123456789abcdef"));

    QSize size;
    const QImage image = provider.requestImage(QStringLiteral("current"), &size, QSize(280, 280));
    QVERIFY(!image.isNull());
    QVERIFY(size.width() >= 200);
    QCOMPARE(image.size(), size);

    // A QR image must contain both black modules and white space.
    const QRgb topLeft = image.pixel(0, 0);
    QCOMPARE(qRed(topLeft), 255);
    bool hasBlack = false;
    for (int y = 0; y < image.height() && !hasBlack; y += 4) {
        for (int x = 0; x < image.width(); x += 4) {
            if (qRed(image.pixel(x, y)) < 32) {
                hasBlack = true;
                break;
            }
        }
    }
    QVERIFY(hasBlack);
}

void QrCodeImageProviderTest::updatesWhenLinkChanges()
{
    QrCodeImageProvider provider;
    provider.setLink(QStringLiteral("tg://login?token=first"));
    QSize firstSize;
    const QImage first = provider.requestImage(QStringLiteral("current"), &firstSize, QSize());

    provider.setLink(QStringLiteral("tg://login?token=second-different-value"));
    QSize secondSize;
    const QImage second = provider.requestImage(QStringLiteral("current"), &secondSize, QSize());

    QVERIFY(!first.isNull());
    QVERIFY(!second.isNull());
    QVERIFY(first != second);
}

QTEST_GUILESS_MAIN(QrCodeImageProviderTest)
#include "tst_qr_image.moc"
