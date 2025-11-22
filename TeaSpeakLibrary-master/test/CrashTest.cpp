#include <cstdio>
#include <src/misc/time.h>
#include <src/log/LogUtils.h>
#include <iostream>

using namespace std;
using namespace std::chrono;

inline void test(const std::string& str, const nanoseconds& expected, bool expectResult) {
    string error;
    auto result = period::parse(str, error);
    if(expectResult) {
        if(!error.empty()) {
            cerr << "Test '" << str << "' failed. Got unexpected error: " << error << endl;
        } else {
            if(result == expected) {
                cout << "Test '" << str << "' Succeed. Got expected result" << endl;
            } else if(result != expected) {
                cout << "Test '" << str << "' failed. Got unexpected result: " << result.count() << ". Expected: " << expected.count() << endl;
            } else {
                cout << "Test '" << str << "' Succeed. Got expected result" << endl;
            }
        }
    } else {
        if(error.empty()) {
            cerr << "Test '" << str << "' failed. Expected error, but got success!" << endl;
        } else {
            cout << "Test '" << str << "' Succeed. Got expected error: " << error << endl;
        }
    }
}

int stack() {
    char buffer[1];
    for(register int i = 0; i <= 32; i++) {
        *(buffer + i) = 3;
        *(buffer - i) = 3;
    }
    return buffer[0];
}

int main(int argc, char* argv[]) {
    cout << "Stack: " << stack() << endl;
    terminal::install();
    auto config = make_shared<logger::LoggerConfig>();
    config->logfileLevel = spdlog::level::off;
    config->terminalLevel = spdlog::level::trace;
    logger::setup(config);

    test("1h", hours(1), true);
    test("2h", hours(2), true);
    test("30h", hours(30), true);

    test("30s", seconds(30), true);
    test("30s?", seconds(30), false);
    test("s", seconds(30), false);

    test("1m:30s", seconds(90), true);

    logMessageFmt(false, 0, "Hello {}", "A", "B", "C");
    logTrace("Hello World");
    logTrace(0, "Hello World");
    logTrace(0, "Hello World");
    logTrace(0, "Hello {:1}", "World");
    logTraceFmt(true, 0, "Hello {:2}", "World", "Dux");
    return 0;
}