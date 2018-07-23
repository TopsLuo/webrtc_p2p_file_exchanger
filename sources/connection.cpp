#include "connection.h"
void Connection::onSuccessCSD(webrtc::SessionDescriptionInterface* desc) {
    peer_connection->SetLocalDescription(ssdo, desc);
    std::string sdp;
    desc->ToString(&sdp);
    //std::cout << sdp_type << " SDP:begin" << std::endl << sdp << sdp_type << " SDP:end" << std::endl;
    callbackToLinker_(PeerId, sdp_type, sdp);
    if(sdp_type == "offer")
        sdp_type = "";
}

void Connection::onIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    picojson::object ice;
    std::string candidate_str;
    candidate->ToString(&candidate_str);
    ice.insert(std::make_pair("candidate", picojson::value(candidate_str)));
    ice.insert(std::make_pair("sdpMid", picojson::value(candidate->sdp_mid())));
    ice.insert(std::make_pair("sdpMLineIndex", picojson::value(static_cast<double>(candidate->sdp_mline_index()))));  
    ice_array.push_back(picojson::value(ice));
    callbackToLinker_(PeerId, "ice", picojson::value(ice_array).serialize(true));
    ice_array.clear();
    //std::cout << picojson::value(ice_array).serialize(true) << std::endl;
}

bool Connection::isNewConnection(){
    return (peer_connection->ice_gathering_state() == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringNew &&
              peer_connection->ice_connection_state() == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew);
}

bool Connection::isConnected(){
    return ((peer_connection->ice_connection_state()== webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected ||
            peer_connection->ice_connection_state()== webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted)&&
            peer_connection->signaling_state()== webrtc::PeerConnectionInterface::SignalingState::kStable);
}

bool Connection::addDataChannel(long id){
    if(isChannelOpen(id))
        return true;
    removeDataChannel(id);
    webrtc::DataChannelInit config;
    DCO* channel = new DCO(*this);
    if(id == 0)
        channel->setStatus(DCO::MESSAGING);
    else
        channel->setStatus(DCO::WAITING);
    channel->data_channel = peer_connection->CreateDataChannel(std::to_string(id), &config);
    channel->data_channel->RegisterObserver(channel);
    {
        std::lock_guard <std::mutex> lock(channel_map_lock);
        this->channels.insert(std::pair<long, DCO*>(id, channel));
    }
    if(id == 0)
    {
        if(!this->isConnected())
            this->createOffer();
    }
    return true;
}

bool Connection::openMessageChannel(){
    if(isChannelOpen(0))
        return true;
    return addDataChannel(0);
}

void Connection::sendFile(const std::string filename){
    int key = hash(filename + PeerId, 1000);
    if(addDataChannel(key)){
        std::lock_guard <std::mutex> lock(channel_map_lock);
        DCO* channel = channels[key];
        channel->setStatus(DCO::READY_TO_SEND);
        channel->setCurrentFileName(filename);
        if(this->isConnected())
            channel->startSendingFile();
        else
            createOffer();
    }
}

void Connection::sendTextMessage(const std::string& message){
    if(isChannelOpen(0)){
        std::lock_guard <std::mutex> lock(channel_map_lock);
        channels[0]->sendTextMessage(message);
    }
}

void Connection::createOffer(){
    sdp_type = "offer";
    peer_connection->CreateOffer(csdo, nullptr);
}

void Connection::onIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state){
    if(new_state == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete){

    }
}

void Connection::setProperties(const std::string& PeerId, const std::string& SelfId, callback callbackToLinker_, const std::string& sharedPath, const std::string& downloadPath, Network *network){
      this->PeerId = PeerId;
      this->SelfID = SelfId;
      this->callbackToLinker_ = callbackToLinker_;
      this->sharedPath = sharedPath;
      this->downloadPath = downloadPath;
      this->network = network;
}

