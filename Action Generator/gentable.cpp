#include <iostream>
#include <cstring>
#include <regex>
#include "constants.h"
#include "TableGenerator.h"

void getHeader(const std::string& header, const std::string& line);
void getStateHeader(const std::string& input);
int getDigit(const std::string& input);
Grammar getAugmentedGrammar();
Follows getFollows(const Grammar& grammar);
LRSet getSets(const Grammar& grammar, const Follows& follows);
Item getItem(const std::string& item, const Follows& follows, const Grammar& grammar);
void getGotoInfo(const std::string& input, std::map<char, int>& gotoMap, const Follows& follows, const Grammar& grammar);
void printExpectedError(const std::string& expected, const std::string& got);
std::pair<std::string, std::string> cleanProduction(const std::string& prod);

 /*******************************************************************************
 * main(): Gets the grammar from standard in. Then retrieves the Follows obj.   *
 * from standard in, which relies on the grammar obj. for error-checking. Then  *
 * the LR(0) set is created using both objects for error-checking. When all the *
 * input is finished, and the checks passed, the table is generated using       *
 * an instance of the TableGenerator class, which relies on all 3 inputs.       *
 *******************************************************************************/
int main() {

    Grammar grammar = getAugmentedGrammar();
    Follows follows = getFollows(grammar);
    LRSet set = getSets(grammar, follows);

    TableGenerator tableGenerator(
            set.numOfStates(),
            grammar.getNumTerms(),
            follows.getNumOfNonTerms(),
            grammar.getNumOfProds()
            );

    tableGenerator.generateTable(grammar, follows, set);

}

 /*******************************************************************************
 * getHeader(): For each section in the input, there is an Augmented Grammar,   *
 * Follows information, and an LR(0) Set. These have specific headers that are  *
 * defines in the constants.h file and have to correspond correctly to the input*
 * otherwise, there's an error and the program exits. The header (title and line*
 * ) must match the constant.                                                   *
 *******************************************************************************/
void getHeader(const std::string& header, const std::string& line) {
    std::string input;
    std::getline(std::cin, input);
    if (input != header) {
        printExpectedError(header, input);
        exit(0);
    }

    std::getline(std::cin, input);
    if (input != line) {
        printExpectedError(line, input);
        exit(0);
    }
}

 /*******************************************************************************
 * cleanProduction(): From the Grammar section, the line is cleaned and added   *
 * as a production object. If there's no arrow, the production is invalid. Also *
 * checks if the nonterminal symbols are proper (upper and alphabetic or ').    *
 * Checks that the production body only contains valid symbols.                 *
 *******************************************************************************/
std::pair<std::string, std::string> cleanProduction(const std::string& prod) {
    std::string arrow = "->";
    size_t locationOfArrow = prod.find(arrow);
    if (locationOfArrow == std::string::npos) {
        std::cerr << "Invalid production: " << prod << std::endl;
        exit(0);
    }

    std::string head = prod.substr(0, locationOfArrow);
    if (!(isupper(head[0]) && isalpha(head[0])) && head[0] != '\'') {
        std::cerr << "Invalid nonterm " << head[0] << std::endl;
        exit(0);
    }

    std::string body = prod.substr(locationOfArrow + 2, prod.length());
    size_t errorPos = std::string::npos;
    if (body.find(' ') != errorPos || body.find('\t') != errorPos ||
            body.find('$') != errorPos || body.find('\n') != errorPos ||
            body.find('\'') != errorPos) {
        std::cerr << "Invalid production: " << prod << std::endl;
        exit(0);
    }

    return {head, body};
}

 /*******************************************************************************
 * getAugmentedGrammar(): Goes through each line of the Grammar section and     *
 * ensures a valid production. If it's valid, a production object is passed back*
 * and stored into the production map and the production vector. These two DS   *
 * create the Grammar. This way, the productions are in indexed order.          *
 *******************************************************************************/
