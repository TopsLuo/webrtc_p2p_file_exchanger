#ifndef TCP_CHAT_CLIENT_LIB_H
#define TCP_CHAT_CLIENT_LIB_H

#include "tcp_chat_client_lib_global.h"
#include "client.h"
#include "message.h"
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

typedef std::function<void(const char* text)> callback__;

class TCP_CHAT_CLIENT_LIBSHARED_EXPORT Tcp_chat_client_lib: public QObject
{
    Q_OBJECT
public:
    Tcp_chat_client_lib(callback__ callbackToLinker_ ,  QObject *parent = 0);
    ~Tcp_chat_client_lib();

public:
    //public interface methods
    bool connectToServer(const char* address, int port);
    bool disconnectFromServer();
    bool authentificate(const char* username);
    bool isOnline(); // check whether user is online
    bool isAuthorized(); //check whether user is authorized
    QString getCurrentUsername();
    void setCurrentUser(QString username);
    void getOnlineUsers(); // request list of online users
    void getAvailableFiles(); // request available files
    void postFilesList(const std::vector<std::string>& files); // post user files list to server
    bool openChatWith(const char* partner); // open chat with a given partner
    void closeCurrentChat();
    bool isChatOpen();
    const char* sendMessage (const char* message); // send message to the partner
    void sendMessageToPeer(const std::string& PeerId, long id, const std::string& type, const std::string& message);

private:
    std::unique_ptr <Client> currentClient_; // connection socket
    QThread* listener_; // thread for connection handling

    callback__ callbackToLinker_;

    QString username_; //current username
    QString partner_; // current partner in chat
    QMutex dataMutex_;

signals:
    void sendMessageSignal(const QJsonObject& msg); // send message command
    void disconnectSignal(); // disconnect from host command

public slots:
    void onListenerFinished();

};

#endif // TCP_CHAT_CLIENT_LIB_H
