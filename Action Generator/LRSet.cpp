#include "LRSet.h"

// An LRSet is a data class that holds a vector of states (0 through n of LR(0))
LRSet::LRSet(const std::vector<State> &s) {
    states = s;
}

const std::vector<State> &LRSet::getStates() const {
    return states;
}