void Connection::acceptSDP(const std::string& parameter, const std::string& type){
      webrtc::SdpParseError error;
      webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, parameter, &error));
      if (session_description == nullptr) {
        std::cout << "Error on CreateSessionDescription." << std::endl
                  << error.line << std::endl
                  << error.description << std::endl;
        std::cout << "Offer SDP:begin" << std::endl << parameter << std::endl << "Offer SDP:end" << std::endl;
      }
      if(type == "offer")
      {
        peer_connection->SetRemoteDescription(ssdo, session_description);
        sdp_type = "answer";
        peer_connection->CreateAnswer(csdo, nullptr);
      }
      else if(type == "answer")
      {
          peer_connection->SetRemoteDescription(ssdo, session_description);
      }
}

void Connection::acceptIceCandidate(const std::string& parameter){
    picojson::value v;
    std::string err = picojson::parse(v, parameter);
    if (!err.empty()) {
      std::cout << "Error on parse json : " << err << std::endl;
      return;
    }

    webrtc::SdpParseError err_sdp;
    for (auto& ice_it : v.get<picojson::array>()) {
      picojson::object& ice_json = ice_it.get<picojson::object>();
      webrtc::IceCandidateInterface* ice =
        CreateIceCandidate(ice_json.at("sdpMid").get<std::string>(),
                          static_cast<int>(ice_json.at("sdpMLineIndex").get<double>()),
                          ice_json.at("candidate").get<std::string>(),
                          &err_sdp);
      if (!err_sdp.line.empty() && !err_sdp.description.empty()) {
        std::cout << "Error on CreateIceCandidate" << std::endl
                  << err_sdp.line << std::endl
                  << err_sdp.description << std::endl;
        return;
      }

      peer_connection->AddIceCandidate(ice);
    }
}

bool Connection::isChannelOpen(long id){
    std::lock_guard <std::mutex> lock(channel_map_lock);
    auto channel = channels.find(id);
    if(channel == channels.end())
        return false;
    if(channels[id]->statusIsEqual(DCO::CLOSED) || !channels[id]->isInOpenState())
        return false;
    return true;
}

void Connection::closeAllChannels(){
    std::map <long, DCO*>::iterator it;
    for (it = channels.begin(); it!=channels.end(); it++){
        removeDataChannel(it->first);
    }
}

void Connection::removeDataChannel(long id){
     std::lock_guard <std::mutex> lock(channel_map_lock);
     auto channel = channels.find(id);
     if(channel != channels.end()){
         DCO* chnl = channels[id];

         std::lock_guard <std::mutex> lock(chnl->progressLock);
         chnl->data_channel->Close();
         {
             delete(chnl);
             chnl = NULL;
         }
         channels.erase(id);
     }

}

void Connection::printProgress(){
    std::lock_guard <std::mutex> lock(channel_map_lock);
    std::map <long, DCO*>::iterator it;
    for (it = channels.begin(); it!=channels.end(); it++){
        DCO* channel = it->second;
        if(channel->statusIsEqual(DCO::DOWNLOADING))
            std::cout << channel->getCurrentFileName() << " [" << channel->getDownloadProgress() << "%]" << std::endl;
    }
}

int Connection::hash(const std::string &key, int tableSize){
    int hashVal = 0;

    for(int i = 0; i<key.length();  i++)
      hashVal = 37*hashVal+key[i];

    hashVal %= tableSize;

    if(hashVal<0)
      hashVal += tableSize;

    return hashVal;
}

bool Connection::fileAlreadyExists(const std::string &name)
{
    std::vector<std::string> files = getLocalFiles(sharedPath);
    bool found = false;
    for (unsigned int i = 0; i < files.size(); i++)
    {
        if(files.at(i) == name)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> Connection::getLocalFiles(const std::string &dir)
{
    using namespace std;
    std::vector<std::string> files;
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return files;
    }

    while ((dirp = readdir(dp)) != NULL) {
        files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return files;
}

std::string Connection::generateNameForNewDownload(const std::string &originalName)
{
    std::string newName(originalName);

    int index = 0;
    while(fileAlreadyExists(newName)){
        index++;
        newName = "(" + std::to_string(index) + ")" + originalName;
    }
    return newName;
}


//////////////////////////////////////////////
//PeerConnectionObserver implementation
//////////////////////////////////////////////

void Connection::PCO::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::SignalingChange(" << new_state << ")" << std::endl;
      if(new_state == webrtc::PeerConnectionInterface::SignalingState::kClosed)
      {
          //parent.sender(parent.PeerId, "remove", "");
      }
}

void Connection::PCO::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::AddStream" << std::endl;
}

