#include <string>

class Production {
    int id;
    std::string head;
    std::string body;

public:
    int getId() const;
    const std::string &getHead() const;
    const std::string &getBody() const;
    Production(int i, const std::string& h, const std::string& b);
};
