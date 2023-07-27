#include "Grammar.h"
#include "Follows.h"
#include "LRSet.h"

class TableGenerator {
private:
    size_t numStates, numTerms, numNonTerms, numProds;
    std::vector<std::vector<char>> action;
    std::vector<std::vector<int>> actionNum, gotoArr;
    std::vector<int> reduceLHS;
    std::vector<size_t> reduceNum;

    void initVectors();
    std::string generateTokenArr(const Grammar& grammar);
    std::string generateReduceLHS(const Grammar& grammar, const Follows& follows);
    std::string generateReduceNum(const Grammar& grammar);
    std::string generateTableString(const Grammar& grammar, const Follows& follows);
    std::string generateDefinitions();
    void createTable(const Grammar& grammar, const Follows& follows, const LRSet& lrSet);
public:
    TableGenerator(size_t numS, size_t numT, size_t numNT, size_t numP);
    void generateTable(const Grammar& grammar, const Follows& follows, const LRSet& lrSet);
};

