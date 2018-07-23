#include "linker.h"
#include <QDateTime>
#include <functional>
#include <memory>
#include <string>

Linker::Linker(callbackToMain callBackQuit): callBackQuit(callBackQuit)
{
    interface_.reset(new Interface(std::bind(&Linker::callBackFromUserInterface, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
    client_.reset(new Tcp_chat_client_lib (std::bind(&Linker::callBackFromSignallingServer, this, std::placeholders::_1)));
}

bool Linker::connectToServer(const std::string& address, int port, int attempts)
{
     if(client_->connectToServer(address.c_str(), port)){
         this->address_ = address;
         this->port_ = port;
         if(client_->isAuthorized() == false)
            interface_->authentificate();
         else{
             client_->authentificate(client_->getCurrentUsername().toStdString().c_str());
         }
     }
     else{
         if(attempts > 2){
             std::cout <<"unable to connect to server! check settings and relaunch the app" << std::endl;
             return false;
         }
         int sleepTime(5*(attempts + 1));
         std::cout << "connection timout! trying to reconnect in "<< sleepTime << " seconds" << std::endl;
         sleep(sleepTime);
         return connectToServer(address, port, ++attempts);
     }

}


void Linker::callBackFromUserInterface(const std::string &command, const std::string &message, const std::string &receiver)
{
    if(command == "AUTH") {
        client_->authentificate(message.c_str());
    }
    else if(command == "GET"){
        if(message == "USERS"){
            client_->getOnlineUsers();
        }
        else if(message == "FILES"){
            postLocalFilesToServer();
            client_->getAvailableFiles();
        }
        else if(message == "PROG"){
            factory_->showDonwloadProgress();
        }
    }
    else if(command == "DOWNLOAD"){
        bool flag = false;
        int ind = QString(message.c_str()).toInt(&flag);
        if(!flag)
        {
            std::cout << "wrong file selected!" << std::endl;
            return;
        }
        if(files_.contains(QString(receiver.c_str())))
        {
            QList<QString> list = files_[QString(receiver.c_str())];
            if(ind > -1 && ind < list.size())
            {
                 long id = QDateTime::currentMSecsSinceEpoch();
                 std::string filename = list.at(ind).toStdString();
                 client_->sendMessageToPeer(receiver, id, "request", filename);
            }
        }
    }

    else if(command == "CHAT")
    {
        if(factory_->chatWith(message))
            interface_->setPartner(message);
    }

    else if(command == "SEND")
    {
        if(factory_->isPeerConnected(receiver))
            factory_->sendMessageToPeer(receiver, message);
        else{
            std::cout << "failed! [" << receiver << "] is offline." << std::endl;
        }
    }

    else if(command == "QUIT"){
        callBackQuit(0);
    }
}

void Linker::callBackFromSignallingServer(const char *text)
{
    QJsonDocument doc = QJsonDocument::fromJson(QString(text).toUtf8());
    QJsonObject json = doc.object();
    QString type = json["type"].toString();
    QString sender = json["sender"].toString();
    QString receiver = json["receiver"].toString();
    QString id = json["messageID"].toString();

    if(type == "O_USERS")
    {
        QJsonArray array = json["message"].toArray();

        std::cout << "online users: ";
        for (int i=0; i<array.size(); i++)
        {
            if(array.at(i) != client_->getCurrentUsername())
                std::cout << array.at(i).toString().toStdString() << " | ";
        }
        std::cout << std::endl;

        interface_->wakeUp(true);
    }

    else if(type == "O_FILES")
    {
        //std::cout << text << std::endl;
        QJsonArray array = json["message"].toArray();
        files_.clear();
        for (int i=0; i<array.size(); i++)
        {
            QJsonDocument docs = QJsonDocument::fromJson(array.at(i).toString().toUtf8());
            QJsonObject obj = docs.object();
            QString name = obj["name"].toString();
            QJsonArray files = obj["files"].toArray();
            for(int j = 0; j<files.size(); j++)
            {
                files_[name].append(files.at(j).toString());
            }
        }
        printFileList();
        interface_->wakeUp(true);
        return;
    }

    else if(type == "REQUEST")
    {
        QString message = json["message"].isArray() ? json["message"].toArray().at(0).toString() : QString("");
        std::string requestedFile = message.toStdString();

        std::string dir = std::string("files");
        std::vector<std::string> files = Connection::getLocalFiles("files");
        bool found = false;
        for (unsigned int i = 0; i < files.size(); i++)
        {
            if(files.at(i) == requestedFile)
            {
                found = true;
            }
        }

        if(found)
        {
            factory_->sendFileToPeer(sender.toStdString(), requestedFile);
        }
    }

    else if(type == "S_AUTH")
    {
        QString message = json["message"].isString() ? json["message"].toString() : QString("");
        if(message == "OK")
        {
            if(interface_->getUsername()!=receiver.toStdString()){
                factory_.reset(new ConnectionFactory(std::bind(&Linker::callBackFromPCFactory, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), receiver.toStdString(), "files", "downloads"));
                interface_->setUsername(receiver.toStdString());
                client_->setCurrentUser(receiver);
                postLocalFilesToServer();
                interface_->wakeUp(true);
            }
            else{
                std::cout << "connection has been recovered. you are online again!" << std::endl;
            }
        }
        else
            interface_->wakeUp(false);
    }



    else if(type == "OFFER")
    {
        QString msg = json["message"].isArray() ? json["message"].toArray().at(0).toString() : QString("");

        std::string PeerId = sender.toStdString();
        std::string type = "offer";
        std::string message = msg.toStdString();
        long Id = id.toLong();
        factory_->signallingMessageHandler(PeerId, type, message);
    }

    else if(type == "ANSWER")
    {

        QString msg = json["message"].isArray() ? json["message"].toArray().at(0).toString() : QString("");

        std::string PeerId = sender.toStdString();
        std::string type = "answer";
        std::string message = msg.toStdString();
        long Id = id.toLong();
        factory_->signallingMessageHandler(PeerId, type, message);
    }

    else if(type == "ICE")
    {
        QString msg = json["message"].isArray() ? json["message"].toArray().at(0).toString() : QString("");

        std::string PeerId = sender.toStdString();
        std::string type = "ice";
        std::string message = msg.toStdString();
        long Id = id.toLong();
        factory_->signallingMessageHandler(PeerId, type, message);
    }

    else if(type == "DISCONNECTED")
    {
        std::cout << "you are disconnected from server! trying to reconnect ..." <<std::endl;
        connectToServer(address_, port_, 0);
    }
}

void Linker::callBackFromPCFactory(const std::string &PeerId, const std::string &type, const std::string &message)
{
    if(type=="update")
    {
        postLocalFilesToServer();
    }
    else
        client_->sendMessageToPeer(PeerId, 0, type, message);
}


void Linker::postLocalFilesToServer()
{
    std::vector <std::string> files = Connection::getLocalFiles("files");
    client_->postFilesList(files);
}

void Linker::printFileList()
{
    std::cout << "available files: " << std::endl;
    for (QString name : files_.keys())
    {
        if(name.toStdString() == interface_->getUsername())
            continue;
        std::cout << "<" + name.toStdString() + ">" << std::endl;

        for(int i = 0; i < files_[name].size(); i++)
        {
            std::cout << "  " << "[" << i << + "] " << files_[name].at(i).toStdString() << std::endl;
        }
    }
}
