﻿#pragma once

#include <mutex>
#include "client/reindexer.h"
#include "debug/backtrace.h"
#include "reindexer_api.h"
#include "server/dbmanager.h"
#include "server/server.h"
#include "thread"
#include "tools/logger.h"
#include "tools/serializer.h"

using namespace reindexer_server;
using std::shared_ptr;

typedef reindexer::client::Reindexer CppClient;
typedef ReindexerTestApi<shared_ptr<CppClient>, reindexer::client::Item> BaseApi;

// for now in default serve 0  always master - other slave - inited in ReplicationApi::SetUp
const size_t kDefaultServerCount = 4;
const size_t kDefaultRpcPort = 4444;
const size_t kDefaultHttpPort = 5555;
const size_t kMaxServerStartTimeSec = 20;

class ServerControl {
public:
	ServerControl(ServerControl&& rhs);
	ServerControl& operator=(ServerControl&&);
	ServerControl(ServerControl& rhs) = delete;
	ServerControl& operator=(ServerControl& rhs) = delete;
	ServerControl();
	~ServerControl();

	struct Interface {
		typedef std::shared_ptr<Interface> Ptr;
		Interface(size_t id, std::atomic_bool& stopped, bool dropDb = false);
		~Interface();

		// Make this server master
		void MakeMaster();
		// Make this server slave
		void MakeSlave(size_t masterId);
		// check with master or slave that sync complete
		bool CheckForSyncCompletion(Ptr rhs);
		// drop db from hdd
		void SetNeedDrop(bool dropDb);
		//

		Server srv;
		BaseApi api;

	private:
		void setServerConfig(const string& role, size_t masterId);
		size_t id_;
		bool dropDb_;
		std::unique_ptr<std::thread> tr;
		std::atomic_bool& stopped_;
	};
	// Get server - wait means wait until server starts if no server
	Interface::Ptr Get(bool wait = true);
	void InitServer(size_t id, bool dropDb = false);
	void Drop();
	bool IsRunning();

private:
	typedef shared_lock<shared_timed_mutex> RLock;
	typedef std::unique_lock<shared_timed_mutex> WLock;

	shared_timed_mutex mtx_;
	std::shared_ptr<Interface> interface;
	std::atomic_bool* stopped_;
};

class ReplicationApi : public ::testing::Test {
public:
	void SetUp();
	void TearDown();

	// stop is sync
	bool StopServer(size_t id, bool dropDb = false);
	// start is sync
	bool StartServer(size_t id, bool dropDb = false);
	// restart is sync
	void RestartServer(size_t id, bool dropDb = false);
	// get server
	ServerControl::Interface::Ptr GetSrv(size_t id);

private:
	vector<ServerControl> svc;
	std::mutex m_;
};