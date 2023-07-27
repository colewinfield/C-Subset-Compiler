#include "Production.h"

// Data class that holds the id, head, and body of a production from the Grammar
Production::Production(int i, const std::string& h, const std::string& b) {
    id = i;
    head = h;
    body = b;
}

const std::string &Production::getBody() const {
    return body;
}

int Production::getId() const {
    return id;
}

const std::string &Production::getHead() const {
    return head;
}

