#ifndef WEATHER_H
#define WEATHER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class Weather : public QObject
{
    Q_OBJECT
public:
    explicit Weather(QObject *parent = nullptr);
    void fetch();

signals:
    void ready(const QString &temp, const QString &desc, const QString &area);

private slots:
    void onFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam;
};

#endif
