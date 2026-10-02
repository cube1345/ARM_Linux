#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "main_window.h"
#include "config.h"

static QFile g_logFile;

static void logHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }
    const QByteArray line = QStringLiteral("[%1] [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
        .arg(QLatin1String(level))
        .arg(msg).toUtf8();
    fwrite(line.constData(), 1, size_t(line.size()), stderr);
    if (g_logFile.isOpen()) {
        g_logFile.write(line);
        g_logFile.flush();
    }
}

int main(int argc, char *argv[])
{
    g_logFile.setFileName(Config::kLogFile);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(logHandler);

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Photo Album"));
    application.setApplicationVersion(QStringLiteral("1.1-touchdiag"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Touch photo album for i.MX6ULL"));
    parser.addPositionalArgument(QStringLiteral("directory"), QStringLiteral("Photo directory"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

    const QString argument = parser.positionalArguments().value(0);
    QString photoDirectory;
    if (!argument.isEmpty()) {
        photoDirectory = argument;
    } else if (QDir(QStringLiteral("photos")).entryList(QDir::Files).size() > 0) {
        photoDirectory = QStringLiteral("photos");
    } else if (QDir(QStringLiteral("/root/photos")).entryList(QDir::Files).size() > 0) {
        photoDirectory = QStringLiteral("/root/photos");
    } else {
        photoDirectory = QString();
    }

    MainWindow window(photoDirectory);
#ifdef __arm__
    window.showFullScreen();
#else
    window.resize(480, 272);
    window.show();
#endif
    return application.exec();
}