Grammar getAugmentedGrammar() {
    getHeader(AUGMENTED, AUGMENTED_LINE);

    std::string input;
    int i = 0;
    std::vector<Production> prods;
    std::map<std::string, std::vector<std::string>> prodMap;

    std::getline(std::cin, input);
    while (!input.empty()) {
        std::pair<std::string, std::string> pair = cleanProduction(input);
        std::string head, body;
        Production currProd(i, pair.first, pair.second);

        if (i == 0 && currProd.getHead() != "'") {
            std::cerr << "Invalid start symbol for Augmented Grammar" << std::endl;
            exit(0);
        }

        if (i > 0 && currProd.getHead() == "'") {
            std::cerr << "Invalid nonterm '" << std::endl;
            exit(0);
        }

        prods.push_back(currProd);
        prodMap[currProd.getHead()].push_back(currProd.getBody());
        std::getline(std::cin, input);
        i++;
    }

    return {prodMap, prods};
}

 /*******************************************************************************
 * getFollows(): Creates the Follows object based on the grammar and the input  *
 * from the Follows section. Also creates a set of the nonterminals for later.  *
 * The followsmap is a (char, vector<char>) map that holds the nonterminals     *
 * follow tokens. These are used to create the Follows data object.             *
 *******************************************************************************/
Follows getFollows(const Grammar& grammar) {
    getHeader(FOLLOWS, FOLLOWS_LINE);

    std::map<char, std::vector<char>> followsMap;
    std::set<char> nonTerminalSet;
    std::map<char, int> nonTermIndex;

    std::string input;
    std::getline(std::cin, input);
    int index = 0;
    while (!input.empty()) {
        char nonTerminal = input[0];
        nonTerminalSet.insert(nonTerminal);
        nonTermIndex[nonTerminal] = index++;
        for (int i = 1; i < input.size(); i++) {
            if (isspace(input[i])) continue;
            if ((std::isalpha(input[i]) && std::isupper(input[i])) || !grammar.isTerminal(input[i])) {
                std::cerr << "Invalid terminal: " << input[i] << std::endl;
                exit(0);
            }

            followsMap[nonTerminal].push_back(input[i]);
        }

        std::getline(std::cin, input);
    }

    std::vector<Production> prods = grammar.getProductions();
    for (int i = 1; i < prods.size(); i++) {
        char head = prods[i].getHead()[0];
        if (nonTerminalSet.find(head) == nonTerminalSet.end()) {
            std::cerr << "Invalid non-terminal " << head << std::endl;
            exit(0);
        }
    }

    return {nonTerminalSet, followsMap, nonTermIndex};
}


 /*******************************************************************************
 * getStateHeader(): Checks if each state header is valid. It should be comrp-  *
 * ised of a header like I%d:. RegEx matching is used for the digits.           *
 *******************************************************************************/
void getStateHeader(const std::string& input) {
    std::regex pat("I[0-9]+:");
    if (!std::regex_match(input, pat)) printExpectedError("I%d:", input);
}


 /*******************************************************************************
 * getDigit(): Gets a number or digit from a string. When the string reaches    *
 * a digit, it signifies the start of the number. The digit is then created     *
 * using the substring (start, end + 1) in stoi.                                *
 *******************************************************************************/
int getDigit(const std::string& input) {
    int start = -1; int end = -1;
    for (int i = 0; i < input.size(); i++) {
        if (std::isdigit(input[i]) && start == -1) {
            start = i;
        }

        if (std::isalpha(input[i]) && start != -1) {
            end = i;
        }
    }

    return stoi(input.substr(start, end));
}


 /*******************************************************************************
 * getItem(): Same as cleanProduction. Checks the validity of the item against  *
 * the known grammar productions. Checks each body against a valid grammar sym- *
 * bol and whether it exists in the productions. Also checks if the item as a   *
 * whole corresponds to a production from the grammar. If everything checks out,*
 * the item is sent back as a proper LR(0) item to be added to the LRSet.       *
 *******************************************************************************/
