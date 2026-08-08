#include "main_window.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("Media Browser Qt");
    application.setApplicationVersion("1.0");
    application.setStyleSheet(
        "QWidget{background:#111923;color:#d9e2ec;font-family:Sans;font-size:13px;}"
        "QMainWindow{background:#111923;} QFrame#sidebar{background:#0d151f;}"
        "QPushButton#navButton{background:transparent;color:#9eafc1;border:0;"
        "border-radius:6px;padding:8px 10px;text-align:left;min-height:18px;}"
        "QPushButton#navButton:hover{background:#182536;color:#e7edf5;}"
        "QPushButton#navButton:pressed{background:#23466b;color:#ffffff;}"
        "QListWidget,QTextEdit{background:#151f2b;border:1px solid #26384b;"
        "border-radius:6px;padding:4px;}"
        "QListWidget::item{border-radius:5px;padding:6px;color:#c6d2df;}"
        "QListWidget::item:hover{background:#1b2b3d;}"
        "QListWidget::item:selected{background:#244b74;color:#ffffff;}"
        "QLabel#title{font-size:19px;font-weight:600;color:#eef4fa;}"
        "QLabel#pageTitle{font-size:21px;font-weight:500;color:#eef4fa;}"
        "QLabel#muted{color:#7f93a8;} QStatusBar{color:#7f93a8;"
        "background:#0d151f;border-top:1px solid #1b2b3b;}"
    );
    QString root = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::homePath();
    MainWindow window(root);
    window.show();
    return application.exec();
}
