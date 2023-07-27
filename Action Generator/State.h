#include <vector>
#include <map>
#include "Item.h"


class State {
private:
    int stateNum;
public:
    int getStateNum() const;

private:
    std::vector<Item> items;
public:
    const std::vector<Item> &getItems() const;

private:
    std::map<char, int> gotoMap;
public:
    const std::map<char, int> &getGotoMap() const;

public:
    State(const int num, const std::vector<Item>& i, const std::map<char, int>& gMap);
};
