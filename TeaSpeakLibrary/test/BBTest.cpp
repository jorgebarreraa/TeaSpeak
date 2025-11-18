#include <string>
#include <src/bbcode/bbcodes.h>
#include <src/misc/advanced_mutex.h>
#include <iostream>
#include <chrono>

#define TEST(message, tag) \
cout << "Testing '" << message << "' for " << tag << " results in " << bbcode::sloppy::has_tag(message, {tag}) << endl;

using namespace std;
using namespace std::chrono;
int main() {
    /*
    auto beg = system_clock::now();
    TEST("Hello [img]World[/img]", "img");
    TEST("[img]World[/img] Hello", "img");
    TEST("[img]World[img] Hello", "img");
    TEST("[img=https://www.teaspeak.de]World[/img] Hello", "img");
    TEST("\\[img]World[/img] Hello", "img");
    TEST("[img]World[img] Hello", "img");
    TEST("[img=https://www.teaspeak.de]World[/img] Hello", "img");
    TEST("\\[img]World[/img] Hello", "img");
    TEST("[img=https://www.teaspeak.de]World[/img] Hello", "img");
    TEST("\\[img]World[/img] Hello", "img");
    auto end = system_clock::now();
    cout << "Needed nanoseconds: " << duration_cast<nanoseconds>(end - beg).count() << endl;
     */
    return 0;
}