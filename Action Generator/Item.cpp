#include "Item.h"

// This is a data object representing the items from LR(0) set
Item::Item(const char& h, const std::string& b) {
    head = h;
    body = b;
}

char Item::getHead() const {
    return head;
}

const std::string &Item::getBody() const {
    return body;
}
