#include <iostream>
#include <fstream>
#include "TableGenerator.h"


 /*******************************************************************************
 * TableGenerator Class: The main, public function is generateTable and is expo-*
 * to the other API. This creates the entire tables.h file. From the Gramamr,   *
 * Follows, and LRSet object that holds the data, they are parsed and put into  *
 * proper order based on the specifications of the output. The arrays are       *
 * created from vectors for modern API usage. They're initialized to the sizes  *
 * that are passed in as Constructor arguments. Then they're initialized with   *
 * 'e' and 0 and other starting state information. The 'e' and 0's that make-up *
 * the action arrays are gradually overriden by the data in the 3 objects.      *
 * Each table has its own significant header, as well.                          *
 *******************************************************************************/
TableGenerator::TableGenerator(size_t numS, size_t numT, size_t numNT, size_t numP) {
    numStates = numS;
    numTerms = numT;
    numNonTerms = numNT;
    numProds = numP;

    initVectors();
}

void TableGenerator::generateTable(const Grammar &grammar, const Follows &follows, const LRSet &lrSet) {
    createTable(grammar, follows, lrSet);
    std::ofstream tables("./tables.h");

    std::string definitions = generateDefinitions();
    std::cout << definitions;
    tables << definitions;

    std::string tableString = generateTableString(grammar, follows);
    std::cout << tableString;
    tables << tableString;

    std::string reduceNumString = generateReduceNum(grammar);
    std::cout << reduceNumString;
    tables << reduceNumString;

    std::string reduceLHSString = generateReduceLHS(grammar, follows);
    std::cout << reduceLHSString;
    tables << reduceLHSString;

    std::string tokenArrString = generateTokenArr(grammar);
    std::cout << tokenArrString;
    tables << tokenArrString;

}

std::string TableGenerator::generateTokenArr(const Grammar &grammar)  {
    std::string result = "static char tokens[NUM_TERMS] =\n /*";
    for (int i = 0; i < numTerms; i++) {
        if (i == 0) result.append("   ");
        else result.append("    ");
        result.append(std::to_string(i));
    }

    result.append("  */\n   { ");
    for (int i = 0; i < numTerms; i++) {
        result.append("'");
        result.push_back(grammar.getTerminal(i));
        result.append("'");
        if (i != numTerms - 1) result.append(",");
        result.append(" ");
    }

    result.append("};\n\n");
    return result;
}

std::string TableGenerator::generateReduceLHS(const Grammar &grammar, const Follows &follows)  {
    std::string result = "static int reduce_lhs[NUM_PRODS] =\n /*";
    for (int i = 1; i <= numProds; i++) {
        result.append("  " + std::to_string(i));
    }
    result.append(" */\n");
    result.append("   { 0,");

    std::vector<Production> prods = grammar.getProductions();
    for (int i = 1; i < numProds; i++) {
        int index = follows.getNonTerminalIndex(prods[i].getHead()[0]);
        reduceLHS.push_back(index);
        result.append(" " + std::to_string(index));
        if (i != numProds - 1) result.append(",");
    }

    result.append(" };\n\n");
    return result;
}

std::string TableGenerator::generateReduceNum(const Grammar &grammar) {
    std::string result = "static int reduce_num[NUM_PRODS] =\n /*";
    for (int i = 1; i <= numProds; i++) {
        result.append("  " + std::to_string(i));
    }
    result.append(" */\n");
    result.append("   {");

    std::vector<Production> prods = grammar.getProductions();
    for (int i = 0; i < numProds; i++) {
        size_t prodSize = prods[i].getBody().size();
        reduceNum.push_back(prodSize);
        result.append(" " + std::to_string(prodSize));
        if (i != numProds - 1) result.append(",");
    }

    result.append(" };\n\n");
    return result;
}

void TableGenerator::initVectors() {
    // Initialize action array
    for (int row = 0; row < numStates; row++) {
        std::vector<char> vec;
        for (int col = 0; col < numTerms; col++) {
            vec.push_back('e');
        }
        action.push_back(vec);
    }

    // Initialize actionNum array
    for (int row = 0; row < numStates; row++) {
        std::vector<int> vec;
        for (int col = 0; col < numTerms; col++) {
            vec.push_back(0);
        }
        actionNum.push_back(vec);
    }

    // Initialize go_to
    for (int row = 0; row < numStates; row++) {
        std::vector<int> vec;
        for (int col = 0; col < numNonTerms; col++) {
            vec.push_back(0);
        }
        gotoArr.push_back(vec);
    }
}

