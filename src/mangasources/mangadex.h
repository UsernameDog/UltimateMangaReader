#ifndef MANGADEX_API_H
#define MANGADEX_API_H

#include <QJsonObject>
#include <QList>
#include <QString>

// Funkcja pobiera listę rozdziałów dla danej mangi w określonym języku
QList<QJsonObject> fetchChapters(const QString &mangaId, const QString &language = "en");

#endif // MANGADEX_API_H
