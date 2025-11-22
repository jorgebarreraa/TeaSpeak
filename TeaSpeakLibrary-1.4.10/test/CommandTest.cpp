#include <src/query/command3.h>
#include <codecvt>
#include <locale>
#include <string>
#include <cassert>
#include <src/misc/base64.h>
#include <sstream>
#include <src/License.h>
#include <functional>
#include <src/query/Command.h>
#include "PermissionManager.h"

#include "src/query/command_handler.h"
#include "src/query/command_constants.h"

using namespace std;
using namespace ts;
using namespace license::teamspeak;

template <class key_t, typename value_type_t>
using field = ts::command_handler::field<key_t, value_type_t>;

template <class key_t>
using trigger = ts::command_handler::trigger<key_t>;

void handleCommand(
        ts::command_parser& parser,
        const cconstants::return_code::optional& return_code,
        const field<tl("key_a"), int>& key_a,
        const field<tl("key_b"), string>::optional& key_b,
        const field<tl("key_c"), uint64_t>::optional::bulked& key_c,
        const trigger<tl("test")>& switch_test
) {
    if(key_a.value() < 10) {
        __asm__("nop");
    }
    __asm__("nop");
    auto c = key_b.has_value();

    __asm__("nop");
    (void) key_c.value(1);
    __asm__("nop");
    (void) return_code.value_or("XXX");
    cout << key_b.has_value() << endl;
    cout << "Return code: " << return_code.value_or("XXX") << endl;
    __asm__("nop");
}

command_result test() {
    return command_result{error::vs_critical};
}

command_result test2() {
    return command_result{permission::b_virtualserver_select_godmode};
}

command_result test3() {
    return command_result{error::vs_critical, "unknown error"};
}

void eval_test(command_result x) {
    if(x.is_detailed()) {
        cout << "Detailed!" << endl;
        x.release_data();
    } else {
        auto a = x.permission_id();
        auto b = x.error_code();
        cout << (void*) a << " - " << (void*) b << endl;
    }
}

void print_entries(const ts::command_parser& parser) {
    size_t index;
    std::string_view key{};
    std::string value{};
    for(const auto& bulk : parser.bulks()) {
        std::cout << "----\n";
        index = 0;
        while(bulk.next_entry(index, key, value)) {
            std::cout << "  " << key << " => " << value << "\n";
        }
    }
}

int main() {
    ts::command_handler::impl::field<tl("A"), int>::as_bulked<false> a;
    std::cout << "Optional: " << a.is_optional() << "\n";
    //test<"abs">();
    //for(const auto& error : avariableErrors)
    //    cout << error.name << " = " << hex << "0x" << error.errorId << "," << endl;

    //eval_test(test());
    //eval_test(test2());
    //eval_test(test3());
    /*
    ios_base::sync_with_stdio(false); // Avoids synchronization with C stdio on gcc
    // (either localize both or disable sync)

    wcout.imbue(locale("de_DE.ISO-8859-1")); // change default locale

    //░█▀▀▀█░░░░▄█░░░░░░░░░▄█░░░░░▄█░░
    const auto message = "\221\210\200\200\200\210\221\221\221\221\204\210\221\221\221\221\221\221\221\221\221\204\210\221\221\221\221\221\204\210\221\221";
    const auto auto_message = "░█▀▀▀█░░░░▄█░░░░░░░░░▄█░░░░░▄█░░";
    cout << " -> " << message << endl;
    cout << " -> " << utf8_check_is_valid(message) << endl;
    cout << " -> " << utf8_check_is_valid("░█▀▀▀█░░░░▄█░░░░░░░░░▄█░░░░░▄█░░") << endl;

    Command cmd = Command::parse("test -mapping test=░█▀▀▀█░░░░▄█░░░░░░░░░▄█░░░░░▄█░░_ -x");
    cout << "Build: " << cmd.build() << endl;
    cout << "X: " << cmd["test"] << endl;
    cout << " -> " << endl;
    for(const auto& e : cmd.parms())
        cout << e << endl;
     */

    /*
    auto handle = make_shared<ts::impl::command_value>();

    ts::command_entry entry(handle);
    entry = 255;
    cout << "Value: " << entry.as<int>() << endl;
    cout << "Value: " << entry.melt().as<uint32_t>() << endl;

    cout << "Str: " << entry.string() << endl;
    cout << "U8: " << (int) entry.melt().as<uint8_t>() << endl;
     */

    //register_function(handleCommand);

    /*
    cout << sizeof(command_result) << endl;
    ts::command cmd("notify");
    */

    auto command = ts::command_parser{"a a=x |x b=x | c=x | a=x"};
    if(!command.parse(true)) return 1;

    print_entries(command);
    auto next = command.next_bulk_containing("a", 0);
    std::cout << (next.has_value() ? *next : -1) << "\n";

    return 0;
    std::cout << "Command v3:\n";
    {
        auto command = ts::command_parser{"a a=c a=c2 -z | -? a=2 key_c=c"};
        if(!command.parse(true)) return 1;

        std::cout << command.bulk_count() << "\n";
        std::cout << command.bulk(1).value("a") << "\n";
        std::cout << command.has_switch("?") << "\n";
    };
    /*
     *
    cmd[0]["key_a"] = 2;
    cmd["b"] = 3;
    cmd["c"] = "Hello World";
    cmd["c"] = "Hello World" + string();
    cmd["c"] = 2;
    cmd.set_trigger("test");
    cmd.set_trigger("test2");
    cout << "Key_A => " << cmd[0]["key_a"].string() << endl;
     */

    auto result = ts::command_handler::describe_function(handleCommand);
    auto cmd_handler = ts::command_handler::parse_function(handleCommand);

    auto cmd = ts::command_parser{"a key_a=77 a=c a=c2 -z | -? key_c=3333 c=22 a=c return_code=234"};
    if(!cmd.parse(true)) return 1;

    cmd_handler->invoke(cmd);

    {
        ts::command_builder builder{"hello_world"};
        builder.put(0, "hello", "this is hello world");
        builder.put(1, "hello", "this is hello world1");
        builder.put(1, "hello", "this is hello world2");
        builder.put_unchecked(6, "hello", 22);
        std::cout << "Result: " << builder.build() << "\n";
    }
    //auto v = ts::descriptor::entry::bulked::val;
    return 0;
}