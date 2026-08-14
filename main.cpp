#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序编码为 UTF-8
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    MainWindow w;
    w.show();
    return a.exec();
}
