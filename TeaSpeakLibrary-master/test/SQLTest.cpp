#include <iostream>
#include <src/sql/SqlQuery.h>
#include <src/sql/mysql/MySQL.h>
#include <src/sql/sqlite/SqliteSQL.h>
#include <src/misc/base64.h>
#include <src/log/LogUtils.h>

using namespace sql;
using namespace std;
using namespace std::chrono;


int f(int i);
void testCompiler() {
    /*
    command(nullptr, "Hello World", variable{"x", "y"}, variable{"x", "y"}, variable{"x", "y"}).query([](int length, std::string* values, std::string* names) -> int {
        return 0;
    });


    command(nullptr, "Hello World", variable{"x", "y"}).query([](int length, char** values, char** names) {

        return 0;
    });
     */


    /*
    //This should fail!
    command(nullptr, "Hello World", variable{"x", "y"}).query([](int length, char*** values, char** names) {

        return 0;
    });
    */

    /*
    command(nullptr, "Hello World", variable{"x", "y"}).query([](void*, int length, char** values, char** names) {

        return 0;
    }, (void*) nullptr);

    command(nullptr, "Hello World", variable{"x", "y"}).query<int>([](int*, int length, std::string* values, std::string* names) {

        return 0;
    }, nullptr);


    command(nullptr, "Hello World", variable{"x", "y"}).query([](int length, char** values, char** names) {

        return 0;
    });

    int d = 0;
    command((SqlManager*) nullptr, std::string("Hello World")).query<int>([](const int& data, int length, std::string* values, std::string* names) -> int {

        return 0;
    }, (const int&) d);

    auto cmd = command(nullptr, "", variable{"X", "Y"});
    cmd.value({"", "b"});
     */

    {
        //auto lambda = [](int, string*, string*) -> int { return false; };
        //command(nullptr, "").query_(lambda);
    }
    {
        auto lambda = [](int, string*, int, string*, string*) -> int { return false; };
        command(nullptr, "").query(lambda, 1, (string*) nullptr);
    }
    auto lambda = [](int, int, char**, char**) -> bool { return false; };
    command(nullptr, "").query(lambda, 1);

    {
        struct ProxyClass {
            void handle(int, int, string*, string*) {}
        } proxy;

        command(nullptr, "").query(&ProxyClass::handle, &proxy, 1);
    }
}

int main() {
    //testCompiler();

#if false
    sql::sqlite::SqliteManager manager;
    sql::result res{};

    manager.connect("test.sqlite");

    cout << command(&manager, "CREATE TABLE `test` (`key` TEXT, `value` TEXT)").execute() << endl;
    //cout << sql::command(&manager, "INSERT INTO `test` (`key`, `value`) VALUES (:key,:value)", variable{":key", "date"}, variable{":value", "test: " + to_string(system_clock::now().time_since_epoch().count())}).execute() << endl;
    int64_t result = 0;
    res = sql::command(&manager, "SELECT * FROM `test`").query([](int64_t& r, int64_t* a, int length, string* names, string* values) {
        cout << "Got entry: Key: " << names[0] << " Value: " << names[1] << endl;
        r = 1;
        *a = 2;
        return 0;
    }, result, &result);
    cout << " -> " << result << endl;
    assert(res);
#endif
#if false
    sql::mysql::MySQLManager manager;
    sql::result res{};
    assert(res = manager.connect("mysql://localhost:3306/teaspeak?userName=root&password=markus&connections=4"));
    /*
    assert(res = sql::command(&manager, "CREATE TABLE IF NOT EXISTS `test` (`key` TEXT, `value` TEXT)").execute());
    cout << "Old:" << endl;
    assert(res = sql::command(&manager, "SELECT * FROM `test`").query([](int length, string* names, string* values) {
        cout << "Got entry: Key: " << names[0] << " Value: " << names[1] << endl;
        return 0;
    }));
    assert(res = sql::command(&manager, "INSERT INTO `test` (`key`, `value`) VALUES (:key,:value)", variable{":key", "date"}, variable{":value", "test: " + to_string(system_clock::now().time_since_epoch().count())}).execute());
    cout << "New:" << endl;
    assert(res = sql::command(&manager, "SELECT * FROM `test`").query([](int length, string* names, string* values) {
        cout << "Got entry: Key: " << names[0] << " Value: " << names[1] << endl;
        return 0;
    }));
    */
    for(int i = 0; i < 80; i++) {
        assert(res = sql::command(&manager, "SHOW TABLES").query([](int length, string* names, string* values) {
            //cout << "Got entry: Key: " << names[0] << ":" << length << endl;
            return 0;
        }));
        //threads::self::sleep_for(seconds(1));
    }
    sql::command(&manager, "SHOW status;").query([](int length, string* values, string* names) {
        if(values[0].find("Com_stmt_") != -1)
            cout << values[0] << " => " << values[1] << endl;
    });

    cout << " ------------------- " << endl;
    for(int i = 0; i < 80; i++) {
        assert(res = sql::command(&manager, "SHOW TABLES").query([](int length, string* names, string* values) {
            //cout << "Got entry: Key: " << names[0] << ":" << length << endl;
            return 0;
        }));
        //threads::self::sleep_for(seconds(1));
    }
    sql::command(&manager, "SHOW status;").query([](int length, string* values, string* names) {
        if(values[0].find("Com_stmt_") != -1)
            cout << values[0] << " => " << values[1] << endl;
    });
#endif

#if false
    //{":hello", "world"}, {":yyyy", "xxx"}, {":numeric", 2}
    sql::command((SqlManager*) nullptr, std::string("SELECT *"), {":hello", "world"}, {":numeric", 2});
#endif


    sql::mysql::MySQLManager manager;
    manager.listener_disconnected = [](bool x){
        cout << "Disconnect: " << x << endl;
    };

    if(!manager.connect("mysql://localhost:3306/teaspeak?userName=root&password=markus&connections=1")) {
        cerr << "failed to connect" << endl;
        return 1;
    }

    /*
    auto result = sql::command(&manager,"INSERT INTO `level_miner` (`username`, `a`) VALUES (:username, :value)", variable{":username", "Hello"}, variable{":value", "TEST!"}).execute();
    if(!result) cout << result.fmtStr() << endl;

    while(true) {
        result = sql::command(&manager, "SELECT * FROM `level_miner`").query([](int length, std::string* values, std::string* names) {
            cout << "-- entry" << endl;
            for(int index = 0; index < length; index++) {
                cout << " " << names[index] << " => " << values[index] << endl;
            }
        });
        cout << result.fmtStr() << endl;
        this_thread::sleep_for(chrono::seconds(1));
    }
    */

    sql::command(&manager, "SELECT `cldbid`,`firstConnect`,`connections` FROM `clients` WHERE `serverId` = :sid AND `clientUid`=:uid LIMIT 1", variable{":sid", 0}, variable{":uid", "serveradmin"}).query([&](void* cl, int length, string* values, string* names){
        for (int index = 0; index < length; index++) {
            logTrace(0, "Reading client property from client database table. (Key: " + names[index] + ", Value: " + values[index] + ")");
        }
        return 0;
    }, (void*) nullptr);

    mysql_library_end();
}