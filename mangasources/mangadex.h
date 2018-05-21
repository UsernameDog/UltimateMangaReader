#ifndef MANGADEX_H
#define MANGADEX_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaDex : public AbstractMangaSource
{
public:

    MangaDex(QObject *parent, DownloadManager *dm);

    bool updateMangaList();
    MangaInfo *getMangaInfo(QString mangalink);
    QStringList *getPageList(QString chapterlink);
    QString getImageLink(QString pagelink);

};

#endif // MANGADEX_H
