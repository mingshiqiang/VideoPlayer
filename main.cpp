#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("VideoPlayer");
    a.setOrganizationName("VideoPlayer");

    // Application / taskbar icon (loaded from compiled SVG resource).
    a.setWindowIcon(QIcon(QStringLiteral(":/icons/resources/logo.svg")));

    // Set default font
    QFont font("Microsoft YaHei UI", 9);
    a.setFont(font);

    MainWindow w;
    w.show();
    return a.exec();
}