Item getItem(const std::string& item, const Follows& follows, const Grammar& grammar) {
    std::string arrow = "->";
    size_t locationOfArrow = item.find(arrow);
    if (locationOfArrow == std::string::npos) {
        std::cerr << "Invalid production: " << item << std::endl;
        exit(0);
    }

    if (!follows.isNonTerminal(item[0]) && item[0] != '\'') {
        std::cerr << "Invalid non-terminal " << item[0] << std::endl;
        exit(0);
    }

    for (size_t i = locationOfArrow + arrow.length(); i < item.size(); i++) {
        if (item[i] == '@') continue;
        if (!grammar.isTerminal(item[i]) && !follows.isNonTerminal(item[i])) {
            std::cerr << "Invalid grammar symbol " << item[i] << std::endl;
            exit(0);
        }
    }

    std::string itemCopy = item;
    itemCopy.erase(std::remove_if (itemCopy.begin (), itemCopy.end (), [](char c)
                  {
                      return c == '@';
                  }),
                   itemCopy.end ());

    if (!grammar.isValidProduction(itemCopy)) {
        std::cerr << "Could not find " << itemCopy << " in productions." << std::endl;
        exit(0);
    }

    return {item[0], item.substr(locationOfArrow + 2)};
}

 /*******************************************************************************
 * getGotoInfo(): Checks if the string is a valid pattern for the goto info.    *
 * Must adhere 'goto(%s)=I%d' pattern. Parses the line for the state number and *
 * the grammar symbol, and then adds it to the (symbol, state) goto mapping.    *
 *******************************************************************************/
void getGotoInfo(const std::string& input, std::map<char, int>& gotoMap,
                 const Follows& follows, const Grammar& grammar) {
    std::regex pat("goto\\(.\\)=I[0-9]+");
    if (!std::regex_match(input, pat)) printExpectedError("goto(%s)=I%d", input);

    size_t firstPar = input.find('(');
    char grammarSymbol = input[firstPar + 1];
    if (!grammar.isTerminal(grammarSymbol) && !follows.isNonTerminal(grammarSymbol)) {
        std::cerr << "Invalid grammar symbol " << grammarSymbol << " in goto" << std::endl;
        exit(0);
    }

    int gotoState = getDigit(input.substr(firstPar + 3));
    gotoMap[grammarSymbol] = gotoState;
}

 /*******************************************************************************
 * getSets(): Creates the LRSet based off the grammar and follows for error-    *
 * handling, and then creates the goto mapping and the vector of items needed   *
 * for proper construction of arrays.                                           *
 *******************************************************************************/
LRSet getSets(const Grammar& grammar, const Follows& follows) {
    getHeader(ITEMS, ITEMS_LINE);

    std::vector<State> states;

    std::string input, stateInput;
    std::getline(std::cin, input);
    while (!input.empty()) {
        getStateHeader(input);
        int stateNumber = getDigit(input);
        std::vector<Item> items;
        std::map<char, int> gotoInfo;

        std::getline(std::cin, stateInput);
        while (!stateInput.empty()) {
            std::vector<std::string> line;
            std::string str;
            std::istringstream iss(stateInput);
            while (iss >> str) line.push_back(str);
            if (line.size() > 2) {
                std::cerr << "Expected 2 arguments per line in State " <<
                          stateNumber << ", got " << line.size() << std::endl;
                exit(0);
            }

            Item item = getItem(line[0], follows, grammar);
            items.push_back(item);

            if (line.size() == 2) getGotoInfo(line[1], gotoInfo, follows, grammar);
            std::getline(std::cin, stateInput);
        }

        State currState(stateNumber, items, gotoInfo);
        states.push_back(currState);
        std::getline(std::cin, input);
    }

    return {states};
}

void printExpectedError(const std::string& expected, const std::string& got) {
    std::cerr << "Expected:\n" << "   " << expected << std::endl;
    std::cerr << "Got:\n" << "   " << got << std::endl;
    exit(0);
}






