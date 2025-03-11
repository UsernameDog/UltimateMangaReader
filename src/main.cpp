#include <QApplication>
#include "manga_details_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Główne okno z widokiem szczegółów mangi
    MangaDetailsWidget w;
    w.show();

    return app.exec();
}
