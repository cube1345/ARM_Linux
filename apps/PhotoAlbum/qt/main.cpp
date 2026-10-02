#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
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

namespace {
// 全屏开屏动画绘制：黑底 + 等比放大铺满屏幕（cover），超出部分裁切，
// 屏幕中心内容保证完整可见。
class SplashWidget : public QWidget {
public:
    explicit SplashWidget(QMovie *m, QWidget *parent = nullptr)
        : QWidget(parent), m_movie(m)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
        connect(m_movie, &QMovie::destroyed, this, [this]() { m_movie = nullptr; });
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        if (!m_movie)
            return;
        const QPixmap pm = m_movie->currentPixmap();
        if (pm.isNull())
            return;
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        // cover：等比放大到完全覆盖窗口，超出部分裁切
        const qreal scale = qMax(qreal(width()) / pm.width(),
                                 qreal(height()) / pm.height());
        const QSize scaled = pm.size() * scale;
        const QRect target((width() - scaled.width()) / 2,
                           (height() - scaled.height()) / 2,
                           scaled.width(), scaled.height());
        painter.drawPixmap(target, pm);
    }
private:
    QMovie *m_movie;
};

// 开屏动画跳过：触摸 / 鼠标按下任意处即退出
class SplashSkipFilter : public QObject {
public:
    explicit SplashSkipFilter(QEventLoop *loop) : m_loop(loop) {}
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::MouseButtonPress ||
            ev->type() == QEvent::TouchBegin) {
            m_loop->quit();
            return true;
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    QEventLoop *m_loop;
};
}

// 启动开屏动画：程序同目录找 opening.gif（或 demo_media/opening.gif），
// 找不到直接跳过；播完一帧循环或任一时间被打断即进入主界面。
static void showOpeningSplash()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    QString gifPath;
    for (const QString &cand : {QStringLiteral("opening.gif"),
                                QStringLiteral("demo_media/opening.gif")}) {
        if (QFile::exists(appDir.filePath(cand))) {
            gifPath = appDir.filePath(cand);
            break;
        }
    }
    if (gifPath.isEmpty()) {
        qInfo() << "[SPLASH] no opening.gif found, skip";
        return;
    }
    qInfo() << "[SPLASH] playing:" << gifPath;

    QDialog splash(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    QMovie *movie = new QMovie(gifPath, QByteArray(), &splash);
    SplashWidget *widget = new SplashWidget(movie, &splash);
    QVBoxLayout *layout = new QVBoxLayout(&splash);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);

#ifdef __arm__
    splash.showFullScreen();
#else
    splash.resize(480, 272);
    splash.show();
#endif

    QEventLoop loop;
    SplashSkipFilter filter(&loop);
    splash.installEventFilter(&filter);
    // 开屏循环播放，10 秒（与 gif 等长）后自动进主界面；触摸随时可跳过
    QTimer::singleShot(10000, &splash, [&loop]() {
        qInfo() << "[SPLASH] 10s done, entering desktop";
        loop.quit();
    });

    movie->start();
    loop.exec();
    splash.close();
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

    showOpeningSplash();

    MainWindow window(photoDirectory);
#ifdef __arm__
    window.showFullScreen();
#else
    window.resize(480, 272);
    window.show();
#endif
    return application.exec();
}
