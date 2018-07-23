#ifndef INTERFACE_H
#define INTERFACE_H
#include <functional>
#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <atomic>

// Implements command line user interface
typedef std::function<void(const std::string& , const std::string&, const std::string& )> callback_;
class Interface {
public:
    enum STATE {AUTHENTIFICATED, AUTHENTIFICATING, WAITING_FOR_SERVER, REQUESTED, CHATTING, IDLE};
public:
    Interface (callback_ callbackToLinker);
    void authentificate();
    void startInteractiveUser();
    void wakeUp(bool success); // wakes up interface waiting for reply (argument indicates whether request succeeded or not)


    std::string getUsername();
    void setUsername(const std::string &username);
    std::string getPartner();
    void setPartner(const std::string& partner);


    STATE getCurrentState();
    bool stateIsEqual(STATE state);

private:
    void setCurrentState(STATE state);
    bool waitForReply(int seconds); //waits for server reply
    void chat(); //starts chat interface
    void downloadFile(); //starts download interface

private:
    callback_ callbackToLinker_;

    STATE state_;
    std::mutex stateLock_;

    std::string username_;
    std::string partner_;
    std::mutex infoLock_;

    std::mutex waitLock_;
    std::mutex wakeUpLock_;
    std::condition_variable waitCondition_;

    std::atomic <bool> success_;
};

#endif
