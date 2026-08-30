#include "main_window.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("Media Browser Qt");
    application.setApplicationVersion("1.0");
    application.setStyleSheet(
        "QWidget{background:#f3f7fb;color:#253447;font-family:Sans;font-size:13px;}"
        "QMainWindow{background:#f3f7fb;} QWidget#desktopPage{background:#f3f7fb;}"
        "QLabel#desktopTitle{font-size:27px;font-weight:600;color:#203247;}"
        "QLabel#desktopHint{font-size:15px;color:#718398;}"
        "QLabel#desktopFooter{color:#9aaabd;font-size:12px;}"
        "QToolButton#appIcon{background:transparent;border:0;border-radius:16px;"
        "color:#33465a;font-size:13px;padding:5px;}"
        "QToolButton#appIcon:hover{background:#e5eef8;color:#1d5d9d;}"
        "QToolButton#appIcon:pressed{background:#d5e6f8;}"
        "QPushButton#homeButton{background:#e5eef8;color:#3f6f9f;border:0;"
        "border-radius:14px;padding:6px 13px;font-weight:500;}"
        "QPushButton#homeButton:hover{background:#d5e3f1;}"
        "QPushButton#navButton{background:transparent;color:#5a6d81;border:0;"
        "border-radius:6px;padding:8px 10px;text-align:left;min-height:18px;}"
        "QPushButton#navButton:hover{background:#d5e3f1;color:#203247;}"
        "QPushButton#navButton:pressed{background:#4d8fd8;color:#ffffff;}"
        "QListWidget,QTextEdit{background:#ffffff;border:1px solid #d5e0eb;"
        "border-radius:6px;padding:4px;}"
        "QListWidget::item{border-radius:5px;padding:6px;color:#33465a;}"
        "QListWidget::item:hover{background:#edf4fb;}"
        "QListWidget::item:selected{background:#d8e9fa;color:#1d5d9d;}"
        "QLabel#title{font-size:19px;font-weight:600;color:#203247;}"
        "QLabel#pageTitle{font-size:21px;font-weight:500;color:#203247;}"
        "QLabel#muted{color:#718398;} QStatusBar{color:#718398;"
        "background:#e7eef6;border-top:1px solid #d4e0eb;}"
    );
    QString root = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::homePath();
    MainWindow window(root);
    window.show();
    return application.exec();
}
