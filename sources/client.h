#ifndef CLIENT_H
#define CLIENT_H
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QDebug>
#include <QVector>
#include <QByteArray>
#include <QDataStream>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QSharedPointer>
#include <QObject>
#include <QThread>
#include <QObject>
#include "message.h"

class Client : public QTcpSocket
{
    Q_OBJECT
public:
    explicit Client( QObject *parent = 0);
public:

private:
    quint16 m_nNextBlockSize; // size of arrived packet

private:
    void notifySender(const Message& msg); // notify sender that message has been successfully received
    void pullMessage(const Message& msg); // pull message to the client using newMessageSignal

signals:
    void handleMessage(const QJsonObject& message); // signal to handle received message
    void sendMessage (const QJsonObject& msg); // send message command
    void newMessageSignal(const char* text); //signals when new message received

public slots:
    void onhandleMessage(const QJsonObject& message); //message handler
    void onSendMessage (const QJsonObject& msg); //message sender
    void onReadyRead (); //ready to read handler
    void onDisconnected (); //disconnected handler
    void onDisconnectCommand(); // disconnect command handler
};

#endif // CLIENT_H
