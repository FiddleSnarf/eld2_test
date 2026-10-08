#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    //app.setWindowIcon(QIcon(":/root/App.ico"));

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
