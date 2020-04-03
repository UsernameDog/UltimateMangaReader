#include "abstractmangasource.h"

#include "mangainfo.h"

AbstractMangaSource::AbstractMangaSource(QObject *parent,
                                         DownloadManager *downloadmanager)
    : QObject(parent), downloadManager(downloadmanager), htmlConverter()
{
}

bool AbstractMangaSource::serializeMangaList()
{
    QFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << mangaList.titles;
    out << mangaList.links;
    out << mangaList.absoluteUrls;
    out << mangaList.nominalSize;
    out << mangaList.actualSize;

    file.close();

    return true;
}

bool AbstractMangaSource::deserializeMangaList()
{
    mangaList.links.clear();
    mangaList.titles.clear();

    QFile file(CONF.mangaListDir + name + "_mangalist.dat");
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QDataStream in(&file);
    in >> mangaList.titles;
    in >> mangaList.links;
    in >> mangaList.absoluteUrls;
    in >> mangaList.nominalSize;
    in >> mangaList.actualSize;

    file.close();

    return true;
}

QString AbstractMangaSource::getImagePath(
    const DownloadImageDescriptor &descriptor)
{
    int ind = descriptor.imageUrl.indexOf('?');
    if (ind == -1)
        ind = descriptor.imageUrl.length();
    QString filetype = descriptor.imageUrl.mid(ind - 4, 4);

    QString path = CONF.mangaimagesdir(name, descriptor.title) +
                   QString::number(descriptor.chapter) + "_" +
                   QString::number(descriptor.page) + filetype;

    return path;
}

QSharedPointer<DownloadFileJob> AbstractMangaSource::downloadImage(
    const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    return downloadManager->downloadAsScaledImage(descriptor.imageUrl, path);
}

