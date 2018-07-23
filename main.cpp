#include "sources/linker.h"
#include <QCoreApplication>
#include <QSettings>
#include <string>
#include <csignal>
#include <QTimer>

void setInterruptHandlers();
void quit(int num);
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    setInterruptHandlers();

    QSettings settings(QString("config.ini"), QSettings::IniFormat);
    QString address = settings.value("Host/ip", QString("127.0.0.1")).toString();
    quint32 portNumber = settings.value("Host/port", 2225).toInt();

    Linker app(&quit);
    if(!app.connectToServer(address.toStdString(), portNumber, 0)){
        quit(0);
    }
    return a.exec();
}

void setInterruptHandlers()
{
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGABRT, quit);
    signal(SIGSTOP, quit);
}

void quit(int num)
{
    QTimer::singleShot(0, qApp, SLOT(quit()));
}