void Connection::PCO::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::RemoveStream" << std::endl;
}

void Connection::PCO::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::DataChannel(" << data_channel
//                << ", " << data_channel->label() << ")" << std::endl;
      long id = std::stol(data_channel->label());
      parent.removeDataChannel(id);
      DCO* channel = new DCO(parent);
      if(id == 0)
          channel->setStatus(DCO::MESSAGING);
      else
          channel->setStatus(DCO::WAITING);
      channel->data_channel = data_channel;
      channel->data_channel->RegisterObserver(channel);
      parent.channels.insert(std::pair<long, DCO*>(id, channel));
}

void Connection::PCO::OnRenegotiationNeeded() {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::RenegotiationNeeded" << std::endl;
}

void Connection::PCO::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::IceConnectionChange(" << new_state << ")" << std::endl;
      if(new_state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
      {
          parent.peer_connection->Close();
      }

}

void Connection::PCO::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)  {
//        std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::IceGatheringChange(" << new_state << ")" << std::endl;
        parent.onIceGatheringChange(new_state);
}

void Connection::PCO::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "PeerConnectionObserver::IceCandidate" << std::endl;
      parent.onIceCandidate(candidate);
}


//////////////////////////////////////////////
//DataChannelObserver implementation
////////////////////////////////////////////

void Connection::DCO::OnStateChange() {
//      std::cout << std::this_thread::get_id() << ":"
//                << "DataChannelObserver::StateChange: " << data_channel->state()<< " Peer: " << parent.PeerId  << std::endl;
      switch (data_channel->state()){
      case webrtc::DataChannelInterface::DataState::kOpen: {
          if(status == READY_TO_SEND)
            startSendingFile();
          break;
      }

      case webrtc::DataChannelInterface::DataState::kClosed: {
          break;
      }

      case webrtc::DataChannelInterface::DataState::kClosing: {
          //std::lock_guard <std::mutex> lock(fileLock);
          closeChannel();
          break;
      }
      }
}

