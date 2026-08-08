#include "main_window.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("Media Browser Qt");
    application.setApplicationVersion("1.0");
    application.setStyleSheet(
        "QWidget{background:#101722;color:#e7edf5;font-family:Sans;font-size:15px;}"
        "QPushButton{background:#1b2838;border:1px solid #2d4159;border-radius:8px;padding:10px;text-align:left;}"
        "QPushButton:hover{background:#263b53;} QPushButton:checked{background:#2f80ed;}"
        "QListWidget,QTextEdit{background:#151f2d;border:1px solid #2d4159;border-radius:8px;}"
        "QLabel#title{font-size:24px;font-weight:bold;} QLabel#muted{color:#91a4b9;}"
        "QStatusBar{color:#91a4b9;}"
    );
    QString root = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::homePath();
    MainWindow window(root);
    window.show();
    return application.exec();
}