void TableGenerator::createTable(const Grammar &grammar, const Follows &follows, const LRSet &lrSet)  {
    // First, find the reductions and add them to the array
    // Go through each state and check the item; if an item has a
    // @ at the end of its body, then reduce based on equivalent grammar index
    int stateNum = 0;
    for (auto& state : lrSet.getStates()) {
        for (auto& item : state.getItems()) {
            std::string itemBody = item.getBody();
            if (itemBody[itemBody.size() - 1] == '@') {
                // If item's head is start symbol, accept, don't reduce
                if (item.getHead() == '\'') {
                    action[stateNum][grammar.getTerminalIndex('$')] = 'a';
                    continue;
                }

                int grammarNumber = grammar.getGrammarNumber(itemBody.substr(0,itemBody.size() - 1));
                if (grammarNumber == -1) {
                    std::cerr << "Production " << item.getHead() << "->" << itemBody << " not found" << std::endl;
                    exit(0);
                }

                char followsHead = item.getHead();
                for (auto& term : follows.getFollowChars(followsHead)) {
                    int termIndex = grammar.getTerminalIndex(term);
                    action[stateNum][termIndex] = 'r';
                    actionNum[stateNum][termIndex] = grammarNumber;
                }
            }
        }

        stateNum++;
    }

    stateNum = 0;
    for (auto& state : lrSet.getStates()) {
        for (auto const &pair: state.getGotoMap()) {
            if (grammar.isTerminal(pair.first)) {
                int termIndex = grammar.getTerminalIndex(pair.first);
                action[stateNum][termIndex] = 's';
                actionNum[stateNum][termIndex] = pair.second;
            } else if (follows.isNonTerminal(pair.first)) {
                int nonTermIndex = follows.getNonTerminalIndex(pair.first);
                gotoArr[stateNum][nonTermIndex] = pair.second;
            } else {
                std::cerr << "Invalid grammar symbol " << pair.first << std::endl;
                exit(0);
            }
        }

        stateNum++;
    }

}

std::string TableGenerator::generateTableString(const Grammar &grammar, const Follows &follows) {
    // Create header for action array
    std::string result = "static char action[NUM_STATES][NUM_TERMS] = {\n /*";
    std::vector<char> terms = grammar.getTermArray();
    for (int i = 0; i < numTerms; i++) {
        if (i == 0) result.append("   ");
        else result.append("    ");
        result.push_back(terms[i]);
    }
    result.append("   */\n");

    for (int row = 0; row < numStates; row++) {
        result.append("   {");
        for (int col = 0; col < numTerms; col++) {
            result.append(" '");
            result.push_back(action[row][col]);
            result.append("'");
            if (col != numTerms - 1) result.append(",");
        }
        if (row != numStates - 1) {
            if (std::to_string(row).length() >= 2) {
                result.append(" }, /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }, /*  " + std::to_string(row) + " */\n");
            }
        }
        else {
            if (std::to_string(row).length() >= 2) {
                result.append(" }  /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }  /*  " + std::to_string(row) + " */\n");
            }
        }
    }
    result.append("};\n\n");

    // Create header for action arrayNum
    result.append("static int action_num[NUM_STATES][NUM_TERMS] = {\n /*");
    for (int i = 0; i < numTerms; i++) {
        result.append("   ");
        result.push_back(terms[i]);
    }
    result.append("  */\n");

    for (int row = 0; row < numStates; row++) {
        result.append("   {");
        for (int col = 0; col < numTerms; col++) {
            if (std::to_string(actionNum[row][col]).length() >= 2) result.append(" ");
            else result.append("  ");
            result.append(std::to_string(actionNum[row][col]));
            if (col != numTerms - 1) result.append(",");
        }
        if (row != numStates - 1) {
            if (std::to_string(row).length() >= 2) {
                result.append(" }, /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }, /*  " + std::to_string(row) + " */\n");
            }
        }
        else {
            if (std::to_string(row).length() >= 2) {
                result.append(" }  /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }  /*  " + std::to_string(row) + " */\n");
            }
        }
    }
    result.append("};\n\n");


    // Create the goto array
    std::vector<char> nonTerms = follows.getNonTerminals();
    result.append("static int go_to[NUM_STATES][NUM_NONTERMS] = {\n /*");
    for (auto& nonT : nonTerms) {
        result.append("   ");
        result.push_back(nonT);
    }
    result.append("  */\n");

    for (int row = 0; row < numStates; row++) {
        result.append("   {");
        for (int col = 0; col < numNonTerms; col++) {
            if (std::to_string(gotoArr[row][col]).length() >= 2) result.append(" ");
            else result.append("  ");
            result.append(std::to_string(gotoArr[row][col]));
            if (col != numNonTerms - 1) result.append(",");
        }
        if (row != numStates - 1) {
            if (std::to_string(row).length() >= 2) {
                result.append(" }, /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }, /*  " + std::to_string(row) + " */\n");
            }
        }
        else {
            if (std::to_string(row).length() >= 2) {
                result.append(" }  /* " + std::to_string(row) + " */\n");
            } else {
                result.append(" }  /*  " + std::to_string(row) + " */\n");
            }
        }
    }
    result.append("};\n\n");

    return result;
}

std::string TableGenerator::generateDefinitions() {
    std::string result;
    result.append("#define NUM_STATES   " + std::to_string(numStates) + "\n");
    result.append("#define NUM_TERMS    " + std::to_string(numTerms) + "\n");
    result.append("#define NUM_NONTERMS " + std::to_string(numNonTerms) + "\n");
    result.append("#define NUM_PRODS    " + std::to_string(numProds) + "\n\n");
    return result;
}





