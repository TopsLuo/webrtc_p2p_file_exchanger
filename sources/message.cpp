#include "message.h"

Message::Message(QJsonObject json, QObject *parent):QObject(parent)
{
    parseFromJson(json);
    parsedMessage_ = json;
}

Message::Message(Commands cmd, Types type, QString sender, QString receiver, QList<QString> message, QObject *parent): cmd_(cmd), type_(type), sender_(sender), receiver_(receiver), message_(message), messageID_(0), QObject(parent)
{
    parseToJson();
}

void Message::parseToJson()
{
    parsedMessage_["command"] = this->cmd_;
    parsedMessage_["type"] = this->type_;
    parsedMessage_["sender"] = this->sender_;
    parsedMessage_["receiver"] = this->receiver_;
    parsedMessage_["messageID"] = this->messageID_;
    QJsonArray array;
    for(QString str : message_)
    {
       array.append(str);
    }

    parsedMessage_["message"] = array;
}

void Message::parseFromJson(QJsonObject json)
{
    this->cmd_ = static_cast <Message::Commands> (json["command"].toInt());
    this->type_ = static_cast <Message::Types> (json["type"].toInt());
    this->sender_ = json["sender"].toString();
    this->receiver_ = json["receiver"].toString();
    this->messageID_ = json["messageID"].toDouble();
    QList<QString> text;
    QJsonArray array = json["message"].toArray();
    for(int i = 0; i < array.size(); i++)
    {
       text.push_back(array.at(i).toString());
    }
    this->message_ = text;
}



QJsonObject Message::getJsonMessage() const
{
    return parsedMessage_;
}

QString Message::getSender() const
{
    return this->sender_;
}

QString Message::getReceiver() const
{
    return this->receiver_;
}

QList<QString> Message::getMessage() const
{
    return this->message_;
}

Message::Commands Message::getCommmand() const
{
    return this->cmd_;
}

Message::Types Message::getType() const
{
    return this->type_;
}

void Message::setMessageID(qint64 id)
{
    this->messageID_ = id;
    parsedMessage_["messageID"] = this->messageID_;
}

qint64 Message::getMessageID() const
{
    return this->messageID_;
}
