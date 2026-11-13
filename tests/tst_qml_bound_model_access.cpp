#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QtTest>

namespace {

QString sourceQmlRoot()
{
    const QString candidate = QFINDTESTDATA("../qml");
    if (!candidate.isEmpty())
        return QFileInfo(candidate).absoluteFilePath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList relative = {
        QStringLiteral("qml"),
        QStringLiteral("../qml"),
        QStringLiteral("../../qml"),
        QStringLiteral("../../../qml"),
    };
    for (const QString& path : relative) {
        const QString absolute = QDir::cleanPath(QDir(appDir).absoluteFilePath(path));
        if (QFileInfo(absolute).isDir())
            return absolute;
    }
    return {};
}

} // namespace

/// Guards the QML rule that broke the chat view: in a file with
/// `pragma ComponentBehavior: Bound`, a delegate cannot read `model`
/// implicitly. The original ChatsPage delegate used `model.text` and friends,
/// every binding threw `ReferenceError: model is not defined`, and the chat
/// showed correctly sized but completely empty bubbles.
class QmlBoundModelAccessTest final : public QObject {
    Q_OBJECT

private slots:
    void boundFilesDoNotReadModelImplicitly();
};

void QmlBoundModelAccessTest::boundFilesDoNotReadModelImplicitly()
{
    const QString root = sourceQmlRoot();
    QVERIFY2(!root.isEmpty(), "qml source tree was not found");

    const QRegularExpression pragma(
        QStringLiteral("^\\s*pragma\\s+ComponentBehavior\\s*:\\s*Bound\\b"));
    const QRegularExpression modelAccess(
        QStringLiteral("(?<![\\w.$])model\\s*[.\\[]"));

    QStringList boundFiles;
    QStringList violations;

    QDirIterator iterator(root, {QStringLiteral("*.qml")}, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream stream(&file);
        const QStringList lines = stream.readAll().split(QLatin1Char('\n'));
        bool bound = false;
        for (const QString& line : lines) {
            if (pragma.match(line).hasMatch()) {
                bound = true;
                break;
            }
            if (!line.trimmed().isEmpty() && !line.trimmed().startsWith(QLatin1Char('/')))
                break;
        }
        if (!bound)
            continue;

        QString boundRelative = QDir(root).relativeFilePath(path);
        boundRelative.replace(QLatin1Char('\\'), QLatin1Char('/'));
        boundFiles.append(boundRelative);
        for (qsizetype i = 0; i < lines.size(); ++i) {
            QString code = lines.at(i);
            const qsizetype comment = code.indexOf(QStringLiteral("//"));
            if (comment >= 0)
                code.truncate(comment);
            if (modelAccess.match(code).hasMatch()) {
                QString relative = QDir(root).relativeFilePath(path);
                relative.replace(QLatin1Char('\\'), QLatin1Char('/'));
                violations.append(QStringLiteral("%1:%2: %3")
                                      .arg(relative)
                                      .arg(i + 1)
                                      .arg(code.trimmed()));
            }
        }
    }

    QVERIFY2(!boundFiles.isEmpty(), "no Bound QML files were found");
    QVERIFY2(boundFiles.contains(QStringLiteral("pages/ChatsPage.qml")),
             "ChatsPage.qml was not recognized as a Bound file");
    QVERIFY2(violations.isEmpty(),
             qPrintable(QStringLiteral("Bound QML delegates must declare required "
                                       "properties instead of reading `model`:\n%1")
                            .arg(violations.join(QLatin1Char('\n')))));
}

QTEST_GUILESS_MAIN(QmlBoundModelAccessTest)
#include "tst_qml_bound_model_access.moc"
