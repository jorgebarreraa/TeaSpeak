#include <iostream>
#include <src/Properties.h>
#include "src/misc/timer.h"

using namespace ts;
using namespace std;


int main() {
    //assert(property::impl::validateUnique());

    cout << property::describe(property::VIRTUALSERVER_HOST).name << endl;
    cout << property::find(property::PROP_TYPE_SERVER, "virtualserver_host").name<< endl;

    Properties props;
    props.register_property_type<property::InstanceProperties>();

    auto property = props[property::SERVERINSTANCE_QUERY_PORT];

    cout << "Port: " << props[property::SERVERINSTANCE_QUERY_PORT].as<string>() << endl;
    props[property::SERVERINSTANCE_QUERY_PORT] = "XX";
    cout << "Port: " << props[property::SERVERINSTANCE_QUERY_PORT].as<string>() << endl;
    props[property::SERVERINSTANCE_QUERY_PORT] = 2;
    cout << "Port: " << props[property::SERVERINSTANCE_QUERY_PORT].as<string>() << endl;
    cout << "Port: " << props[property::SERVERINSTANCE_QUERY_PORT].as<int32_t>() << endl;


    /*
    {
        assert(property::impl::validateInput("022222", property::TYPE_UNSIGNED_NUMBER) == true);
        assert(property::impl::validateInput("000000", property::TYPE_UNSIGNED_NUMBER) == true);
        assert(property::impl::validateInput("011011", property::TYPE_UNSIGNED_NUMBER) == true);

        assert(property::impl::validateInput("-022222", property::TYPE_UNSIGNED_NUMBER) == false);
        assert(property::impl::validateInput(" 00000", property::TYPE_UNSIGNED_NUMBER) == false);
        assert(property::impl::validateInput("01101.", property::TYPE_UNSIGNED_NUMBER) == false);

        assert(property::impl::validateInput("022222", property::TYPE_SIGNED_NUMBER) == true);
        assert(property::impl::validateInput("000000", property::TYPE_SIGNED_NUMBER) == true);
        assert(property::impl::validateInput("011011", property::TYPE_SIGNED_NUMBER) == true);
        assert(property::impl::validateInput("-022222", property::TYPE_SIGNED_NUMBER) == true);
        assert(property::impl::validateInput("-00000", property::TYPE_SIGNED_NUMBER) == true);

        assert(property::impl::validateInput("01101.", property::TYPE_SIGNED_NUMBER) == false);
        assert(property::impl::validateInput("01-101", property::TYPE_SIGNED_NUMBER) == false);

        assert(property::impl::validateInput("01101.", property::TYPE_FLOAT) == true);
        assert(property::impl::validateInput("-01101.", property::TYPE_FLOAT) == true);
        assert(property::impl::validateInput("-.1", property::TYPE_FLOAT) == true);
        assert(property::impl::validateInput("-2.22222", property::TYPE_FLOAT) == true);

        assert(property::impl::validateInput("01101.-2", property::TYPE_FLOAT) == false);
        assert(property::impl::validateInput("-011.01.", property::TYPE_FLOAT) == false);
        assert(property::impl::validateInput("-.1-", property::TYPE_FLOAT) == false);
        assert(property::impl::validateInput("-2.22222.2", property::TYPE_FLOAT) == false);
    }
     */

    {
        TIMING_START(timings);
        this_thread::sleep_for(chrono::milliseconds(100));
        TIMING_STEP(timings, "01");
        this_thread::sleep_for(chrono::milliseconds(200));
        TIMING_STEP(timings, "02");
        this_thread::sleep_for(chrono::milliseconds(50));
        TIMING_STEP(timings, "03");
        this_thread::sleep_for(chrono::milliseconds(150));
        cout << TIMING_FINISH(timings) << endl;
    }
}