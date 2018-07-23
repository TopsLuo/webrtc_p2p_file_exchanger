#include "connectionfactory.h"

ConnectionFactory::ConnectionFactory(callback callbackToLinker, const std::string &user, const std::string &sharedPath, const std::string &downloadPath) :
    callbackToLinker_(callbackToLinker), user_(user),
    shared_path_(sharedPath), download_path_(downloadPath){
    DIR* dp;
    if((dp = opendir(download_path_.c_str())) == NULL || (dp = opendir(shared_path_.c_str()))==NULL)
        std::cout << "non existing path" << std::endl;
    initialize();
};

ConnectionFactory::~ConnectionFactory()
{
    closeAllConnections();
    network_->stopNetworkThreads();
    peer_connection_factory_ = nullptr;
    thread_->Quit();
    thread_.reset();
    rtc::CleanupSSL();
}

void ConnectionFactory::initialize()
{
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    configuration_.servers.push_back(ice_server);
    thread_.reset(new rtc::Thread(&socket_server_));
    rtc::InitializeSSL();
    runnable_.reset(new CustomRunnable(this));
    thread_->Start(runnable_.get());
    network_.reset(new Connection::Network(2, 2));
    network_->startNetworkThreads();
}

bool ConnectionFactory::createPeerConnection(const std::string& PeerId)
{
    if(!peerExists(PeerId))
    {
        Connection* newConnection = new Connection();

        if(peer_connection_factory_.get() == nullptr)
        {
            std::cout << "empty factory" << std::endl;
            return false;
        }
        newConnection->peer_connection = peer_connection_factory_->CreatePeerConnection(configuration_, nullptr, nullptr, &(newConnection->pco));
        if(newConnection->peer_connection.get() == nullptr)
        {
            std::cout << "empty peer connection" << std::endl;
            return false;
        }
        newConnection->setProperties(PeerId, this->user_, this->callbackToLinker_,  shared_path_, download_path_, this->network_.get());
        std::lock_guard <std::mutex> lock(peers_map_lock_);
        peers_.insert(std::pair<std::string, Connection*>(PeerId, newConnection));
        return true;
    }

    if(!isPeerConnected(PeerId))
    {
        removePeerConnection(PeerId);
        return createPeerConnection(PeerId);
    }
    else
        return true;

}

void ConnectionFactory::signallingMessageHandler(const std::string& PeerId, const std::string& type, const std::string& message)
{
    if(!peerExists(PeerId)){
        if(type == "offer"){
            if(createPeerConnection(PeerId)){
                Connection* receiver = getPeer(PeerId);
                std::lock_guard <std::mutex> lock(peers_map_lock_);
                if(receiver != 0)
                    receiver->acceptSDP(message, type);
            }
        }
        return;
    }

    else if(!isPeerConnected(PeerId)){
        if(type == "offer"){
            if(createPeerConnection(PeerId)){
                Connection* receiver = getPeer(PeerId);
                std::lock_guard <std::mutex> lock(peers_map_lock_);
                if(receiver != 0)
                    receiver->acceptSDP(message, type);
            }
            return;
        }

        else if(type == "answer") {
            Connection* receiver = getPeer(PeerId);
            std::lock_guard <std::mutex> lock(peers_map_lock_);
            if(receiver != 0)
                receiver->acceptSDP(message, type);
            return;
        }
    }

    if(type == "ice"){
        Connection* receiver = getPeer(PeerId);
        std::lock_guard <std::mutex> lock(peers_map_lock_);
        if(receiver != 0)
            receiver->acceptIceCandidate(message);
    }
    
}


bool ConnectionFactory::chatWith(const std::string& PeerId){
    if(createPeerConnection(PeerId)){
        Connection* peer = getPeer(PeerId);
        std::lock_guard <std::mutex> lock(peers_map_lock_);
        if(peer != 0)
            return peer->openMessageChannel();
        return false;
    }
    return false;
}

void ConnectionFactory::sendFileToPeer(const std::string& PeerId, const std::string& filename)
{
    if(createPeerConnection(PeerId)){
        Connection* peer = getPeer(PeerId);
        std::lock_guard <std::mutex> lock(peers_map_lock_);
        if(peer != 0)
            peer->sendFile(filename);
    }
}

bool ConnectionFactory::sendMessageToPeer(const std::string& PeerId, const std::string& message)
{
    if(!isPeerConnected(PeerId))
        return false;
    Connection* peer = getPeer(PeerId);
    std::lock_guard <std::mutex> lock(peers_map_lock_);
    if(peer != 0)
        peer->sendTextMessage(message);
    return true;
}



bool ConnectionFactory::peerExists(const std::string &PeerId)
{
    std::lock_guard <std::mutex> lock(peers_map_lock_);
    auto peer = peers_.find(PeerId);
    if(peer == peers_.end())
       return false;
    return true;
}

bool ConnectionFactory::isPeerConnected(const std::string &PeerId)
{
    if(!peerExists(PeerId))
        return false;
    Connection* peer = getPeer(PeerId);
    if(peer != 0)
        return peer->isConnected();
}

Connection* ConnectionFactory::getPeer(const std::string &PeerId){
    std::lock_guard <std::mutex> lock(peers_map_lock_);
    auto peer = peers_.find(PeerId);
    if(peer == peers_.end())
        return 0;
    return peer->second;
}

void ConnectionFactory::removePeerConnection(const std::string& PeerId)
{ 
    std::lock_guard <std::mutex> lock(peers_map_lock_);
    auto peer = peers_.find(PeerId);
    if(peer == peers_.end())
        return;
    peer->second->closeAllChannels();
    peer->second->peer_connection->Close();
    delete(peer->second);
    peer->second = 0;
    peers_.erase(PeerId);
}

void ConnectionFactory::showDonwloadProgress()
{
    std::cout << "--------------------------------------------------" << std::endl;
    std::lock_guard <std::mutex> lock(peers_map_lock_);
    for(auto it = peers_.begin(); it != peers_.end(); it++){
       it->second->printProgress();
    }
    std::cout << "--------------------------------------------------" << std::endl;
}

void ConnectionFactory::closeAllConnections()
{
    for(auto it = peers_.begin(); it != peers_.end(); it++){
        removePeerConnection(it->first);
    }
}

void ConnectionFactory::CustomRunnable::Run(rtc::Thread* subthread){
     parent->peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
                                webrtc::CreateBuiltinAudioEncoderFactory(),
                                webrtc::CreateBuiltinAudioDecoderFactory());
        
     if (parent->peer_connection_factory_.get() == nullptr) {
        std::cout << "Error on CreatePeerConnectionFactory." << std::endl;
        return;
     }
     subthread->Run();
}

