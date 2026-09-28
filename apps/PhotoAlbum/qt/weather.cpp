#include "weather.h"
#include "config.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

Weather::Weather(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

void Weather::fetch()
{
    QNetworkRequest request(QUrl(Config::kWeatherUrl));
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFinished(reply);
        reply->deleteLater();
    });
}

void Weather::onFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit ready(QString(), QStringLiteral("天气不可用"), QString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isNull())
        return;
    const QJsonObject root = doc.object();
    const QJsonArray current = root.value(QStringLiteral("current_condition")).toArray();
    if (current.isEmpty())
        return;
    const QJsonObject cond = current.first().toObject();
    const QString temp = cond.value(QStringLiteral("temp_C")).toString();
    const QJsonArray descArr = cond.value(QStringLiteral("weatherDesc")).toArray();
    const QString desc = descArr.isEmpty() ? QString()
        : descArr.first().toObject().value(QStringLiteral("value")).toString();
    const QJsonArray areaArr = root.value(QStringLiteral("nearest_area")).toArray();
    const QString area = areaArr.isEmpty() ? QString()
        : areaArr.first().toObject().value(QStringLiteral("areaName")).toArray()
              .first().toObject().value(QStringLiteral("value")).toString();
    emit ready(temp, desc, area);
}
