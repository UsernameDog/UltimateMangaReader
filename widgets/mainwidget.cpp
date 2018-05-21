#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QStringListModel>
#include <QScrollBar>
#include "configs.h"

#ifndef WINDOWS

#include "Config.h"
#include "QtUtils.h"
#include "Platform.h"
#include "FileOpenDialog.h"
#include "FileSaveDialog.h"
#include "WidgetCommon.h"
#include "VirtualKeyboard.h"
#include "VirtualKeyboardContainer.h"

#endif



MainWidget::MainWidget(QWidget *parent) :
#ifdef WINDOWS
    QWidget(parent),
#endif
    ui(new Ui::MainWidget),
    currentmanga(nullptr),
    readingstatemanager(),
    lastTab(0)
{
    ui->setupUi(this);
    setupDirs();

    readingstatemanager.deserialize();

    downloadmanager = new DownloadManager(this);


    mangasources.append(new MangaPanda(this, downloadmanager));
    mangasources.append(new MangaDex(this, downloadmanager));
    mangasources.append(new MangaTown(this, downloadmanager));


    currentsource = mangasources[0];

    ui->homeWidget->setMangaSources(&mangasources);

    downloadmanager->connect();

    foreach (AbstractMangaSource *ms, mangasources)
        ms->deserialize();


    setupUI();

    QObject::connect(ui->homeWidget, SIGNAL(mangaSourceClicked(AbstractMangaSource *)), this, SLOT(setCurrentSource(AbstractMangaSource *)));
    QObject::connect(ui->homeWidget, SIGNAL(mangaClicked(QString)), this, SLOT(viewMangaInfo(QString)));

    QObject::connect(ui->mangaInfoWidget, SIGNAL(toggleFavoriteClicked(MangaInfo *)), this, SLOT(toggleFavorite(MangaInfo *)));
    QObject::connect(ui->mangaInfoWidget, SIGNAL(readMangaClicked(MangaIndex)), this, SLOT(viewMangaImage(MangaIndex)));

    QObject::connect(ui->favoritesWidget, SIGNAL(favoriteClicked(ReadingState, bool)), this, SLOT(viewFavorite(ReadingState, bool)));

    QObject::connect(ui->mangaReaderWidget, SIGNAL(changeView(int)), this, SLOT(setWidgetTab(int)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(advancPageClicked(bool)), this, SLOT(advanceMangaPage(bool)));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(closeApp()), this, SLOT(on_pushButtonClose_clicked()));
    QObject::connect(ui->mangaReaderWidget, SIGNAL(back()), this, SLOT(readerGoBack()));

//    QObject::connect(currentmanga, SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));
}

MainWidget::~MainWidget()
{
    delete ui;
}

void  MainWidget::setupUI()
{
#ifndef WINDOWS
    initTopLevelWidget(this);

    VirtualKeyboard *vk = getVirtualKeyboard();


    ui->verticalLayout->insertWidget(1, vk);
#endif

    downloadmanager->setImageSize(this->width(), this->height());

    adjustSizes();
}

void  MainWidget::adjustSizes()
{
    ui->pushButtonClose->setMinimumHeight(buttonsize);
    ui->pushButtonFavorites->setMinimumHeight(buttonsize);
    ui->pushButtonHome->setMinimumHeight(buttonsize);
}

void MainWidget::setupDirs()
{
    if (!QDir(downloaddir).exists())
        QDir().mkdir(downloaddir);

    if (!QDir(cachedir).exists())
        QDir().mkdir(cachedir);

    if (!QDir(downloaddirimages).exists())
        QDir().mkdir(downloaddirimages);

    if (!QDir(downloaddircovers).exists())
        QDir().mkdir(downloaddircovers);

    if (!QDir(manglistcachdir).exists())
        QDir().mkdir(manglistcachdir);

    if (!QDir(readingstatesdir).exists())
        QDir().mkdir(readingstatesdir);
}



void MainWidget::on_pushButtonHome_clicked()
{
    setWidgetTab(0);
}

void MainWidget::on_pushButtonFavorites_clicked()
{
    setWidgetTab(2);
}

void MainWidget::on_pushButtonClose_clicked()
{
    close();
}

void MainWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    downloadmanager->setImageSize(this->width(), this->height());
}



void MainWidget::setWidgetTab(int page)
{
#ifndef WINDOWS
    getVirtualKeyboard()->hide();
#endif


    if (page == ui->stackedWidget->currentIndex())
        return;

    if (currentmanga != nullptr)
        currentmanga->CancelAllPreloads();

    if (page == 3)
    {
        ui->navigationBar->setVisible(false);
        ui->stackedWidget->setCurrentIndex(page);
    }
    else
    {
        if (page == 1)
        {
            lastTab = 1;
        }
        if (page == 2)
        {
            ui->favoritesWidget->updateList(readingstatemanager.getFavorites());
            lastTab = 2;
        }

        ui->navigationBar->setVisible(true);
        ui->stackedWidget->setCurrentIndex(page);
    }

}



void MainWidget::viewFavorite(ReadingState fav, bool current)
{
    foreach (AbstractMangaSource *source, mangasources)
        if (fav.hostname == source->name)
            currentsource = source;


    qDebug() << currentsource->name;
    qDebug() << fav.coverpath;
    qDebug() << fav.mangalink;

    if (current)
    {
        currentmanga = currentsource->getMangaInfo(fav.mangalink);
        viewMangaImage(fav.currentindex);
    }
    else
    {
        viewMangaInfo(fav.mangalink);
    }
}

void MainWidget::setCurrentSource(AbstractMangaSource *source)
{
    currentsource = source;
}

void MainWidget::viewMangaInfo(QString mangalink)
{
    currentmanga = currentsource->getMangaInfo(mangalink);
    QObject::connect(currentmanga, SIGNAL(completedImagePreloadSignal(QString)), ui->mangaReaderWidget, SLOT(addImageToCache(QString)));


    ReadingState *rs = readingstatemanager.findOrInsert(*currentmanga);
    currentmanga->currentindex = rs->currentindex;
    ui->mangaInfoWidget->setManga(currentmanga);
    ui->mangaInfoWidget->setFavoriteButtonState(!rs->isfavorite);

    setWidgetTab(1);

//    currentmanga->PreloadPopular();
}

void MainWidget::toggleFavorite(MangaInfo *manga)
{
    readingstatemanager.toggleFavorite(manga);
    ReadingState *rs = readingstatemanager.findOrInsert(*currentmanga);
    ui->mangaInfoWidget->setFavoriteButtonState(!rs->isfavorite);
}


void MainWidget::advanceMangaPage(bool direction)
{
    if (direction)
        viewMangaImage(currentmanga->currentindex.nextPageIndex(&currentmanga->chapters));
    else
        viewMangaImage(currentmanga->currentindex.prevPageIndex(&currentmanga->chapters));

}

void MainWidget::readerGoBack()
{
    setWidgetTab(lastTab);
}


void MainWidget::viewMangaImage(const MangaIndex &index)
{
    ui->mangaReaderWidget->showImage(currentmanga->goChapterPage(index));
    ui->mangaReaderWidget->updateReaderLabels(currentmanga);

    setWidgetTab(3);

    currentmanga->PreloadNeighbours();

    readingstatemanager.update(currentmanga);
}






























