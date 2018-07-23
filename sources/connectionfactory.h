#ifndef CONNECTIONFACTORY_H
#define CONNECTIONFACTORY_H

#include "connection.h"
class ConnectionFactory {
    class CustomRunnable : public rtc::Runnable {
    public:
    CustomRunnable(ConnectionFactory *parent):parent(parent){}
    void Run(rtc::Thread* subthread) override;
    private:
        ConnectionFactory * parent;
    };

    public:
    ConnectionFactory(callback callbackToLinker,  const std::string& user, const std::string& sharedPath, const std::string& downloadPath);
    ~ConnectionFactory();

    //////////////////////// INTERFACE
    public:
        bool isPeerConnected(const std::string& PeerId); //checks whether given peer is connected
        bool chatWith(const std::string& PeerId); //initiates chat with a given peer
        bool sendMessageToPeer(const std::string &PeerId, const std::string &message); //sends text message to a given peer if datachannel is open
        void signallingMessageHandler (const std::string& PeerId, const std::string& type, const std::string& message); //handles signalling messages
        void sendFileToPeer(const std::string& PeerId, const std::string& filename); //sends given file to requested peer
        void showDonwloadProgress(); //prints download progress
        void closeAllConnections(); //closes all peer connections releasing all objects
    ///////////////////////////////

    private:
        void initialize(); //initilizes peerconnectionfactory
        bool createPeerConnection (const std::string& PeerId); //creates webrtc peer connection with a given peer
        void removePeerConnection (const std::string& PeerId); //removes specific peer connection from factory
        bool peerExists(const std::string& PeerId); //checks whether specific peer exists
        Connection* getPeer(const std::string& PeerId);

    private:
        callback callbackToLinker_;
        std::string user_;
        std::string shared_path_;
        std::string download_path_;
        std::mutex peers_map_lock_;

        std::map <std::string,  Connection* > peers_; // map for storing active peer connections
        std::unique_ptr<rtc::Thread> thread_;
        rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
        webrtc::PeerConnectionInterface::RTCConfiguration configuration_;
        rtc::PhysicalSocketServer socket_server_;
        std::unique_ptr <CustomRunnable> runnable_;
        std::unique_ptr <Connection::Network> network_;
};
#endif
