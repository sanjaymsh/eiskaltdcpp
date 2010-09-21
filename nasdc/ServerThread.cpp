/*
 * ServerThread.cpp
 *
 *  Created on: 17.08.2009
 *      Author: alex
 */

//---------------------------------------------------------------------------
#include "stdafx.h"
#include "dcpp/DCPlusPlus.h"
//---------------------------------------------------------------------------
#include "utility.h"
#include "ServerThread.h"
//---------------------------------------------------------------------------
#include "dcpp/ClientManager.h"
#include "dcpp/Client.h"
#include "dcpp/ConnectionManager.h"
#include "dcpp/FavoriteManager.h"
#include "dcpp/DownloadManager.h"
#include "dcpp/UploadManager.h"
#include "dcpp/QueueManager.h"
#include "dcpp/SearchManager.h"
//#include "dcpp/ChatMessage.h"

#include "dcpp/version.h"

//#include "../dht/DHT.h"

ServerThread::ClientMap ServerThread::clients;

//----------------------------------------------------------------------------
ServerThread::ServerThread() : threadId(0), bTerminated(false), lastUp(0), lastDown(0), lastUpdate(GET_TICK())
{
	pthread_mutex_init(&mtxServerThread, NULL);
}
//---------------------------------------------------------------------------

ServerThread::~ServerThread() {
    if(threadId != 0) {
        Close();
        WaitFor();
    }

    pthread_mutex_destroy(&mtxServerThread);
}
//---------------------------------------------------------------------------

static void* ExecuteServerThread(void* SrvThread) {
	((ServerThread *)SrvThread)->Run();
	return 0;
}
//---------------------------------------------------------------------------

void ServerThread::Resume() {
    int iRet = pthread_create(&threadId, NULL, ExecuteServerThread, this);
    if(iRet != 0) {
		AppendSpecialLog("[ERR] Failed to create new ServerThread!");
    }
}
//---------------------------------------------------------------------------

void ServerThread::Run()
{
	TimerManager::getInstance()->addListener(this);
	QueueManager::getInstance()->addListener(this);
	LogManager::getInstance()->addListener(this);
	//WebServerManager::getInstance()->addListener(this);

	//bool bWithWeb = BOOLSETTING(WEBSERVER);
	//if (bWithWeb) {
		//try {
			//WebServerManager::getInstance()->Start();
		//} catch(const Exception& e) {}
	//}

	TimerManager::getInstance()->start();

	try {
		File::ensureDirectory(SETTING(LOG_DIRECTORY));
	} catch (const FileException) {	}

	autoConnect();

	int result;
	const uint64_t millis = 1000;

	timeval tv;
	fd_set rfd;
	fd_set *rfdp = NULL;

	//while(!bTerminated)
	//{
		//if (/*bWithWeb && */webSock) {
			tv.tv_sec = static_cast<uint32_t>(millis/1000);
			tv.tv_usec = static_cast<uint32_t>((millis%1000)*1000);

			rfdp = &rfd;
			FD_ZERO(&rfd);

			//FD_SET(webSock, &rfd);

			//result = select((int)(webSock+1), rfdp, NULL, NULL, &tv);
			//if (result < 0  && errno == EINTR) {
				//continue;
			//} else if (result >= 0 && rfdp && FD_ISSET(webSock, rfdp)) {
				////WebServerManager::getInstance()->getServerSocket().incoming();
			//}
		//}
	//}
}

//---------------------------------------------------------------------------
void ServerThread::Close()
{
	//WebServerManager::getInstance()->removeListener(this);
	SearchManager::getInstance()->disconnect();

	LogManager::getInstance()->removeListener(this);
	QueueManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);

	for(ClientIter i = clients.begin() ; i != clients.end() ; i++) {
		Client* cl = i->second;
		cl->removeListener(this);
		cl->disconnect(true);
		ClientManager::getInstance()->putClient(cl);
	};

	ConnectionManager::getInstance()->disconnect();

	bTerminated = true;
}
//---------------------------------------------------------------------------

void ServerThread::WaitFor() {
	if(threadId != 0) {
		pthread_join(threadId, NULL);
        threadId = 0;
	}
}
//---------------------------------------------------------------------------

//void ServerThread::startSocket()
//{
	//SearchManager::getInstance()->disconnect();
	//ConnectionManager::getInstance()->disconnect();
	////DHT::getInstance()->stop();

	//try {
		//ConnectionManager::getInstance()->listen();
	//} catch(const Exception&) {
		////..
	//}
	//try {
		//SearchManager::getInstance()->listen();
	//} catch(const Exception&) {
		////..
	//}
	////try {
		////DHT::getInstance()->start();
	////} catch(const Exception&) {
		//////..
	////}
//}
//----------------------------------------------------------------------------

