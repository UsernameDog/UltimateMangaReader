#include "mangadex_api.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QUrl>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

QList<QJsonObject> fetchChapters(const QString &mangaId, const QString &language)
{
    QList<QJsonObject> chapters;
    QNetworkAccessManager manager;
    int offset = 0;
    const int limit = 100;

    while (true) {
        QUrl url("https://api.mangadex.org/chapter");
        QUrlQuery query;
        query.addQueryItem("manga", mangaId);
        query.addQueryItem("limit", QString::number(limit));
        query.addQueryItem("offset", QString::number(offset));
        query.addQueryItem("translatedLanguage[]", language);
        url.setQuery(query);

        QNetworkRequest request(url);
        QNetworkReply* reply = manager.get(request);

        // Używamy QEventLoop, aby poczekać na zakończenie żądania (w realnej aplikacji warto zastosować podejście asynchroniczne)
        QEventLoop loop;
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject json = doc.object();
        QJsonArray dataArray = json["data"].toArray();

        for (int i = 0; i < dataArray.size(); i++) {
            chapters.append(dataArray[i].toObject());
        }

        // Jeśli liczba pobranych elementów jest mniejsza niż limit – kończymy pętlę
        if (dataArray.size() < limit) {
            break;
        }
        offset += limit;
    }
    return chapters;
}
