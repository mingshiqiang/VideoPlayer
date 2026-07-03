#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("VideoPlayer");
    a.setOrganizationName("VideoPlayer");

    // Set default font
    QFont font("Microsoft YaHei UI", 9);
    a.setFont(font);

    MainWindow w;
    w.show();
    return a.exec();
}
