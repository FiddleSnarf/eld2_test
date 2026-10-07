#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    //app.setWindowIcon(QIcon(":/root/App.ico"));

    QMainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
