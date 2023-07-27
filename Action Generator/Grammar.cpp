#include <set>
#include <algorithm>
#include "Grammar.h"

// Create the grammar that'll hold the production mapping
// The vector prods holds list of productions in order of input
Grammar::Grammar(
        const std::map<std::string, std::vector<std::string>> &pMap,
        const std::vector<Production> &prods) {
    productions = prods;
    prodMap = pMap;
    termArray = createTerminalArray();
    setUpTerminalData();
}

// From each of the productions, get the possible tokens. If the array
// already contains a terminal, then skip it. This will be in input order 
// for proper indexing into an array.
std::vector<char> Grammar::createTerminalArray() {
    std::vector<char> uniqueTerms;
    for (auto &prod: productions) {
        for (char letter: prod.getBody()) {
            if (!std::isupper(letter) && !containsGrammarSymbol(uniqueTerms, letter)) {
                uniqueTerms.push_back(letter);
            }
        }
    }

    uniqueTerms.push_back('$');
    return uniqueTerms;
}

bool Grammar::containsGrammarSymbol(const std::vector<char> &arr, char token) {
    return (std::find(arr.begin(), arr.end(), token) != arr.end());
}

// From the terminal array, create a (terminal, index) map 
// for further ease-of-use when indexing into the arrays.
void Grammar::setUpTerminalData() {
    int i = 0;
    while (i < termArray.size()) {
        termIndex[termArray[i]] = i;
        terminals.insert(termArray[i]);
        i++;
    }
}

const std::set<char> &Grammar::getTerminals() const {
    return terminals;
}

const std::vector<char> &Grammar::getTermArray() const {
    return termArray;
}

const std::map<char, int> &Grammar::getTermIndex() const {
    return termIndex;
}

// Checks whether or not the given terminal is in the terminal set
bool Grammar::isTerminal(char x) const {
    return terminals.find(x) != terminals.end();
}

// Return the terminal's index into the action tables.
char Grammar::getTerminal(int index) const {
    return termArray[index];
}

const std::vector<Production> &Grammar::getProductions() const {
    return productions;
}

// Returns which grammar # is associated with the item body from LR(0) set
int Grammar::getGrammarNumber(const std::string &itemBody) const {
    for (int i = 0; i < productions.size(); i++) {
        if (productions[i].getBody() == itemBody) return i;
    }

    return -1;
}

int Grammar::getTerminalIndex(const char termKey) const {
    return termIndex.at(termKey);
}

// Checks whether or not the item from LR(0) is valid, i.e, in the grammar
bool Grammar::isValidProduction(const std::string &item) const {
    for (auto& prod : productions) {
        std::string production = prod.getHead() + "->" + prod.getBody();
        if (item == production) return true;
    }

    return false;
}


