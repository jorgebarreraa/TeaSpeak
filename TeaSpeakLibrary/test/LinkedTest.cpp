#include <linked_helper.h>
#include <iostream>

using namespace std;

int main() {
    deque entries = {
            linked::create_entry(0, 1, 0),
            linked::create_entry(0, 2, 1),
            linked::create_entry(0, 3, 3),
            linked::create_entry(0, 4, 5),
            linked::create_entry(0, 5, 3),
    };

    deque<string> errors;
    auto head = linked::build_chain(entries, errors);
    if(!errors.empty()) {
        cout << "got " << errors.size() << " errors" << endl;
        for(const auto& error : errors)
            cout << "  " << error << endl;
    }

    while(head) {
        cout << " => " << head->entry_id << endl;
        head = head->next;
    }

    return 0;
}