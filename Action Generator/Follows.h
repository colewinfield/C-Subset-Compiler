#include <set>
#include <vector>
#include <map>

class Follows {
    std::set<char> nonTerminals;
    std::map<char, std::vector<char>> followMap;
    std::map<char, int> nonTerminalIndex;
public:
    Follows(
            const std::set<char>& nonTerms,
            const std::map<char, std::vector<char>>& fMap,
            const std::map<char, int>& nonTIndex
            );
    bool isNonTerminal(char X) const;
    std::size_t getNumOfNonTerms() { return nonTerminals.size(); }
    int getNonTerminalIndex(const char& nonTerminal) const;
    std::vector<char> getFollowChars(const char X) const;
    std::vector<char> getNonTerminals() const;
};

