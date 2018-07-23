#ifndef LINKER_H
#define LINKER_H
#include "connectionfactory.h"
#include "tcp_chat_client_lib.h"
#include "interface.h"

/*Linker class initializes and manages webrtc and signalling connections together with user interface.*/
typedef void(* callbackToMain)(int num);
class Linker{
public:
    Linker(callbackToMain callBackQuit);
    bool connectToServer(const std::string& address, int port, int attempts); // connect to signalling server

private:
    // callbacks from webrtc, signalling server and user interface
    //////////////////////////////////////////////////////////////
    void callBackFromPCFactory(const std::string& PeerId, const std::string& type, const std::string& message);
    void callBackFromSignallingServer(const char* text);
    void callBackFromUserInterface(const std::string& command, const std::string& message, const std::string &receiver);
    callbackToMain callBackQuit;


    //////////////////////////////////////////////////////////////
private:

    void postLocalFilesToServer(); // posts local files list to server
    void printFileList(); //prints list of files available to download

private:
    std::unique_ptr <Tcp_chat_client_lib> client_ = 0; //interface to signalling server
    std::unique_ptr <ConnectionFactory> factory_ = 0; //interface to webrtc connections
    std::unique_ptr <Interface> interface_ = 0; //user interface

    QHash <QString, QList<QString>> files_; //stores list of files posted on server

    //signalling server address
    std::string address_;
    int port_;
};

#endif
