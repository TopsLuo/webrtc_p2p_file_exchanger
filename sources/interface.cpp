#include "interface.h"

Interface::Interface(callback_ callbackToLinker):callbackToLinker_(callbackToLinker){
}

void Interface::authentificate()
{
    std::string username;
    std::cout << "input username: ";
    std::cin >> username;
    setCurrentState(REQUESTED);
    callbackToLinker_("AUTH", username, "");
    if(waitForReply(3))
    {
        std::cout << "you are registered as [" << getUsername() << "]" << std::endl;
        startInteractiveUser();
    }
    else
    {
        std::cout << "registration failed! try again." << std::endl;
        authentificate();
    }
}

void Interface::startInteractiveUser()
{
    std::string command;
    std::string partner;
    do
       {
           std::cout << "1. To start chatting use <chat> command.\n2. To start downloading files use <get> command\n3. To see download progress use <prog> command\n4. To quit the application use <.quit> command." << std::endl;
           std::cin>>command;
           if(command == "chat")
           {
                setCurrentState(CHATTING);

                bool success = false;
                setCurrentState(REQUESTED);
                callbackToLinker_("GET", "USERS", "");
                if(waitForReply(5))
                    chat();
                else{
                    std::cout << "unable to get online users" << std::endl;
                }

           }

           else if(command == "get")
           {
               std::cout << "1. To download file use <'peername'> <'index'> command" << std::endl;
               setCurrentState(REQUESTED);
               callbackToLinker_("GET", "FILES", "");
               if(!waitForReply(3))
               {
                   std::cout << "unable to get file lists" << std::endl;
                   continue;
               }

               downloadFile();
           }

           else if(command == "prog")
           {
               callbackToLinker_("GET", "PROG", "");
           }

           else if(command == ".quit")
           {
               callbackToLinker_("QUIT", "", "");
               break;
           }

           else if(command == "clear"){
               std::system("clear");
           }
           else
               std::cout << "unknown command" << std::endl;
       } while(true);

}

void Interface::chat()
{
    std::string partner;
    std::cout << "chat with: ";
    std::cin >> partner;
    if(partner == getUsername()){
        std::cout << "cannot chat with yourself!" << std::endl;
       return;
    }
    setCurrentState(REQUESTED);
    callbackToLinker_("CHAT", partner, "");

    std::cout << "chatting with [" << getPartner() << "]. To close the chat use <.close> command" << std::endl;
    std::string msg;
    std::cin.clear();
    for(;;){
        std::getline(std::cin, msg);
        if(msg == "" || msg =="\n")
            continue;
        else if(msg == ".close")
            break;
        else if(msg == ".quit")
            callbackToLinker_("QUIT", "", "");

        else
        {
            if(msg.size()>100)
                std::cout << "message length exceeded limit !" << std::endl;
            else
                callbackToLinker_("SEND", msg, getPartner());
        }
    }
}

void Interface::downloadFile()
{
    std::string owner;
    std::string index;
    std::cin >> owner >> index;

    if(owner == getUsername())
    {
        std::cout << "cannot download from yourself!" << std::endl;
        return;
    }
    callbackToLinker_("DOWNLOAD", index, owner);
}

std::string Interface::getUsername()
{
    std::lock_guard<std::mutex> lock(infoLock_);
    return this->username_;
}

std::string Interface::getPartner()
{
    std::lock_guard<std::mutex> lock(infoLock_);
    return this->partner_;
}

void Interface::setUsername(const std::string& username)
{
    std::lock_guard<std::mutex> lock(infoLock_);
    this->username_ = username;
}

void Interface::setPartner(const std::string &partner)
{
    std::lock_guard<std::mutex> lock(infoLock_);
    this->partner_ = partner;
}

void Interface::setCurrentState(STATE state)
{
    std::lock_guard<std::mutex> lock(stateLock_);
    this->state_ = state;
}

Interface::STATE Interface::getCurrentState()
{
    std::lock_guard<std::mutex> lock(stateLock_);
    return this->state_;
}

bool Interface::stateIsEqual(STATE state)
{
    std::lock_guard<std::mutex> lock(stateLock_);
    return this->state_ == state;
}

bool Interface::waitForReply(int seconds)
{
    std::unique_lock<std::mutex> lock(waitLock_);
    success_ = false;
    setCurrentState(WAITING_FOR_SERVER);
    waitCondition_.notify_one();
    bool isNotExpired = (waitCondition_.wait_for(lock, std::chrono::seconds(seconds)) == std::cv_status::no_timeout);
    setCurrentState(IDLE);
    if(isNotExpired)
        return this->success_;
    else
        false;
}

void Interface::wakeUp(bool success)
{
    this->success_ = success;
    if(waitLock_.try_lock()){
        if(stateIsEqual(REQUESTED)){
            std::unique_lock<std::mutex> lock(wakeUpLock_);
            waitLock_.unlock();
            waitCondition_.wait(lock);
            waitCondition_.notify_one();
        }
        else if(stateIsEqual(WAITING_FOR_SERVER)){
            waitLock_.unlock();
            waitCondition_.notify_one();
        }
    }
    else {
        waitLock_.unlock();
        waitCondition_.notify_one();
    }


}
