#include <string>

class Item {
    char head;
    std::string body;
public:
    char getHead() const;

    const std::string &getBody() const;

public:
    Item(const char& h, const std::string& b);
};
