#include "downloadimageandrescalejob.h"
#include "downloadmanager.h"
#include <QImage>
#include <QPixmap>
#include <QtConcurrentRun>

//#include <QtTest/QTest>



DownloadScaledImageJob::DownloadScaledImageJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &path, int width, int height) :
    DownloadFileJob(parent, nm, url, path),
    width(width),
    height(height)
{
}


void DownloadScaledImageJob::downloadFileReadyRead()
{
//    file.write(reply->readAll());
}

void DownloadScaledImageJob::downloadFileFinished()
{

    reply->deleteLater();

    if ( QNetworkReply::NoError != reply->error() )
    {
        file.close();
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
#ifndef QT_NO_CONCURRENT

        file.close();
        connect(&watcher, SIGNAL(finished()), this, SLOT(rescaleImageFinised()));

        array = reply->readAll();
        QFuture<void> conc = QtConcurrent::run(this, &DownloadScaledImageJob::rescaleImage, array, file.fileName());
        watcher.setFuture(conc);
#else
        QImage img;
        img.loadFromData(reply->readAll());
        img = img.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(&file);

        file.close();

        isCompleted = true;

        QFile completedFile(file.fileName() + ".completed");
        completedFile.open(QIODevice::WriteOnly);
        completedFile.close();

        emit completed();
#endif
    }
}

void DownloadScaledImageJob::rescaleImageFinised()
{
    array.clear();
    isCompleted = true;
    emit completed();

}


void DownloadScaledImageJob::rescaleImage(QByteArray array, QString filename)
{
    QImage img;
    img.loadFromData(array);
    img = img.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img.save(filename);


    QFile completedFile(filename + ".completed");
    completedFile.open(QIODevice::WriteOnly);
    completedFile.close();
}
