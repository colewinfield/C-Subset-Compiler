#include "State.h"

// Each state has a list of items, a (grammar symbol, state) mapping, and a state number
State::State(const int num, const std::vector<Item> &i, const std::map<char, int> &gMap) {
    stateNum = num;
    items = i;
    gotoMap = gMap;
}

const std::map<char, int> &State::getGotoMap() const {
    return gotoMap;
}

int State::getStateNum() const {
    return stateNum;
}

const std::vector<Item> &State::getItems() const {
    return items;
}
