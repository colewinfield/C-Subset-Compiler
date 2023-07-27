#include <map>
#include <string>
#include <vector>
#include <set>
#include "Production.h"

class Grammar {
private:
    std::map<std::string, std::vector<std::string>> prodMap;
    std::vector<Production> productions;
public:
    const std::vector<Production> &getProductions() const;

private:
    std::set<char> nonTerminals;
    std::set<char> terminals;
    std::vector<char> termArray;
    std::map<char, int> termIndex;
    static bool containsGrammarSymbol(const std::vector<char>& arr, char token);
    void setUpTerminalData();

public:
    Grammar(const std::map<std::string, std::vector<std::string>>& pMap, const std::vector<Production>& prods);
    std::vector<char> createTerminalArray();
    const std::set<char> &getTerminals() const;
    const std::vector<char> &getTermArray() const;
    const std::map<char, int> &getTermIndex() const;
    bool isTerminal(char x) const;
    size_t getNumOfProds() { return productions.size(); };
    size_t getNumTerms() { return terminals.size(); };
    char getTerminal(int index) const;
    int getGrammarNumber(const std::string& itemBody) const;
    int getTerminalIndex(const char termKey) const;
    bool isValidProduction(const std::string& item) const;
};



