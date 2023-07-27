#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include "tables.h"


std::map<char, int> getTerminalMap();
bool isBadToken(char token);

 /*******************************************************************************
 * main: This is the function that acts as the main while-loop that collects    *
 * the input from standard-in and parses it. To start the algorithm, a '0' is   *
 * added to the state stack, signifying the start state. Characters (excluding  *
 * white spaces) are read into the currentLetter. The currentLetter acts as the *
 * top of the character stack, so there is no need for two stacks or one comb-  *
 * ined stack. Each letter is tested against the action array. If it's an 'r',  *
 * then the stack must be continuously reduced until another action is avail-   *
 * able. After the reduce, depending on the state number and the current token, *
 * it is decided ifi 'ts an error or an accept state. If neither, then shift    *
 * the new state onto the stack. After the input is finished, there can still   *
 * be a full state stack. The parser must continue until the stack is empty     *
 * by reducing the remainder; it no longer shifts because there isn't any input.*
 *******************************************************************************/
int main() {
    std::stack<int> stateStack;
    stateStack.push(0);
    std::map<char, int> terminalIndex = getTerminalMap();

    char curr, currentLetter;
    while (std::cin >> curr && curr != '$') {
        currentLetter = curr;

        // check if the current token is in the set of tokens; if not, exit
        if (isBadToken(currentLetter)) {
            std::cerr << "Bad token: " << currentLetter << std::endl;
            exit(0);
        }

        int termIndex = terminalIndex[currentLetter];
        int currentState = stateStack.top();

        char act = action[currentState][termIndex];
        int actionNum = action_num[currentState][termIndex];

        while(act == 'r') {
            for (int i = 0; i < reduce_num[actionNum]; i++)
                stateStack.pop();

            std::cout << "reduce " << std::to_string(actionNum) << std::endl;
            currentState = stateStack.top();
            int lhs = reduce_lhs[actionNum];
            stateStack.push(go_to[currentState][lhs]);
            if (stateStack.size() >= 100) {
                std::cout << "\nStack overflow occurred.\n";
                exit(0);
            }
            currentState = stateStack.top();

            act = action[currentState][termIndex];
            actionNum = action_num[currentState][termIndex];
        }

        switch(act) {
            case 'e':
                std::cout << "\nError state on token '" << currentLetter
                << "' at state " << std::to_string(currentState) << ".\n";
                exit(0);
            case 'a':
                std::cout << "\nAccept state reached." << std::endl;
                exit(1);
            default:
                stateStack.push(actionNum);
                if (stateStack.size() >= 100) {
                    std::cout << "\nStack overflow occurred.\n";
                    exit(0);
                }
                currentState = stateStack.top();
                break;
        }

    }

    // signifiy that the current token is the end-of-input token
    // this is done because the main while-loop, which collected the tokens
    // has ended without issue. The remaining tokens on the stack must now
    // be reduced and checked if they're an error or within the accept. 
    // If the stack increases past 100 tokens, then a stack-overflow occurs. 
    currentLetter = '$';
    int termIndex = terminalIndex[currentLetter];
    int currentState = stateStack.top();

    char act = action[currentState][termIndex];
    int actionNum = action_num[currentState][termIndex];

    while(act == 'r') {
        for (int i = 0; i < reduce_num[actionNum]; i++)
            stateStack.pop();

        std::cout << "reduce " << std::to_string(actionNum) << std::endl;
        currentState = stateStack.top();
        int lhs = reduce_lhs[actionNum];
        stateStack.push(go_to[currentState][lhs]);
        if (stateStack.size() >= 100) {
            std::cout << "\nStack overflow occurred.\n";
            exit(0);
        }
        currentState = stateStack.top();

        act = action[currentState][termIndex];
        actionNum = action_num[currentState][termIndex];
    }

    if (act == 'e') {
        std::cout << "\nError state on token '" << currentLetter
                  << "' at state " << std::to_string(currentState) << ".\n";
        exit(0);
    } else {
        std::cout << "\nAccept state reached." << std::endl;
    }

}

 /*******************************************************************************
 * getTerminalMap: Creates a map of the terminals and their index into the      * 
 * token array for ease-of-use. This way, during algorithm, when a token is     *
 * needed for indexing, the map can return its proper index.                    *
 * Return: return the created (token, index) map.                               *
 *******************************************************************************/
std::map<char, int> getTerminalMap() {
    std::map<char, int> map;

    int index = 0;
    for (auto& term : tokens) {
        map[term] = index++;
    }

    return map;
}

 /*******************************************************************************
 * isBadToken: Checks whether or not the token passed into the parser is a bad  *
 * token. This is done by checking whether or not the token is in the tokens    *
 * array. If it's found, then it's a !badToken = good token.                    *                        
 *******************************************************************************/
bool isBadToken(char token) {
    for (auto& term : tokens) {
        if (term == token) return false;
    }

    return true;
}