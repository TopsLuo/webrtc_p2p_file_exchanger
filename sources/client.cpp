#include "client.h"
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QThreadPool>
#include <QMetaObject>
#include <QGenericArgument>
#include <QList>
#include <string>


Client::Client(QObject *parent): QTcpSocket(parent)
{
    m_nNextBlockSize = 0;
    connect(this, SIGNAL(handleMessage(QJsonObject)), this, SLOT(onhandleMessage(QJsonObject)), Qt::QueuedConnection);
}

void Client::onReadyRead()
{
    QDataStream in(this);
    while(true){
        if(!m_nNextBlockSize){
            if(this->bytesAvailable()<sizeof(quint16)){
                break;
            }
            in >> m_nNextBlockSize;
        }
        if(this->bytesAvailable() < m_nNextBlockSize){
            break;
        }
        QByteArray bytes;
        in >> bytes;
        emit handleMessage(QJsonDocument::fromBinaryData(bytes).object());
        m_nNextBlockSize = 0;
    }
}

void Client::onhandleMessage(const QJsonObject& message)
{
    Message parsedMessage(message);
    pullMessage(parsedMessage);

}

void Client::onSendMessage(const QJsonObject &msg)
{
    QByteArray arrBlock;
    QDataStream out (&arrBlock, QIODevice::WriteOnly);
    QJsonDocument doc(msg);
    QByteArray bytes = doc.toBinaryData();
    out << quint16(bytes.size()) << bytes;
    this->write(arrBlock);
    emit readyRead();
}

void Client::onDisconnected()
{
    QJsonObject notification;
    notification["type"] = QString("DISCONNECTED");
    QJsonDocument doc(notification);
    QString text(doc.toJson(QJsonDocument::Compact));
    emit this->newMessageSignal(text.toStdString().c_str());
}

void Client::onDisconnectCommand()
{
    this->disconnectFromHost();
}

void Client::pullMessage(const Message &msg)
{
    QJsonObject notification;
    notification["sender"] = msg.getSender();
    notification["receiver"] = msg.getReceiver();
    notification["message"] = msg.getJsonMessage()["message"].toArray();
    notification["messageID"] = QString::number(msg.getMessageID());
    Message::Commands cmd = msg.getCommmand();
    Message::Types type = msg.getType();
    switch (cmd) {
    case Message::Commands::SEND:
        switch (type) {
        case Message::Types::MESSAGE:
            notification["type"] = QString("MESSAGE");
            break;
        case Message::Types::OFFER:
            notification["type"] = QString("OFFER");
            break;
        case Message::Types::ANSWER:
            notification["type"] = QString("ANSWER");
            break;
        case Message::Types::ICE:
            notification["type"] = QString("ICE");
            break;
        case Message::Types::REQUEST:
            notification["type"] = QString("REQUEST");
            break;

        default:
            break;
        }
        break;
    case Message::Commands::POST:
        switch (type) {
        case Message::Types::USERS:
            notification["type"] = QString("O_USERS");
            break;
        case Message::Types::FILES:
            notification["type"] = QString("O_FILES");
            break;
        default:
            break;
        }

        break;
    case Message::Commands::RESPONSE:
        notification["type"] = QString("S_DELIVERY");
        switch (type) {
        case Message::Types::DELIVERED:
            notification["message"] = QString("OK");
            break;

        case Message::Types::FAILURE:
            notification["message"] = QString("NO");
            break;
        default:
            break;
        }
        break;

    case Message::Commands::AUTH:
        notification["type"] = QString("S_AUTH");
        switch (type) {
        case Message::Types::SUCCESS:
            notification["message"] = QString("OK");
            break;
        case Message::Types::FAILURE:
            notification["message"] = QString("NO");
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    QJsonDocument doc(notification);
    QString text(doc.toJson(QJsonDocument::Compact));
    emit this->newMessageSignal(text.toStdString().c_str());
}

void Client::notifySender(const Message &msg)
{
    QList<QString> message;
    Message reply(Message::Commands::RESPONSE, Message::Types::DELIVERED, msg.getReceiver(), msg.getSender(), message);
    reply.setMessageID(msg.getMessageID());
    this->onSendMessage(reply.getJsonMessage());
}