void Connection::DCO::OnMessage(const webrtc::DataBuffer& buffer){
        if(!statusIsEqual(READY_TO_DOWNLOAD) && !statusIsEqual(DOWNLOADING))
        {
          //std::cout << buffer.data.data<char>() << std::endl;
            std::string msg(buffer.data.data<char>(), buffer.data.size());
            picojson::value v;
            std::string err = picojson::parse(v, msg);
              if (!err.empty()) {
                std::cout << "Error on parse json : " << err << std::endl;
                return;
              }
            picojson::object& obj = v.get<picojson::object>();
            std::string type = obj.at("type").get<std::string>();
            std::string sender = obj.at("sender").get<std::string>();
            std::string message = obj.at("message").get<std::string>();
            long size = std::stol(obj.at("size").get<std::string>());
            switch(status){
                case MESSAGING: {
                    if(type == "text")
                    {
                        std::cout << ">" << sender << ": " << message << std::endl;
                    }
                    break;
                }

                case WAITING: {
                    if(type == "offer")
                    {

                        fileSizeInBytes = size;
                        currentFileName = message;

                        std::string path = parent.downloadPath + "/"+ parent.PeerId + "_" +  currentFileName;

                        currentFile.open(path, std::ios_base::out | std::fstream::binary | std::fstream::app);
                        if(!currentFile.is_open())
                        {
                            std::string reply = makeJsonString("error", parent.SelfID,"", 0);
                            webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(reply.c_str(), reply.size()), true);
                            data_channel->Send(buffer);
                            setStatus(WAITING);
                            break;
                        }
                        long position = currentFile.tellp();
                        byteCounter = position;
                        progress = (position*100)/fileSizeInBytes;

                        setStatus(READY_TO_DOWNLOAD);
                        std::string reply = makeJsonString("answer", parent.SelfID, message, currentFile.tellp());
                        webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(reply.c_str(), reply.size()), true);
                        data_channel->Send(buffer);
                    }
                    break;
                }
                case READY_TO_SEND: {
                    if(type == "answer")
                    {
                        if(message == currentFileName)
                        {
                            currentFile.seekg(size);
                            chunkCounter = 0;
                            numberOfChunks = (fileSizeInBytes - size)/CHUNK_SIZE;
                            setStatus(SENDING);
                            std::cout << "sending " << currentFileName << " to [" << parent.PeerId << "] ..." << std::endl;
                            if(parent.network != NULL)
                                parent.network->pushToSendQueue(this);
                        }
                    }

                    else if(type == "error")
                    {
                        data_channel->Close();
                    }
                    break;
                }
           }
        }
        else
        {
            if(statusIsEqual(READY_TO_DOWNLOAD))
            {
                std::cout << "downloading " << currentFileName << " from [" << parent.PeerId << "] ..." << std::endl;
                setStatus(DOWNLOADING);
                pushMessageToBuffer(buffer);
                parent.network->pushToDownloadQueue(this);
            }

            else if(statusIsEqual(DOWNLOADING))
                pushMessageToBuffer(buffer);
                parent.network->pushToDownloadQueue(this);
        }

    }

    void Connection::DCO::OnBufferedAmountChange(uint64_t previous_amount){
    //          std::cout << std::this_thread::get_id() << ":"
    //                    << "DataChannelObserver::BufferedAmountChange(" << previous_amount << ")" << std::endl;
          if(data_channel->buffered_amount() < 2*CHUNK_SIZE)
          {
              parent.network->pushToSendQueue(this);
          }
    }

    bool Connection::DCO::isInOpenState(){
        return data_channel->state() == 1;
    }

    void Connection::DCO::setStatus(STATUS status){
        std::lock_guard<std::mutex> lock(statusLock);
        this->status = status;
    }

    bool Connection::DCO::statusIsEqual(STATUS status)
    {
        std::lock_guard<std::mutex> lock(statusLock);
        return this->status == status;
    }

    Connection::DCO::STATUS Connection::DCO::getCurrentStatus()
    {
        std::lock_guard<std::mutex> lock(statusLock);
        return this->status;
    }

    void Connection::DCO::setCurrentFileName(const std::string &name){
        this->currentFileName = name;
    }



    void Connection::DCO::closeChannel(){
        std::lock_guard <std::mutex> lock(channelLock);
        STATUS prevState = getCurrentStatus();
        setStatus(CLOSED);
        if(prevState == CLOSED)
            return;
        switch(prevState){
            case SENDING:{
                std::cout << "failed to send " << currentFileName << ". connection was lost!" <<std::endl;
                closeCurrentFile();
              break;
            }
            case DOWNLOADING:{
                std::cout << "failed to download " << currentFileName << ". connection was lost!" <<std::endl;
                closeCurrentFile();
                break;
            }
            case FINISHED_DOWNLOAD:{
                std::cout << currentFileName << " has been successfully downloaded." << std::endl;
                closeCurrentFile();

                std::string command = "mv " + parent.downloadPath + "/'"+ parent.PeerId +"_"+currentFileName + "' " + parent.downloadPath + "/'"+currentFileName +"'";
                std::system(command.c_str());
                std::string newName = parent.generateNameForNewDownload(currentFileName);
                command = "mv -u " + parent.downloadPath + "/'"+currentFileName + "' " + parent.sharedPath + "/'" + newName + "'";
                std::system(command.c_str());
                parent.callbackToLinker_("", "update", "");
                break;
            }

            case FINISHED_SENDING:{
                std::cout <<  currentFileName << " has been successfully sent." << std::endl;
                closeCurrentFile();
                break;
            }
        }
}

void Connection::DCO::closeCurrentFile(){
    std::lock_guard <std::mutex> fileLocker(fileLock);
    if(currentFile.is_open())
        currentFile.close();
}


int Connection::DCO::getDownloadProgress(){
    return progress;
}

std::string Connection::DCO::getCurrentFileName(){
    std::lock_guard<std::mutex> lock(dataLock);
    return  currentFileName;
}

std::string Connection::DCO::getLocalDownloadPath(std::string file){

}

void Connection::DCO::sendTextMessage(const std::string& message){
    std::string msg = makeJsonString("text", parent.SelfID, message, message.size());
    webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(msg.c_str(), msg.size()), true);
    data_channel->Send(buffer);
}

