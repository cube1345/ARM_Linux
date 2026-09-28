#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

namespace Config {

// 板端路径
const QString kPhotoDir = QStringLiteral("/home/root/photos");
const QString kMusicDir = QStringLiteral("/home/root/music");
const QString kWallpaper = QStringLiteral("/home/root/wallpaper.jpg");
const QString kLogFile = QStringLiteral("/home/root/app.log");

// 网络
const QString kWeatherUrl = QStringLiteral("http://wttr.in/?format=j1&lang=zh");

// 外设设备节点
const QString kImuDevice = QStringLiteral("/dev/spidev2.0");
const QString kAlsDevice = QStringLiteral("/dev/i2c-0");

// 屏幕
const int kScreenW = 1024;
const int kScreenH = 600;

} // namespace Config

#endif
