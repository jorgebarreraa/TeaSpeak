#include <iostream>
#include <src/sql/SqlQuery.h>
#include <src/sql/sqlite/SqliteSQL.h>
#include <src/sql/mysql/MySQL.h>

#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/metadata.h>
#include <cppconn/driver.h>


using namespace std;
using namespace sql;

void testSql(){
    sql::sqlite::SqliteManager handle;
    auto res = handle.connect("test.sqlite");
    if(!res) {
        cerr << "Failed to open file. error: " << res << endl;
        return 0;
    }

    res = sql::command(&handle, "CREATE TABLE IF NOT EXISTS `test` (`key` TEXT, `value` TEXT);").execute();
    if(!res) {
        cerr << "Failed to execute command. error: " << res << endl;
        return 0;
    }

    res = sql::command(&handle, "SELECT * FROM `sqlite_master` WHERE `name` = :name", variable{":name", "test"}).query([](void*, int length, string* values, string* names) {
        cout << " | ";
        for(int i = 0; i < length; i++)
            cout << values[i] << " | ";
        cout << endl;
        return 0;
    }, (void*) nullptr);

    res = sql::command(&handle, "SELECT * FROM `sqlite_master` WHERE `name` = :name", variable{":name", "test"}).query<void>([](void*, int length, string* values, string* names) {
        cout << " | ";
        for(int i = 0; i < length; i++)
            cout << values[i] << " | ";
        cout << endl;
        return 0;
    }, nullptr);

    res = sql::command(&handle, "SELECT * FROM `sqlite_master` WHERE `name` = :name", variable{":name", "test"}).query<int>([](void*, int length, string* values, string* names) {
        cout << " | ";
        for(int i = 0; i < length; i++)
            cout << values[i] << " | ";
        cout << endl;
        return 0;
    }, (int*) nullptr);

    cout << "Res: " << res << endl;
}

int main(int, char**) {
    sql::Driver* driver = get_driver_instance();
    driver->connect(SQLString("tcp://127.0.0.1:3306"), SQLString("root"), SQLString("markus"));
    std::shared_ptr<sql::Connection> con(driver->connect("", "root", "markus"));
    if(!con->isValid()) {
        cerr << "Invalid connection!" << endl;
    }
    con->setSchema("test");
    //std::shared_ptr<sql::Statement> stmt(con->createStatement());
    /*
    sql::mysql::MySQLManager manager;
    manager.connect("hellop");
*/

    return 0;
}