Result<QString, QString> AbstractMangaSource::downloadAwaitImage(
    const DownloadImageDescriptor &descriptor)
{
    QString path = getImagePath(descriptor);

    if (QFile::exists(path))
        return Ok(path);

    auto job =
        downloadManager->downloadAsScaledImage(descriptor.imageUrl, path);

    if (job->await(5000))
        return Ok(path);
    else
        return Err(job->errorString);
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::loadMangaInfo(
    const QString &mangalink, const QString &mangatitle, bool update)
{
    QString path(CONF.mangainfodir(name, mangatitle) + "mangainfo.dat");
    if (QFile::exists(path))
    {
        auto info = MangaInfo::deserialize(this, path);
        if (update)
            info->mangaSource->updateMangaInfoAsync(info);

        return Ok(info);
    }
    else
    {
        auto infoR = getMangaInfo(mangalink);

        if (infoR.isOk())
            infoR.unwrap()->serialize();

        return infoR;
    }
}

Result<QSharedPointer<MangaInfo>, QString> AbstractMangaSource::getMangaInfo(
    const QString &mangalink)
{
    auto job = downloadManager->downloadAsString(mangalink);

    auto info = QSharedPointer<MangaInfo>(new MangaInfo(this));

    info->mangaSource = this;
    info->hostname = name;

    info->link = mangalink;

    if (!job->await(5000))
        return Err(job->errorString);

    updateMangaInfoFinishedLoading(job, info);

    downloadCoverAsync(info);

    return Ok(info);
}

void AbstractMangaSource::updateMangaInfoAsync(QSharedPointer<MangaInfo> info)
{
    int oldnumchapters = info->chapters.count();

    auto job = downloadManager->downloadAsString(info->link);

    auto lambda = [oldnumchapters, info, job, this] {
        bool newchapters = info->chapters.count() > oldnumchapters;

        {
            QMutexLocker locker(info->updateMutex.get());
            updateMangaInfoFinishedLoading(job, info);
        }

        info->updateCompeted(newchapters);

        downloadCoverAsync(info);
        info->serialize();
    };

    //    job->await(3000);
    //    lambda();

    executeOnJobCompletion(job, lambda);
}

Result<void, QString> AbstractMangaSource::updatePageList(
    QSharedPointer<MangaInfo> info, int chapter)
{
    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (info->chapters[chapter].pagesLoaded)
        return Ok();

    auto newpagelistR = getPageList(info->chapters[chapter].chapterUrl);

    if (!newpagelistR.isOk())
        return Err(newpagelistR.unwrapErr());

    auto newpagelist = newpagelistR.unwrap();

    QMutexLocker locker(info->updateMutex.get());

    if (chapter >= info->chapters.count() || chapter < 0)
        return Err(QString("Chapter number out of bounds."));
    auto &ch = info->chapters[chapter];

    ch.pageUrlList = newpagelist;

    if (ch.pageUrlList.count() == 0)
    {
        qDebug() << "pagelinks empty" << ch.chapterUrl;
        ch.numPages = 1;
        ch.pageUrlList.clear();
        ch.pageUrlList << "";
        return Err(QString("Can't download chapter: pagelist empty."));
    }
    ch.numPages = ch.pageUrlList.count();
    ch.imageUrlList = QStringList();
    for (int i = 0; i < ch.pageUrlList.count(); i++)
        ch.imageUrlList.append("");
    ch.pagesLoaded = true;

    return Ok();
}

void AbstractMangaSource::genrateCoverThumbnail(
    QSharedPointer<MangaInfo> mangainfo)
{
    QString scpath = mangainfo->coverThumbnailPath();

    if (!QFile::exists(scpath))
    {
        QImage img;
        img.load(mangainfo->coverPath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }
}

void AbstractMangaSource::downloadCoverAsync(
    QSharedPointer<MangaInfo> mangainfo)
{
    if (mangainfo->coverLink == "")
    {
        return;
    }

    if (mangainfo->coverPath == "")
    {
        int ind = mangainfo->coverLink.indexOf('?');
        if (ind == -1)
            ind = mangainfo->coverLink.length();
        QString filetype = mangainfo->coverLink.mid(ind - 4, 4);
        mangainfo->coverPath =
            CONF.mangainfodir(name, mangainfo->title) + "cover" + filetype;
    }

    auto coverjob = downloadManager->downloadAsFile(mangainfo->coverLink,
                                                    mangainfo->coverPath);

    auto lambda = [this, mangainfo]() {
        genrateCoverThumbnail(mangainfo);
        mangainfo->sendCoverLoaded();
    };

    //    coverjob->await(1000);
    //    lambda();

    executeOnJobCompletion(coverjob, lambda);
}

QString AbstractMangaSource::htmlToPlainText(const QString &str)
{
    htmlConverter.setHtml(str);
    return htmlConverter.toPlainText();
}

void AbstractMangaSource::fillMangaInfo(
    QSharedPointer<MangaInfo> info, const QString &buffer,
    const QRegularExpression &titlerx, const QRegularExpression &authorrx,
    const QRegularExpression &artistrx, const QRegularExpression &statusrx,
    const QRegularExpression &yearrx, const QRegularExpression &genresrx,
    const QRegularExpression &summaryrx, const QRegularExpression &coverrx)
{
    auto titlerxmatch = titlerx.match(buffer);
    auto authorrxmatch = authorrx.match(buffer);
    auto artistrxmatch = artistrx.match(buffer);
    auto statusrxmatch = statusrx.match(buffer);
    auto yearrxmatch = yearrx.match(buffer);
    auto genresrxmatch = genresrx.match(buffer);
    auto summaryrxmatch = summaryrx.match(buffer);
    auto coverrxmatch = coverrx.match(buffer);

    if (titlerxmatch.hasMatch())
        info->title = htmlToPlainText(titlerxmatch.captured(1)).trimmed();
    if (authorrxmatch.hasMatch())
        info->author = htmlToPlainText(authorrxmatch.captured(1)).remove('\n');
    if (artistrxmatch.hasMatch())
        info->artist = htmlToPlainText(artistrxmatch.captured(1)).remove('\n');
    if (statusrxmatch.hasMatch())
        info->status = htmlToPlainText(statusrxmatch.captured(1));
    if (yearrxmatch.hasMatch())
        info->releaseYear = htmlToPlainText(yearrxmatch.captured(1));
    if (genresrxmatch.hasMatch())
        info->genres = htmlToPlainText(genresrxmatch.captured(1))
                           .trimmed()
                           .remove('\n')
                           .remove(',');
    if (summaryrxmatch.hasMatch())
        info->summary = htmlToPlainText(summaryrxmatch.captured(1));
    if (coverrxmatch.hasMatch())
        info->coverLink = coverrxmatch.captured(1);
}
