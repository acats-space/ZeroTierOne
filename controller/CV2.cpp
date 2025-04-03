/*
 * Copyright (c)2025 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "CV2.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../node/Constants.hpp"
#include "../node/SHA512.hpp"
#include "EmbeddedNetworkController.hpp"
#include "../version.h"
#include "CtlUtil.hpp"

#include <libpq-fe.h>
#include <sstream>
#include <iomanip>
#include <climits>
#include <chrono>


using json = nlohmann::json;

namespace {

}

using namespace ZeroTier;

CV2::CV2(const Identity &myId, const char *path, int listenPort)
	: DB()
	, _pool()
	, _myId(myId)
	, _myAddress(myId.address())
	, _ready(0)
	, _connected(1)
	, _run(1)
	, _waitNoticePrinted(false)
	, _listenPort(listenPort)
{
	char myAddress[64];
	_myAddressStr = myId.address().toString(myAddress);
	_connString = std::string(path);
	auto f = std::make_shared<PostgresConnFactory>(_connString);
	_pool = std::make_shared<ConnectionPool<PostgresConnection> >(
		15, 5, std::static_pointer_cast<ConnectionFactory>(f));
	
	memset(_ssoPsk, 0, sizeof(_ssoPsk));
	char *const ssoPskHex = getenv("ZT_SSO_PSK");
#ifdef ZT_TRACE
	fprintf(stderr, "ZT_SSO_PSK: %s\n", ssoPskHex);
#endif
	if (ssoPskHex) {
		// SECURITY: note that ssoPskHex will always be null-terminated if libc actually
		// returns something non-NULL. If the hex encodes something shorter than 48 bytes,
		// it will be padded at the end with zeroes. If longer, it'll be truncated.
		Utils::unhex(ssoPskHex, _ssoPsk, sizeof(_ssoPsk));
	}

	auto c = _pool->borrow();
	pqxx::work txn{*c->c};

	pqxx::row r{txn.exec1("SELECT version FROM ztc_database")};
	int dbVersion = r[0].as<int>();
	txn.commit();

	// if (dbVersion < DB_MINIMUM_VERSION) {
	// 	fprintf(stderr, "Central database schema version too low.  This controller version requires a minimum schema version of %d. Please upgrade your Central instance", DB_MINIMUM_VERSION);
	// 	exit(1);
	// }
	_pool->unborrow(c);

	_readyLock.lock();
	
	fprintf(stderr, "[%s] NOTICE: %.10llx controller PostgreSQL waiting for initial data download..." ZT_EOL_S, ::_timestr(), (unsigned long long)_myAddress.toInt());
	_waitNoticePrinted = true;

	initializeNetworks();
	initializeMembers();

	_heartbeatThread = std::thread(&CV2::heartbeat, this);
	_membersDbWatcher = std::thread(&CV2::membersDbWatcher, this);
	_networksDbWatcher = std::thread(&CV2::networksDbWatcher, this);
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i] = std::thread(&CV2::commitThread, this);
	}
	_onlineNotificationThread = std::thread(&CV2::onlineNotificationThread, this);
}

CV2::~CV2()
{
	_run = 0;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	_heartbeatThread.join();
	_membersDbWatcher.join();
	_networksDbWatcher.join();
	_commitQueue.stop();
	for (int i = 0; i < ZT_CENTRAL_CONTROLLER_COMMIT_THREADS; ++i) {
		_commitThread[i].join();
	}
	_onlineNotificationThread.join();
}

bool CV2::waitForReady()
{
	while (_ready < 2) {
		_readyLock.lock();
		_readyLock.unlock();
	}
	return true;
}

bool CV2::isReady()
{
    return (_ready == 2) && _cldemote;
} 

bool CV2::save(nlohmann::json &record,bool notifyListeners)
{
	bool modified = false;
	try {
		if (!record.is_object()) {
			fprintf(stderr, "record is not an object?!?\n");
			return false;
		}
		const std::string objtype = record["objtype"];
		if (objtype == "network") {
			//fprintf(stderr, "network save\n");
			const uint64_t nwid = OSUtils::jsonIntHex(record["id"],0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json,bool>(record,notifyListeners));
					modified = true;
				}
			}
		} else if (objtype == "member") {
			std::string networkId = record["nwid"];
			std::string memberId = record["id"];
			const uint64_t nwid = OSUtils::jsonIntHex(record["nwid"],0ULL);
			const uint64_t id = OSUtils::jsonIntHex(record["id"],0ULL);
			//fprintf(stderr, "member save %s-%s\n", networkId.c_str(), memberId.c_str());
			if ((id)&&(nwid)) {
				nlohmann::json network,old;
				get(nwid,network,id,old);
				if ((!old.is_object())||(!_compareRecords(old,record))) {
					//fprintf(stderr, "commit queue post\n");
					record["revision"] = OSUtils::jsonInt(record["revision"],0ULL) + 1ULL;
					_commitQueue.post(std::pair<nlohmann::json,bool>(record,notifyListeners));
					modified = true;
				} else {
					//fprintf(stderr, "no change\n");
				}
			}
		} else {
			fprintf(stderr, "uhh waaat\n");
		}
	} catch (std::exception &e) {
		fprintf(stderr, "Error on PostgreSQL::save: %s\n", e.what());
	} catch (...) {
		fprintf(stderr, "Unknown error on PostgreSQL::save\n");
	}
	return modified;
}

void CV2::eraseNetwork(const uint64_t networkId)
{
	fprintf(stderr, "PostgreSQL::eraseNetwork\n");
	char tmp2[24];
	waitForReady();
	Utils::hex(networkId, tmp2);
	std::pair<nlohmann::json,bool> tmp;
	tmp.first["id"] = tmp2;
	tmp.first["objtype"] = "_delete_network";
	tmp.second = true;
	_commitQueue.post(tmp);
	nlohmann::json nullJson;
	_networkChanged(tmp.first, nullJson, true);
}

void CV2::eraseMember(const uint64_t networkId, const uint64_t memberId)
{
	fprintf(stderr, "PostgreSQL::eraseMember\n");
	char tmp2[24];
	waitForReady();
	std::pair<nlohmann::json,bool> tmp, nw;
	Utils::hex(networkId, tmp2);
	tmp.first["nwid"] = tmp2;
	Utils::hex(memberId, tmp2);
	tmp.first["id"] = tmp2;
	tmp.first["objtype"] = "_delete_member";
	tmp.second = true;
	_commitQueue.post(tmp);
	nlohmann::json nullJson;
	_memberChanged(tmp.first, nullJson, true);
}

void CV2::nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress &physicalAddress)
{
	std::lock_guard<std::mutex> l(_lastOnline_l);
	std::pair<int64_t, InetAddress> &i = _lastOnline[std::pair<uint64_t,uint64_t>(networkId, memberId)];
	i.first = OSUtils::now();
	if (physicalAddress) {
		i.second = physicalAddress;
	}
}

AuthInfo CV2::getSSOAuthInfo(const nlohmann::json &member, const std::string &redirectURL)
{
    // TODO: Redo this for CV2

	Metrics::db_get_sso_info++;
	// NONCE is just a random character string.  no semantic meaning
	// state = HMAC SHA384 of Nonce based on shared sso key
	// 
	// need nonce timeout in database? make sure it's used within X time
	// X is 5 minutes for now.  Make configurable later?
	//
	// how do we tell when a nonce is used? if auth_expiration_time is set
	std::string networkId = member["nwid"];
	std::string memberId = member["id"];


	char authenticationURL[4096] = {0};
	AuthInfo info;
	info.enabled = true;

	//if (memberId == "a10dccea52" && networkId == "8056c2e21c24673d") {
	//	fprintf(stderr, "invalid authinfo for grant's machine\n");
	//	info.version=1;
	//	return info;
	//}
	// fprintf(stderr, "PostgreSQL::updateMemberOnLoad: %s-%s\n", networkId.c_str(), memberId.c_str());
	std::shared_ptr<PostgresConnection> c;
	try {
// 		c = _pool->borrow();
// 		pqxx::work w(*c->c);

// 		char nonceBytes[16] = {0};
// 		std::string nonce = "";

// 		// check if the member exists first.
// 		pqxx::row count = w.exec_params1("SELECT count(id) FROM ztc_member WHERE id = $1 AND network_id = $2 AND deleted = false", memberId, networkId);
// 		if (count[0].as<int>() == 1) {
// 			// get active nonce, if exists.
// 			pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
// 				"WHERE network_id = $1 AND member_id = $2 "
// 				"AND ((NOW() AT TIME ZONE 'UTC') <= authentication_expiry_time) AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
// 				networkId, memberId);

// 			if (r.size() == 0) {
// 				// no active nonce.
// 				// find an unused nonce, if one exists.
// 				pqxx::result r = w.exec_params("SELECT nonce FROM ztc_sso_expiry "
// 					"WHERE network_id = $1 AND member_id = $2 "
// 					"AND authentication_expiry_time IS NULL AND ((NOW() AT TIME ZONE 'UTC') <= nonce_expiration)",
// 					networkId, memberId);

// 				if (r.size() == 1) {
// 					// we have an existing nonce.  Use it
// 					nonce = r.at(0)[0].as<std::string>();
// 					Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
// 				} else if (r.empty()) {
// 					// create a nonce
// 					Utils::getSecureRandom(nonceBytes, 16);
// 					char nonceBuf[64] = {0};
// 					Utils::hex(nonceBytes, sizeof(nonceBytes), nonceBuf);
// 					nonce = std::string(nonceBuf);

// 					pqxx::result ir = w.exec_params0("INSERT INTO ztc_sso_expiry "
// 						"(nonce, nonce_expiration, network_id, member_id) VALUES "
// 						"($1, TO_TIMESTAMP($2::double precision/1000), $3, $4)",
// 						nonce, OSUtils::now() + 300000, networkId, memberId);

// 					w.commit();
// 				}  else {
// 					// > 1 ?!?  Thats an error!
// 					fprintf(stderr, "> 1 unused nonce!\n");
// 					exit(6);
// 				}
// 			} else if (r.size() == 1) {
// 				nonce = r.at(0)[0].as<std::string>();
// 				Utils::unhex(nonce.c_str(), nonceBytes, sizeof(nonceBytes));
// 			} else {
// 				// more than 1 nonce in use?  Uhhh...
// 				fprintf(stderr, "> 1 nonce in use for network member?!?\n");
// 				exit(7);
// 			}

// 			r = w.exec_params(
// 				"SELECT oc.client_id, oc.authorization_endpoint, oc.issuer, oc.provider, oc.sso_impl_version "
// 				"FROM ztc_network AS n "
// 				"INNER JOIN ztc_org o "
// 				"  ON o.owner_id = n.owner_id "
// 			    "LEFT OUTER JOIN ztc_network_oidc_config noc "
// 				"  ON noc.network_id = n.id "
// 				"LEFT OUTER JOIN ztc_oidc_config oc "
// 				"  ON noc.client_id = oc.client_id AND oc.org_id = o.org_id "
// 				"WHERE n.id = $1 AND n.sso_enabled = true", networkId);
		
// 			std::string client_id = "";
// 			std::string authorization_endpoint = "";
// 			std::string issuer = "";
// 			std::string provider = "";
// 			uint64_t sso_version = 0;

// 			if (r.size() == 1) {
// 				client_id = r.at(0)[0].as<std::optional<std::string>>().value_or("");
// 				authorization_endpoint = r.at(0)[1].as<std::optional<std::string>>().value_or("");
// 				issuer = r.at(0)[2].as<std::optional<std::string>>().value_or("");
// 				provider = r.at(0)[3].as<std::optional<std::string>>().value_or("");
// 				sso_version = r.at(0)[4].as<std::optional<uint64_t>>().value_or(1);
// 			} else if (r.size() > 1) {
// 				fprintf(stderr, "ERROR: More than one auth endpoint for an organization?!?!? NetworkID: %s\n", networkId.c_str());
// 			} else {
// 				fprintf(stderr, "No client or auth endpoint?!?\n");
// 			}
		
// 			info.version = sso_version;
			
// 			// no catch all else because we don't actually care if no records exist here. just continue as normal.
// 			if ((!client_id.empty())&&(!authorization_endpoint.empty())) {
				
// 				uint8_t state[48];
// 				HMACSHA384(_ssoPsk, nonceBytes, sizeof(nonceBytes), state);
// 				char state_hex[256];
// 				Utils::hex(state, 48, state_hex);
				
// 				if (info.version == 0) {
// 					char url[2048] = {0};
// 					OSUtils::ztsnprintf(url, sizeof(authenticationURL),
// 						"%s?response_type=id_token&response_mode=form_post&scope=openid+email+profile&redirect_uri=%s&nonce=%s&state=%s&client_id=%s",
// 						authorization_endpoint.c_str(),
// 						url_encode(redirectURL).c_str(),
// 						nonce.c_str(),
// 						state_hex,
// 						client_id.c_str());
// 					info.authenticationURL = std::string(url);
// 				} else if (info.version == 1) {
// 					info.ssoClientID = client_id;
// 					info.issuerURL = issuer;
// 					info.ssoProvider = provider;
// 					info.ssoNonce = nonce;
// 					info.ssoState = std::string(state_hex) + "_" +networkId;
// 					info.centralAuthURL = redirectURL;
// #ifdef ZT_DEBUG
// 					fprintf(
// 						stderr,
// 						"ssoClientID: %s\nissuerURL: %s\nssoNonce: %s\nssoState: %s\ncentralAuthURL: %s\nprovider: %s\n",
// 						info.ssoClientID.c_str(),
// 						info.issuerURL.c_str(),
// 						info.ssoNonce.c_str(),
// 						info.ssoState.c_str(),
// 						info.centralAuthURL.c_str(),
// 						provider.c_str());
// #endif
// 				}
// 			}  else {
// 				fprintf(stderr, "client_id: %s\nauthorization_endpoint: %s\n", client_id.c_str(), authorization_endpoint.c_str());
// 			}
// 		}

// 		_pool->unborrow(c);
	} catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error updating member on load for network %s: %s\n", networkId.c_str(), e.what());
	}

	return info; //std::string(authenticationURL);
}

void CV2::initializeNetworks()
{
    try {
        // TODO: Update for CV2

        if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
        fprintf(stderr, "network init done\n");
    } catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing networks: %s\n", e.what());
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		exit(-1);
	}
}   

void CV2::initializeMembers()
{
    std::string memberId;
	std::string networkId;
    try {
        // TODO: Update for CV2
    
        if (++this->_ready == 2) {
			if (_waitNoticePrinted) {
				fprintf(stderr,"[%s] NOTICE: %.10llx controller PostgreSQL data download complete." ZT_EOL_S,_timestr(),(unsigned long long)_myAddress.toInt());
			}
			_readyLock.unlock();
		}
        fprintf(stderr, "member init done\n");
    } catch (std::exception &e) {
		fprintf(stderr, "ERROR: Error initializing member: %s-%s %s\n", networkId.c_str(), memberId.c_str(), e.what());
		exit(-1);
	}
}

void CV2::heartbeat()
{
    char publicId[1024];
	char hostnameTmp[1024];
	_myId.toString(false,publicId);
	if (gethostname(hostnameTmp, sizeof(hostnameTmp))!= 0) {
		hostnameTmp[0] = (char)0;
	} else {
		for (int i = 0; i < (int)sizeof(hostnameTmp); ++i) {
			if ((hostnameTmp[i] == '.')||(hostnameTmp[i] == 0)) {
				hostnameTmp[i] = (char)0;
				break;
			}
		}
	}
	const char *controllerId = _myAddressStr.c_str();
	const char *publicIdentity = publicId;
	const char *hostname = hostnameTmp;

    // TODO:  Update for CV2

    while (_run == 1) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    fprintf(stderr, "Exited heartbeat thread\n");
}

void CV2::membersDbWatcher() {
    auto c = _pool->borrow();

	std::string stream = "member_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());
	MemberNotificationReceiver m(this, *c->c, stream);

	while(_run == 1) {
		c->c->await_notification(5, 0);
	}

	_pool->unborrow(c);

    fprintf(stderr, "Exited membersDbWatcher\n");
}

void CV2::networksDbWatcher()
{
    std::string stream = "network_" + _myAddressStr;

	fprintf(stderr, "Listening to member stream: %s\n", stream.c_str());
	
	auto c = _pool->borrow();

	NetworkNotificationReceiver n(this, *c->c, stream);

	while(_run == 1) {
		c->c->await_notification(5,0);
	}

    _pool->unborrow(c);
    fprintf(stderr, "Exited networksDbWatcher\n");
}

void CV2::commitThread()
{
    // TODO: Update for CV2
}

void CV2::onlineNotificationThread() {
    waitForReady();

    _connected = 1;

    nlohmann::json jtmp1, jtmp2;
    while (_run == 1) {
        // TODO: Update for CV2
    }

    fprintf(stderr, "%s: Fell out of run loop in onlineNotificationThread\n", _myAddressStr.c_str());
	if (_run == 1) {
		fprintf(stderr, "ERROR: %s onlineNotificationThread should still be running! Exiting Controller.\n", _myAddressStr.c_str());
		exit(6);
	}
}
#endif // ZT_CONTROLLER_USE_LIBPQ