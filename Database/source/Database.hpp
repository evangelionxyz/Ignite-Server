#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/resultset.h>

#define TRHOW_SQL_EXCEPTION(e) std::cerr << "SQL Error (" << e.getErrorCode() << "): " << e.what() << e.getSQLStateCStr() << std::endl

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T, typename... Args>
Ref<T> MakeRef(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename... Args>
Scope<T> MakeScope(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

namespace ignite {

    struct User
    {
        int id;
        std::string username;
        std::string email;
        std::string password;
    };

    struct DBConnectionInfo
    {
        std::string server;
        std::string username;
        std::string password;
    };

    class Database
    {
    public:
        Database()
        {
            try
            {
                m_Driver = get_driver_instance();
            }
            catch (sql::SQLException &e)
            {
                TRHOW_SQL_EXCEPTION(e);
            }
        }

        ~Database()
        {
            if (m_Connection)
                delete m_Connection;
        }


        bool Connect(const DBConnectionInfo &connectionInfo)
        {
            if (!m_Driver)
            {
                std::cerr << "Database: No SQL driver initialized\n";
                return false;
            }

            try
            {
                m_Connection = m_Driver->connect(connectionInfo.server, connectionInfo.username, connectionInfo.password);
                if (m_Connection->isValid())
                {
                    std::cout << "> Connected to database\n";
                    return true;
                }
            }
            catch (sql::SQLException &e)
            {
                TRHOW_SQL_EXCEPTION(e);
                return false;
            }

            return false;
        }

        sql::Connection *GetConnection() { return m_Connection; }

    private:
        sql::Connection *m_Connection = nullptr;
        sql::Driver *m_Driver = nullptr;
    };

}
