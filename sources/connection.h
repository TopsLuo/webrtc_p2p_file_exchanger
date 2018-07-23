#ifndef CONNECTION_H
#define CONNECTION_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <map>
#include <iterator>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <thread>
#include <vector>
#include <dirent.h>
#include <sys/types.h>
#include <chrono>
#include <sys/time.h>
#include <functional>
#include <deque>
#include <atomic>


#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/base/flags.h>
#include <webrtc/base/physicalsocketserver.h>
#include <webrtc/base/ssladapter.h>
#include <webrtc/base/thread.h>
#include "sources/picojson.h"

#define CHUNK_SIZE (1024*10)

typedef std::function<void (const std::string& PeerId, const std::string& type, const std::string& message)> callback;

/*  Connection Class to handle peer connections. This class implements also the peer connection interface
    */
class Connection {

   public:
    class Network;
    class DCO;
    class PCO;
    class CSDO;
    class SSDO;
    ////////////////////////////////////////////////////////////
    class PCO : public webrtc::PeerConnectionObserver {
     private:
      Connection& parent;

     public:
      PCO(Connection& parent) : parent(parent) {
      }
      void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
      void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
      void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
      void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
      void OnRenegotiationNeeded() override;
      void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
      void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
      void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
    };
    /////////////////////////////////////////////////////////////


    /* This class implements interface of datachannel attached to peer connection
       and handles datachannel transmissions between peer and user*/
    /////////////////////////////////////////////////////////////////
    class DCO : public webrtc::DataChannelObserver {
    public:
      enum STATUS{READY_TO_SEND, READY_TO_DOWNLOAD, DOWNLOADING, SENDING, MESSAGING, WAITING, FINISHED_DOWNLOAD, FINISHED_SENDING, CLOSED};
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
      friend class Network;

    public:
      Connection& parent;
      std::mutex fileLock;
      std::mutex channelLock;
      std::mutex progressLock;

    private:
      std::fstream currentFile;

      std::atomic <long> fileSizeInBytes;
      std::atomic <long> numberOfChunks;
      std::atomic <long> chunkCounter;
      std::atomic <long> byteCounter;
      std::atomic <int> progress;

      std::mutex bufferLock;
      std::queue <std::string> buffer;

      std::mutex statusLock;
      STATUS status;

      std::string currentFileName;
      std::mutex dataLock;

    public:
      DCO(Connection& parent) : parent(parent) {

      }
      //implementation of webrtc DataChannelObserver
      void OnStateChange() override;
      void OnMessage(const webrtc::DataBuffer& buffer) override;
      void OnBufferedAmountChange(uint64_t previous_amount) override;

    public:
      bool isInOpenState();
      void closeChannel();
      void setStatus(STATUS status);
      STATUS getCurrentStatus ();
      bool statusIsEqual(STATUS status);
      void setCurrentFileName(const std::string& name);
      int getDownloadProgress();
      std::string getCurrentFileName();
      void sendTextMessage(const std::string& message);
      void startSendingFile();
      std::string popMessageFromBuffer();

    private:
      void pushMessageToBuffer(const webrtc::DataBuffer& buffer);
      void waitForNetworkThread();
      std::string makeJsonString(const std::string& type, const std::string& sender, const std::string& message, long size);
      std::string getLocalDownloadPath(std::string file);
      void closeCurrentFile();
    };

  class CSDO : public webrtc::CreateSessionDescriptionObserver {
     private:
      Connection& parent;

     public:
      CSDO(Connection& parent) : parent(parent) {
      }
      void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
      void OnFailure(const std::string& error) override;
    };


  class SSDO : public webrtc::SetSessionDescriptionObserver {
     private:
      Connection& parent;
     public:
      SSDO(Connection& parent) : parent(parent) {}
      void OnSuccess() override;
      void OnFailure(const std::string& error) override;
  };
///////////////////////////////////////////////////////////////////////////////

  /* Network class handles data transmissions on separate threads using protected queues.
    Each time new data is to be sent or received DCO object pushes itself to corresponding queue.
    This allows simultaneous transmission in turns. Number of threads to be initialized passed to constructor.
    */
///////////////////////////////////////////////////////////////
  class Network{
  public:
      Network (int numberOfDownloaders = 1, int numberOfSenders = 1);
      void startNetworkThreads(); //starts all threads
      void stopNetworkThreads(); //says stop to all threads

  public:
      void pushToDownloadQueue(Connection::DCO* channel); //pushes channel to downloader queue
      void pushToSendQueue(Connection::DCO* channel);  //pushes channel to sender queue

  private:
      Connection::DCO* popFromDownloadQueue(); //pops one channel from downloader queue
      Connection::DCO* popFromSendQueue();  //pops one channel from sender queue

      void senderThread(int id); //sender thread implementation
      void downloaderThread(int id); //downloader thread implementation

  private:
      int numberOfDownloaders;
      int numberOfSenders;

      std::deque <Connection::DCO*> downloadQueue;
      std::mutex downloadQueueLock;
      std::condition_variable waitToDownloadCondition;
      int numOfWaitingDownloaders;

      std::deque <Connection::DCO*> sendQueue;
      std::mutex sendQueueLock;
      std::condition_variable waitToSendCondition;
      int numOfWaitingSenders;

      std::vector <std::thread*> downloaders;
      std::vector <std::thread*> senders;
      std::mutex threadsLock;

      std::atomic <bool> stopped; //stop flag
  };
  /////////////////////////////////////////////////////////////////

    public:
        Connection() :
            pco(*this),
            csdo(new rtc::RefCountedObject<CSDO>(*this)),
            ssdo(new rtc::RefCountedObject<SSDO>(*this)) {
        }
        static std::vector<std::string> getLocalFiles(const std::string& dir); // gets list of local files (located in files folder)
        friend class DCO;

    public:
      bool isNewConnection(); //checks whether peer connection is new or just created
      bool isConnected(); //checks whether peer is connected
      bool isChannelOpen(long id); //checks whether specific channel is open
      bool addDataChannel(long id); //adds datachannel to the peer connection
      bool openMessageChannel(); //opens channel for text messaging
      void setProperties(const std::string& PeerId, const std::string& SelfId, callback callbackToLinker_, //sets connection properties
                         const std::string& sharedPath, const std::string& downloadPath, Network* network);
      void acceptSDP(const std::string& parameter, const std::string& type); //accepts sdp message
      void acceptIceCandidate(const std::string& parameter); //accepts ice message
      void sendFile(const std::string filename);
      void sendTextMessage (const std::string& message);
      void createOffer(); //creates sdp offer
      void closeAllChannels(); //closes all channels
      void removeDataChannel(long id); //removes specific channel
      void printProgress();


    private:
      void onSuccessCSD(webrtc::SessionDescriptionInterface* desc); //called sdp message is created is created (custom)
      void onIceCandidate(const webrtc::IceCandidateInterface* candidate); // called when ice candidate is received from STUN (custom)
      void onIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state); //called when ice gathering finished (custom)
      int hash(const std::string &key, int tableSize);
      bool fileAlreadyExists(const std::string& name);
      std::string generateNameForNewDownload(const std::string& originalName);

    public:
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
        PCO  pco;
        rtc::scoped_refptr<CSDO> csdo;
        rtc::scoped_refptr<SSDO> ssdo;


    private:
        Network* network;
        std::string sdp_type;
        picojson::array ice_array;
        std::map <long, DCO*> channels; // map for storing active datachannels
        std::mutex channel_map_lock;

        std::string PeerId;
        std::string SelfID;
        callback callbackToLinker_;
        std::string sharedPath;
        std::string downloadPath;
};

#endif
