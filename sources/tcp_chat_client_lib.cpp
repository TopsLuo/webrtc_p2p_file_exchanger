#include "tcp_chat_client_lib.h"
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QThreadPool>
#include <QMetaObject>
#include <QGenericArgument>
#include <iostream>
#include <vector>
#include <string>


Tcp_chat_client_lib::Tcp_chat_client_lib(callback__ callbackToLinker_, QObject *parent) : QObject(parent), callbackToLinker_(callbackToLinker_)
{
    getCurrentUsername().clear();
    this->partner_.clear();

    currentClient_.reset(new Client());
    listener_ = new QThread();

    connect(currentClient_.get(), SIGNAL(readyRead()), currentClient_.get(), SLOT(onReadyRead()), Qt::QueuedConnection);
    connect(this, SIGNAL(sendMessageSignal(QJsonObject)), currentClient_.get(), SLOT(onSendMessage(QJsonObject)), Qt::QueuedConnection);
    connect(this, SIGNAL(disconnectSignal()), currentClient_.get(), SLOT(onDisconnectCommand()), Qt::QueuedConnection);
    connect(currentClient_.get(), SIGNAL(disconnected()), currentClient_.get(), SLOT(onDisconnected()), Qt::QueuedConnection);
    connect(listener_, SIGNAL(finished()), this, SLOT(onListenerFinished()) );
    connect(currentClient_.get(), &Client::newMessageSignal, this->callbackToLinker_);

    listener_->start();
}

Tcp_chat_client_lib::~Tcp_chat_client_lib(){
    listener_->quit();
    listener_->wait();
}

bool Tcp_chat_client_lib::connectToServer(const char *address, int port)
{
    QString haddress(address);
    currentClient_->connectToHost(haddress, port);
    if(currentClient_->waitForConnected(5000))
    {
        currentClient_->moveToThread(listener_);
        return true;
    }
    return false;
}

bool Tcp_chat_client_lib::authentificate(const char *username)
{
    if(!this->isOnline())
        return false;
    Message msg(Message::Commands::AUTH, Message::Types::MESSAGE, QString::fromUtf8(username) , QString("server"));
    emit sendMessageSignal(msg.getJsonMessage());
    return this->isAuthorized();
}

bool Tcp_chat_client_lib::openChatWith(const char *partner)
{
    if(!this->isAuthorized() || !this->isOnline())
        return false;
    QMutexLocker locker(&dataMutex_);
    this->partner_ = QString::fromUtf8(partner);
    return true;
}

void Tcp_chat_client_lib::setCurrentUser(QString username){
    QMutexLocker locker(&dataMutex_);
    this->username_ = username;
}

QString Tcp_chat_client_lib::getCurrentUsername(){
    QMutexLocker locker(&dataMutex_);
    return this->username_;
}

void Tcp_chat_client_lib::closeCurrentChat()
{
    QMutexLocker locker(&dataMutex_);
    this->partner_.clear();
}

bool Tcp_chat_client_lib::isChatOpen()
{
    QMutexLocker locker(&dataMutex_);
    return !this->partner_.isEmpty();
}

bool Tcp_chat_client_lib::isAuthorized()
{
    QMutexLocker locker(&dataMutex_);
    return !this->username_.isEmpty();
}

const char* Tcp_chat_client_lib::sendMessage(const char *message)
{
    if(!this->isAuthorized() || !this->isOnline() || !this->isChatOpen())
        return NULL;
    QList<QString> text;
    text.push_front(QString(message));
    Message msg(Message::Commands::SEND, Message::Types::MESSAGE, getCurrentUsername(), this->partner_, text);
    msg.setMessageID(QDateTime::currentMSecsSinceEpoch());
    emit sendMessageSignal(msg.getJsonMessage());
    return QString::number(msg.getMessageID()).toStdString().c_str();
}

void Tcp_chat_client_lib::sendMessageToPeer(const std::string &PeerId, long id, const std::string &type, const std::string &message)
{
    if(!this->isAuthorized() || !this->isOnline())
        return;
    QList<QString> text;
    text.push_front(QString(message.c_str()));
    Message::Types msgType;
    if(type == "ice")
        msgType = Message::Types::ICE;
    else if(type == "offer")
        msgType = Message::Types::OFFER;
    else if(type == "answer")
        msgType = Message::Types::ANSWER;
    else if(type == "request")
        msgType = Message::Types::REQUEST;
    else
    {
        std::cout << "unknown message type !" << std::endl;
        return;
    }

    Message msg(Message::Commands::SEND, msgType, getCurrentUsername(), QString(PeerId.c_str()), text);
    msg.setMessageID(id);
    emit sendMessageSignal(msg.getJsonMessage());

}

void Tcp_chat_client_lib::getOnlineUsers()
{
    Message msg(Message::Commands::GET, Message::Types::USERS, getCurrentUsername() , QString("server"));
    emit sendMessageSignal(msg.getJsonMessage());
}

void Tcp_chat_client_lib::getAvailableFiles()
{
    Message msg(Message::Commands::GET, Message::Types::FILES, getCurrentUsername() , QString("server"));
    emit sendMessageSignal(msg.getJsonMessage());
}

void Tcp_chat_client_lib::postFilesList(const std::vector<std::string> &files)
{
    QList<QString> list;
    for(int i=0; i<files.size(); i++)
    {
        if(files.at(i).c_str()[0] != '.')
        list.append(QString(files.at(i).c_str()));
    }
    Message msg(Message::Commands::POST, Message::Types::FILES, getCurrentUsername() , QString("server"), list);
    emit sendMessageSignal(msg.getJsonMessage());
}

bool Tcp_chat_client_lib::isOnline()
{
    QMutexLocker locker(&dataMutex_);
    return currentClient_->isOpen();
}

bool Tcp_chat_client_lib::disconnectFromServer()
{
    emit disconnectSignal();
}

void Tcp_chat_client_lib::onListenerFinished()
{
    currentClient_->close();
}
