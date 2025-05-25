#include "Database.hpp"

#include <iomanip>
#include <algorithm>

int main()
{
    Scope<ignite::Database> db = MakeScope<ignite::Database>();

    ignite::DBConnectionInfo connectionInfo;
    connectionInfo.server = "tcp://127.0.0.1:3306";
    connectionInfo.username = "root";

    std::string passwordStr;
    std::string schema;
    std::string sqlQuery;

    bool connected = false;
    while (true)
    {
        // trying to connect
        while (!connected)
        {
            std::cout << "Server: " << connectionInfo.server << "\n";
            std::cout << "Login as '" << connectionInfo.username << "'\n";
            std::cout << "password: ";
            std::getline(std::cin, connectionInfo.password);

            connected = db->Connect(connectionInfo);
        }

        std::cout << "database: ";
        std::getline(std::cin, schema);

        std::cout << "query: ";
        std::getline(std::cin, sqlQuery);

        try
        {
            sql::Connection *connection = db->GetConnection();
            connection->setSchema(schema);

            Scope<sql::Statement> statement(connection->createStatement());
            Scope<sql::ResultSet> result(statement->executeQuery(sqlQuery));
            sql::ResultSetMetaData *meta = result->getMetaData();

            std::vector<std::string> columnVector;
            int columnCount = meta->getColumnCount();

            for (int i = 1; i <= columnCount; ++i) // 1-based index!
            {
                columnVector.push_back(meta->getColumnLabel(i));
            }


            // read the result into memory
            std::vector<std::vector<std::string>> rows;
            while (result->next())
            {
                std::vector<std::string> row;
                for (const auto &columnLabel : columnVector)
                    row.push_back(result->getString(columnLabel));
                rows.push_back(row);
            }

            // calculate max width per column
            std::vector<size_t> columnWidths(columnVector.size());
            for (size_t col = 0; col < columnVector.size(); ++col)
            {
                columnWidths[col] = columnVector[col].length(); // start width header length
                for (const auto &row : rows)
                    columnWidths[col] = std::max(columnWidths[col], row[col].length());
            }

            std::cout << "\n\nQuery: '" << sqlQuery << "'\n";
            std::cout << "Executing...\n";

            // print header
            for (size_t col = 0; col < columnVector.size(); ++col)
            {
                std::cout << std::left << std::setw(columnWidths[col] + 4) << columnVector[col];
            }
            std::cout << "\n";

            // print separator
            for (size_t col = 0; col < columnVector.size(); ++col)
            {
                std::cout << std::left << std::setw(columnWidths[col] + 4) << std::setfill('-') << "";
            }
            std::cout << std::setfill(' ') << "\n"; // reset fill

            // print rows
            for (const auto &row : rows)
            {
                for (size_t col = 0; col < row.size(); ++col)
                {
                    std::cout << std::left << std::setw(columnWidths[col] + 4) << row[col];
                }

                std::cout << "\n";
            }
        }
        catch (sql::SQLException &e)
        {
            TRHOW_SQL_EXCEPTION(e);
        }
    }

    return EXIT_SUCCESS;
}