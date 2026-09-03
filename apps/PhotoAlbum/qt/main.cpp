#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "main_window.h"

int main(int argc, char *argv[])
{
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
