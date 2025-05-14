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

#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_POSTGRESQL_HPP
#define ZT_CONTROLLER_POSTGRESQL_HPP

#include "DB.hpp"
#include "ConnectionPool.hpp"
#include <pqxx/pqxx>
#include <memory>

namespace ZeroTier {

extern "C" {
typedef struct pg_conn PGconn;
}


class PostgresConnection : public Connection {
public:
	virtual ~PostgresConnection() {
	}

	std::shared_ptr<pqxx::connection> c;
	int a;
};


class PostgresConnFactory : public ConnectionFactory {
public:
	PostgresConnFactory(std::string &connString) 
		: m_connString(connString)
	{
	}

	virtual std::shared_ptr<Connection> create() {
		Metrics::conn_counter++;
		auto c = std::shared_ptr<PostgresConnection>(new PostgresConnection());
		c->c = std::make_shared<pqxx::connection>(m_connString);
		return std::static_pointer_cast<Connection>(c);
	}
private:
	std::string m_connString;
};

class MemberNotificationReceiver : public pqxx::notification_receiver {
public: 
	MemberNotificationReceiver(DB *p, pqxx::connection &c, const std::string &channel);
	virtual ~MemberNotificationReceiver() {
		fprintf(stderr, "MemberNotificationReceiver destroyed\n");
	}

	virtual void operator() (const std::string &payload, int backendPid);
private:
	DB *_psql;
};

class NetworkNotificationReceiver : public pqxx::notification_receiver {
public:
	NetworkNotificationReceiver(DB *p, pqxx::connection &c, const std::string &channel);
	virtual ~NetworkNotificationReceiver() {
		fprintf(stderr, "NetworkNotificationReceiver destroyed\n");
	};

	virtual void operator() (const std::string &payload, int packend_pid);
private:
	DB *_psql;
};

struct NodeOnlineRecord {
	uint64_t lastSeen;
	InetAddress physicalAddress;
	std::string osArch;
};

} // namespace ZeroTier

#endif // ZT_CONTROLLER_POSTGRESQL_HPP

#endif // ZT_CONTROLLER_USE_LIBPQ