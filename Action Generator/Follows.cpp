#include "Follows.h"

// Creates the Follows object that'll hold the non-terminals, the follow
// map and the non-terminals' index. These are all used in further
// processing the tables (made from arrays). 
Follows::Follows(const std::set<char>& nonTerms,
                 const std::map<char, std::vector<char>>& fMap,
                 const std::map<char, int>& nonTIndex) {
    nonTerminals = nonTerms;
    followMap = fMap;
    nonTerminalIndex = nonTIndex;
}

// Checks if the passed terminal is a nonterminal
bool Follows::isNonTerminal(const char X) const {
    return nonTerminals.find(X) != nonTerminals.end();
}

// Returns the index of the terminal from the map
int Follows::getNonTerminalIndex(const char& nonTerminal) const {
    return nonTerminalIndex.at(nonTerminal);
}

// Returns the vector of characters that are the follows info for NT
std::vector<char> Follows::getFollowChars(const char X) const {
    return followMap.at(X);
}

// Creates a vector of the non-terminals in indexed-order
// the set is hashed, so it has no natural ordering.
std::vector<char> Follows::getNonTerminals() const{
    std::vector<char> nonTerms(nonTerminals.size());
    for (auto& nonTerm : nonTerminals) {
        int index = getNonTerminalIndex(nonTerm);
        nonTerms[index] = nonTerm;
    }

    return nonTerms;
}

