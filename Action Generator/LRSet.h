#include <vector>
#include "State.h"

class LRSet {
private:
    std::vector<State> states;
public:
    const std::vector<State> &getStates() const;

public:
    LRSet(const std::vector<State>& s);
    size_t numOfStates() { return states.size(); }
};
