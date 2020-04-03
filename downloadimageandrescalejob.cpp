#include "downloadimageandrescalejob.h"

#include <QImage>

DownloadScaledImageJob::DownloadScaledImageJob(
    QNetworkAccessManager *networkManager, const QString &url,
    const QString &path, QSize size)
    : DownloadFileJob(networkManager, url, path), size(size)
{
}

void DownloadScaledImageJob::downloadFileReadyRead()
{
    // don't save to file because its gonna be rescaled anyway
    //    file.write(reply->readAll());
}

void DownloadScaledImageJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.close();
        file.remove();
    }

    QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid() && reply->url() != redirect)
    {
        this->url = redirect.toString();
        this->restart();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        if (rescaleImage(reply->readAll()))
        {
            isCompleted = true;
            emit completed();
        }
        else
        {
            errorString = "Failed to load or rescale image.";
            emit downloadError();
        }
    }
}

bool DownloadScaledImageJob::rescaleImage(const QByteArray &array)
{
    QImage img;
    if (!img.loadFromData(array))
        return false;

    auto rsize = size;
    if ((img.width() <= img.height()) != (size.width() <= size.height()))
        rsize.transpose();

    img = img.convertToFormat(QImage::Format_Grayscale8);
    img = img.scaled(rsize.width(), rsize.height(), Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
    if (!img.save(filepath))
        return QFileInfo(filepath).size() > 0;

    return true;
}