void ServerThread::autoConnect()
{
	const FavoriteHubEntryList& fl = FavoriteManager::getInstance()->getFavoriteHubs();
	for(FavoriteHubEntryList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
		FavoriteHubEntry* entry = *i;
		if (entry->getConnect()) {
			Client* cl = ClientManager::getInstance()->getClient(entry->getServer());
			cl->addListener(this);
			cl->connect();
		}
	}
}
//----------------------------------------------------------------------------
void ServerThread::on(TimerManagerListener::Second, uint64_t aTick) throw()
{
	int64_t diff = (int64_t)((lastUpdate == 0) ? aTick - 1000 : aTick - lastUpdate);
	int64_t updiff = Socket::getTotalUp() - lastUp;
	int64_t downdiff = Socket::getTotalDown() - lastDown;

	SettingsManager::getInstance()->set(SettingsManager::TOTAL_UPLOAD, SETTING(TOTAL_UPLOAD) + updiff);
	SettingsManager::getInstance()->set(SettingsManager::TOTAL_DOWNLOAD, SETTING(TOTAL_DOWNLOAD) + downdiff);

	lastUpdate = aTick;
	lastUp = Socket::getTotalUp();
	lastDown = Socket::getTotalDown();

	//if(SETTING(DISCONNECT_SPEED) < 1) {
		//SettingsManager::getInstance()->set(SettingsManager::DISCONNECT_SPEED, 1);
	//}
}

void ServerThread::on(Connecting, const Client* cur) throw() {
	ClientIter i = clients.find(cur->getAddress());
	if(i == clients.end()) {
		clients[cur->getAddress()] = const_cast<Client*>(cur);
	}
}

void ServerThread::on(Connected, const Client*) throw() {

}

void ServerThread::on(UserUpdated, const Client*, const OnlineUserPtr& user) throw() {

}

void ServerThread::on(UsersUpdated, const Client*, const OnlineUserList& aList) throw() {

}

void ServerThread::on(UserRemoved, const Client*, const OnlineUserPtr& user) throw() {

}

void ServerThread::on(Redirect, const Client*, const string& line) throw() {

}

void ServerThread::on(Failed, const Client*, const string& line) throw() {

}

void ServerThread::on(GetPassword, const Client* cur) throw() {
	ClientIter i = clients.find(cur->getAddress());
	if (i != clients.end()) {
		Client* cl = i->second;
		string pass = cl->getPassword();
		if (!pass.empty())
			cl->password(pass);
	}
}

void ServerThread::on(HubUpdated, const Client*) throw() {

}

//void ServerThread::on(ClientListener::Message, const Client *cl, const ChatMessage& message) throw()
//{
	//StringMap params;
	//string msg = message.format();

	//if (message.to && message.replyTo) {
		//if (BOOLSETTING(LOG_PRIVATE_CHAT)) {
			//const string& hint = cl->getHubUrl();
			//const CID& cid = message.replyTo->getUser()->getCID();
			//bool priv = FavoriteManager::getInstance()->isPrivate(hint);

			//params["message"] = Text::fromUtf8(msg);
			//params["hubNI"] = Util::toString(ClientManager::getInstance()->getHubNames(cid, hint, priv));
			//params["hubURL"] = Util::toString(ClientManager::getInstance()->getHubs(cid, hint, priv));
			//params["userCID"] = cid.toBase32();
			//params["userNI"] = ClientManager::getInstance()->getNicks(cid, hint, priv)[0];
			//params["myCID"] = ClientManager::getInstance()->getMe()->getCID().toBase32();
			//LOG(LogManager::PM, params);
		//}
	//} else {
		//if(BOOLSETTING(LOG_MAIN_CHAT)) {
			//params["message"] = Text::fromUtf8(msg);
			//cl->getHubIdentity().getParams(params, "hub", false);
			//params["hubURL"] = cl->getHubUrl();
			//cl->getMyIdentity().getParams(params, "my", true);
			//LOG(LogManager::CHAT, params);
		//}
	//}


	//cout << msg.c_str() << "\n" << endl;
//}

//void ServerThread::on(StatusMessage, const Client *cl, const string& line, int statusFlags) throw()
//{
	//string msg = line;

	//if(BOOLSETTING(LOG_STATUS_MESSAGES)) {
		//StringMap params;
		//cl->getHubIdentity().getParams(params, "hub", false);
		//params["hubURL"] = cl->getHubUrl();
		//cl->getMyIdentity().getParams(params, "my", true);
		//params["message"] = Text::fromUtf8(msg);
		//LOG(LogManager::STATUS, params);
	//}

	//cout << msg.c_str() << "\n" << endl;
//}

void ServerThread::on(NickTaken, const Client*) throw() {

}

void ServerThread::on(SearchFlood, const Client*, const string& line) throw() {

}

//void ServerThread::on(WebServerListener::Setup) throw() {
	////webSock = WebServerManager::getInstance()->getServerSocket().getSock();
//}

//void ServerThread::on(WebServerListener::ShutdownPC, int action) throw() {

//}