std::string Connection::DCO::makeJsonString(const std::string& type, const std::string& sender, const std::string& message, long size){
    picojson::object ice;
    ice.insert(std::make_pair("type", picojson::value(type)));
    ice.insert(std::make_pair("sender", picojson::value(sender)));
    ice.insert(std::make_pair("message", picojson::value(message)));
    ice.insert(std::make_pair("size", picojson::value(std::to_string(size))));
    return picojson::value(ice).serialize(true);
}

void Connection::DCO::startSendingFile(){
    if(statusIsEqual(READY_TO_SEND))
    {
        std::string path = parent.sharedPath + "/" + currentFileName;
        currentFile.open(path, std::fstream::in | std::fstream::binary);
        if(currentFile.is_open())
        {
            currentFile.seekg(0, currentFile.end);
            fileSizeInBytes = currentFile.tellg();
            currentFile.seekg(0);
            std::string msg = makeJsonString("offer", parent.SelfID, currentFileName, fileSizeInBytes);
            webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(msg.c_str(), msg.size()), true);
            data_channel->Send(buffer);
        }
    }
}


void Connection::DCO::pushMessageToBuffer(const webrtc::DataBuffer &buffer){
    std::lock_guard <std::mutex> lock(bufferLock);
    const std::string data (std::string(buffer.data.data<char>(), buffer.data.size()));
    this->buffer.push(std::move(data));
}

std::string Connection::DCO::popMessageFromBuffer(){
    std::lock_guard <std::mutex> lock(bufferLock);
    if(this->buffer.size() == 0)
        return std::string();
    std::string data= std::move (this->buffer.front());
    this->buffer.pop();
    return data;
}

//////////////////////////////////////
//CSDO implementation
//////////////////////////////////////

void Connection::CSDO::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "CreateSessionDescriptionObserver::OnSuccess" << std::endl;
      parent.onSuccessCSD(desc);
}

void Connection::CSDO::OnFailure(const std::string& error) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "CreateSessionDescriptionObserver::OnFailure" << std::endl << error << std::endl;
}



//////////////////////////////////////
//SSDO implementation
//////////////////////////////////////

void Connection::SSDO::OnSuccess(){
//      std::cout << std::this_thread::get_id() << ":"
//                << "SetSessionDescriptionObserver::OnSuccess" << std::endl;
}

void Connection::SSDO::OnFailure(const std::string& error) {
//      std::cout << std::this_thread::get_id() << ":"
//                << "SetSessionDescriptionObserver::OnFailure" << std::endl << error << std::endl;
}

///////////////////////////////////////
//Network implementation
///////////////////////////////////////

Connection::Network::Network(int numberOfDownloaders, int numberOfSenders):
    numberOfDownloaders(numberOfDownloaders),
    numberOfSenders(numberOfSenders),
    stopped(false){
}

void Connection::Network::startNetworkThreads(){
    std::lock_guard <std::mutex> lock(threadsLock);
    for (int i=0; i<numberOfDownloaders; i++){
        if(downloaders.size() == numberOfDownloaders)
            break;
        std::thread* th = new std::thread(&Connection::Network::downloaderThread, this, i);
        downloaders.push_back(th);
    }

    for (int i=0; i<numberOfSenders; i++){
        if(senders.size() == numberOfSenders)
            break;
        std::thread* th = new std::thread(&Connection::Network::senderThread, this, i);
        senders.push_back(th);
    }
}

void Connection::Network::stopNetworkThreads()
{
    std::lock_guard <std::mutex> lock(threadsLock);
    stopped = true;
    waitToDownloadCondition.notify_all();
    waitToSendCondition.notify_all();

    for(std::thread* t : senders){
        t->join();
        delete(t);
    }

    for(std::thread* t : downloaders){
        t->join();
        delete(t);
    }
    senders.clear();
    downloaders.clear();
    return;
}


Connection::DCO* Connection::Network::popFromDownloadQueue()
{
    std::unique_lock <std::mutex> lock(downloadQueueLock);
    if(downloadQueue.size() == 0){
        numOfWaitingDownloaders++;
        while(downloadQueue.size()==0){
            waitToDownloadCondition.wait(lock);
            if(stopped == true)
                return NULL;
        }
        numOfWaitingDownloaders--;
    }
    Connection::DCO* channel = downloadQueue.front();
    downloadQueue.pop_front();
    return channel;
}

