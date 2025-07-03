#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "PostgreSQL.hpp"

#include <nlohmann/json.hpp>

using namespace nlohmann;

using namespace ZeroTier;

MemberNotificationReceiver::MemberNotificationReceiver(DB *p, pqxx::connection &c, const std::string &channel)
	: pqxx::notification_receiver(c, channel)
	, _psql(p)
{
	fprintf(stderr, "initialize MemberNotificationReceiver\n");
}
	

void MemberNotificationReceiver::operator() (const std::string &payload, int packend_pid) {
	fprintf(stderr, "Member Notification received: %s\n", payload.c_str());
	Metrics::pgsql_mem_notification++;
	json tmp(json::parse(payload));
	json &ov = tmp["old_val"];
	json &nv = tmp["new_val"];
	json oldConfig, newConfig;
	if (ov.is_object()) oldConfig = ov;
	if (nv.is_object()) newConfig = nv;
	if (oldConfig.is_object() || newConfig.is_object()) {
		_psql->_memberChanged(oldConfig,newConfig,_psql->isReady());
		fprintf(stderr, "payload sent\n");
	}
}


NetworkNotificationReceiver::NetworkNotificationReceiver(DB *p, pqxx::connection &c, const std::string &channel)
	: pqxx::notification_receiver(c, channel)
	, _psql(p)
{
	fprintf(stderr, "initialize NetworkNotificationReceiver\n");
}

void NetworkNotificationReceiver::operator() (const std::string &payload, int packend_pid) {
	fprintf(stderr, "Network Notification received: %s\n", payload.c_str());
	Metrics::pgsql_net_notification++;
	json tmp(json::parse(payload));
	json &ov = tmp["old_val"];
	json &nv = tmp["new_val"];
	json oldConfig, newConfig;
	if (ov.is_object()) oldConfig = ov;
	if (nv.is_object()) newConfig = nv;
	if (oldConfig.is_object() || newConfig.is_object()) {
		_psql->_networkChanged(oldConfig,newConfig,_psql->isReady());
		fprintf(stderr, "payload sent\n");
	}
}

#endif