#pragma once

#include <Common/PoolBase.h>
#include <Common/Priority.h>
#include <Client/Connection.h>
#include <IO/ConnectionTimeouts.h>
#include <Core/Settings.h>
#include <base/defines.h>

namespace DB
{

/** Interface for connection pools.
  *
  * Usage (using the usual `ConnectionPool` example)
  * ConnectionPool pool(...);
  *
  *    void thread()
  *    {
  *        auto connection = pool.get();
  *        connection->sendQuery(...);
  *    }
  */

class IConnectionPool : private boost::noncopyable
{
public:
    using Entry = PoolBase<Connection>::Entry;

    virtual ~IConnectionPool() = default;

    /// Selects the connection to work.
    virtual Entry get(const ConnectionTimeouts & timeouts) = 0;
    /// If force_connected is false, the client must manually ensure that returned connection is good.
    virtual Entry get(const ConnectionTimeouts & timeouts, /// NOLINT
                      const Settings & settings,
                      bool force_connected = true) = 0;

    virtual Priority getPriority() const { return Priority{1}; }
};

using ConnectionPoolPtr = std::shared_ptr<IConnectionPool>;
using ConnectionPoolPtrs = std::vector<ConnectionPoolPtr>;

/** A common connection pool, without fault tolerance.
  */
class ConnectionPool : public IConnectionPool, private PoolBase<Connection>
{
public:
    using Entry = IConnectionPool::Entry;
    using Base = PoolBase<Connection>;

    ConnectionPool(unsigned max_connections_,
            const String & host_,
            UInt16 port_,
            const String & default_database_,
            const String & user_,
            const String & password_,
            const String & quota_key_,
            const String & cluster_,
            const String & cluster_secret_,
            const String & client_name_,
            Protocol::Compression compression_,
            Protocol::Secure secure_,
            Priority priority_ = Priority{1})
       : Base(max_connections_,
        &Poco::Logger::get("ConnectionPool (" + host_ + ":" + toString(port_) + ")")),
        host(host_),
        port(port_),
        default_database(default_database_),
        user(user_),
        password(password_),
        quota_key(quota_key_),
        cluster(cluster_),
        cluster_secret(cluster_secret_),
        client_name(client_name_),
        compression(compression_),
        secure(secure_),
        priority(priority_)
    {
    }

    Entry get(const ConnectionTimeouts & timeouts) override
    {
        Entry entry = Base::get(-1);
        entry->forceConnected(timeouts);
        return entry;
    }

    Entry get(const ConnectionTimeouts & timeouts, /// NOLINT
              const Settings & settings,
              bool force_connected = true) override
    {
        Entry entry = Base::get(settings.connection_pool_max_wait_ms.totalMilliseconds());

        if (force_connected)
            entry->forceConnected(timeouts);

        return entry;
    }

    const std::string & getHost() const
    {
        return host;
    }
    std::string getDescription() const
    {
        return host + ":" + toString(port);
    }

    Priority getPriority() const override
    {
        return priority;
    }

protected:
    /** Creates a new object to put in the pool. */
    ConnectionPtr allocObject() override
    {
        return std::make_shared<Connection>(
            host, port,
            default_database, user, password, ssh::SSHKey(), quota_key,
            cluster, cluster_secret,
            client_name, compression, secure);
    }

private:
    String host;
    UInt16 port;
    String default_database;
    String user;
    String password;
    String quota_key;

    /// For inter-server authorization
    String cluster;
    String cluster_secret;

    String client_name;
    Protocol::Compression compression; /// Whether to compress data when interacting with the server.
    Protocol::Secure secure;           /// Whether to encrypt data when interacting with the server.
    Priority priority;                 /// priority from <remote_servers>
};

/**
 * Connection pool factory. Responsible for creating new connection pools and reuse existing ones.
 */
class ConnectionPoolFactory final : private boost::noncopyable
{
public:
    struct Key
    {
        unsigned max_connections;
        String host;
        UInt16 port;
        String default_database;
        String user;
        String password;
        String quota_key;
        String cluster;
        String cluster_secret;
        String client_name;
        Protocol::Compression compression;
        Protocol::Secure secure;
        Priority priority;
    };

    struct KeyHash
    {
        size_t operator()(const ConnectionPoolFactory::Key & k) const;
    };

    static ConnectionPoolFactory & instance();

    ConnectionPoolPtr
    get(unsigned max_connections,
        String host,
        UInt16 port,
        String default_database,
        String user,
        String password,
        String quota_key,
        String cluster,
        String cluster_secret,
        String client_name,
        Protocol::Compression compression,
        Protocol::Secure secure,
        Priority priority);
private:
    mutable std::mutex mutex;
    using ConnectionPoolWeakPtr = std::weak_ptr<IConnectionPool>;
    std::unordered_map<Key, ConnectionPoolWeakPtr, KeyHash> pools TSA_GUARDED_BY(mutex);
};

inline bool operator==(const ConnectionPoolFactory::Key & lhs, const ConnectionPoolFactory::Key & rhs)
{
    return lhs.max_connections == rhs.max_connections && lhs.host == rhs.host && lhs.port == rhs.port
        && lhs.default_database == rhs.default_database && lhs.user == rhs.user && lhs.password == rhs.password
        && lhs.quota_key == rhs.quota_key
        && lhs.cluster == rhs.cluster && lhs.cluster_secret == rhs.cluster_secret && lhs.client_name == rhs.client_name
        && lhs.compression == rhs.compression && lhs.secure == rhs.secure && lhs.priority == rhs.priority;
}

}