void Connection::Network::pushToDownloadQueue(Connection::DCO* channel){
    std::lock_guard <std::mutex> lock(downloadQueueLock);
    downloadQueue.push_back(channel);
    if(numOfWaitingDownloaders > 0)
        waitToDownloadCondition.notify_one();
    return;
}

Connection::DCO* Connection::Network::popFromSendQueue()
{
    std::unique_lock <std::mutex> lock(sendQueueLock);
    if(sendQueue.size() == 0){
        numOfWaitingSenders++;
        while(sendQueue.size() == 0){
            waitToSendCondition.wait(lock);
            if(stopped == true)
                return NULL;
        }
        numOfWaitingSenders--;
    }
    Connection::DCO* channel = sendQueue.front();
    sendQueue.pop_front();
    return channel;
}

void Connection::Network::pushToSendQueue(Connection::DCO *channel){
    std::unique_lock <std::mutex> lock(sendQueueLock);
    sendQueue.push_back(channel);
    if(numOfWaitingSenders > 0)
        waitToSendCondition.notify_one();
    return;
}

void Connection::Network::senderThread(int id)
{
    while(!stopped){
        Connection::DCO* channel = popFromSendQueue();
        if(channel == NULL)
            continue;
        std::lock_guard <std::mutex> progressLocker(channel->progressLock);
        if(channel->statusIsEqual(Connection::DCO::SENDING)){
            char chunk[CHUNK_SIZE];
            if(channel->chunkCounter < channel->numberOfChunks){
                if(channel->data_channel->buffered_amount() > 5 * CHUNK_SIZE){
                    continue;
                }
                try{
                    std::lock_guard <std::mutex> fileLocker(channel->fileLock);
                    if(channel->currentFile.is_open())
                        channel->currentFile.read(chunk, CHUNK_SIZE);
                    else
                        channel->data_channel->Close();
                }
                catch (const std::exception e){
                std::cout << "caught exception" << std::endl;
                    channel->data_channel->Close();
                    continue;
                }
                webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(chunk, CHUNK_SIZE), true);
                if(channel->data_channel->state() == webrtc::DataChannelInterface::DataState::kOpen){
                    channel->data_channel->Send(buffer);
                    channel->chunkCounter++;
                    pushToSendQueue(channel);
                }
            }
            else{
                long rem = channel->fileSizeInBytes % CHUNK_SIZE;
                if(rem > 0){
                    try{
                        std::lock_guard <std::mutex> fileLocker(channel->fileLock);
                        if(channel->currentFile.is_open())
                            channel->currentFile.read(chunk, rem);
                        else
                            channel->data_channel->Close();
                    }
                    catch(const std::exception e){
                        channel->data_channel->Close();
                    }
                    webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(chunk, rem), true);
                    channel->data_channel->Send(buffer);
                }
                channel->setStatus(Connection::DCO::FINISHED_SENDING);
            }
        }
    }
}

void Connection::Network::downloaderThread(int id)
{
    while (!stopped) {
        Connection::DCO* channel = popFromDownloadQueue();
        if(channel == NULL)
            continue;
        std::lock_guard <std::mutex> progressLocker(channel->progressLock);
        if(channel->statusIsEqual(Connection::DCO::DOWNLOADING)){
            std::string data ;//= channel->popMessageFromBuffer();
            while((data = channel->popMessageFromBuffer()).size() != 0){
                try{
                    std::lock_guard <std::mutex> fileLocker(channel->fileLock);
                    if(channel->currentFile.is_open())
                        channel->currentFile.write(data.c_str(), data.size());
                    else
                        channel->data_channel->Close();
                }
                catch (const std::exception e){
                    channel->data_channel->Close();
                }
                channel->byteCounter+=data.size();
                if(channel->byteCounter / (channel->fileSizeInBytes/100) > channel->progress && channel->progress <=100)
                {
                    channel->progress = channel->byteCounter / (channel->fileSizeInBytes/100);
                }
                if(channel->byteCounter >= channel->fileSizeInBytes)
                {
                    channel->setStatus(Connection::DCO::FINISHED_DOWNLOAD);
                    channel->data_channel->Close();
                    break;
                }
            }

        }
    }
}



