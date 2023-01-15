# include <stdio.h>
# include "semutil.h"
# include "sem.h"
# include "sym.h"
# include "cc.h"
# include <string.h>
#include <stdlib.h>

#define MAXARGS 50
#define MAXLOCS 50
#define MAXLINES 80
#define MAXRECS 4096
#define MAXLABELS 50
#define MAXBLANKS 256
#define OPB 0
#define OP1 1
#define OP2 2

extern int localnum;                          /* number of local variables  */
extern char localtypes[MAXLOCS];              /* types of local variables   */
extern char localnames[MAXLOCS][MAXLINES];    /* names of local variables   */
extern int localwidths[MAXLOCS];              /* widths of local variables  */
extern int formalnum;                         /* number of formal arguments */
extern char formaltypes[MAXARGS];             /* types of formal arguments  */
extern char formalnames[MAXARGS][MAXLINES];   /* names of formal arguments  */
extern int ntmp;

int currentFunctionType;
int labelNum = 0;
int blankLabel = 0;
int labelScope = 1;

struct BackpatchLabel {
    char labelName[256];
    int labels[MAXBLANKS];
    int topLabel;
    int patched;
} typedef BackpatchLabel;

BackpatchLabel bpArr[MAXLABELS];
int currBpArr = 0;


/* Stack and Stack Helpers */
struct Stack {
    struct sem_rec *data[MAXRECS];
    int size;
} typedef Stack;

Stack continueStack;
Stack breakStack;

struct sem_rec *stackTop(Stack *stack);

void stackPush(Stack *stack, struct sem_rec *rec);

void stackPop(Stack *stack);

int nextBlankLabel() { return ++blankLabel; }

void dfsBackpatch(struct sem_rec *rec, int true, int trueM, int falseM);

struct sem_rec *mergeFalse(struct sem_rec *p1, struct sem_rec *p2);

int selectOp(char *op);

BackpatchLabel *findLabel(char *id);

void addLabel(int labelNo, char *id);

void setBackpatchedLabel(char *id);

void checkUnpatchedLabels();

void clearLabelArray();

void backpatchGotoLabels(BackpatchLabel *list, int label);

/*
 * backpatch - backpatch list of quadruples starting at p with k
 */
void backpatch(struct sem_rec *p, int k) {
    if (p)
        printf("B%d=L%d\n", p->s_place, k);
}

/*
 * bgnstmt - encountered the beginning of a statement
 */
void bgnstmt() {
    extern int lineno;
    printf("bgnstmt %d\n", lineno);
}

/*
 * call - procedure invocation
 * Check if it's been installed, otherwise, install it as T_INT function.
 * Go through args linked list (from cexprs) and print out each one as an argument.
 * Then print out instruction: number of args, temp args, and function name.
 */
struct sem_rec *call(char *f, struct sem_rec *args) {
    struct id_entry *idEntry;
    if ((idEntry = lookup(f, 0)) == NULL) {
        idEntry = install(f, 0);
        idEntry->i_type = T_INT;
        idEntry->i_scope = GLOBAL;
        idEntry->i_defined = 1;
    }

    int numArgs = 0;
    char argTemps[256];
    char *target = argTemps;
    while (args) {
        char type = args->s_mode == T_DOUBLE ? 'f' : 'i';
        printf("arg%c t%d\n", type, args->s_place);
        if (args->back.s_link)
            target += sprintf(target, "t%d ", args->s_place);
        else
            target += sprintf(target, "t%d", args->s_place);
        args = args->back.s_link;
        numArgs++;
    }

    int funcTemp = nexttemp();
    printf("t%d := global %s\n", funcTemp, idEntry->i_name);

    int temp = nexttemp();
    char funcType = idEntry->i_type == T_INT ? 'i' : 'f';

    if (numArgs > 0)
        printf("t%d := f%c t%d %d %s\n", temp, funcType, funcTemp, numArgs, argTemps);
    else
        printf("t%d := f%c t%d %d\n", temp, funcType, funcTemp, numArgs);

    labelScope = 1;
    return node(temp, idEntry->i_type, NULL, NULL);
}

/*
 * ccand - logical and
 * Author: Book algorithm: backpatch, then create a new node with merged lists.
 */
struct sem_rec *ccand(struct sem_rec *e1, int m, struct sem_rec *e2) {
    labelScope = 1;
    dfsBackpatch(e1->back.s_true, 1, m, -1);
    struct sem_rec *ret = node(
            0,
            0,
            e2->back.s_true,
            mergeFalse(e1->s_false, e2->s_false)
    );
    return ret;
}

/*
 * ccexpr - convert arithmetic expression to logical expression
 * When comparing, it just create an additional instruction of != 0.
 */
struct sem_rec *ccexpr(struct sem_rec *e) {
    labelScope = 1;
    return rel("!=", e, con("0"));
}

/*
 * ccnot - logical not
 * Author: Book algorithm: reverse the lists.
 */
struct sem_rec *ccnot(struct sem_rec *e) {
    labelScope = 1;
    return node(0, 0, e->s_false, e->back.s_true);
}

/*
 * ccor - logical or
 * Author: Book algorithm: backpatch, then create a new node with merged lists.
 */
struct sem_rec *ccor(struct sem_rec *e1, int m, struct sem_rec *e2) {
    labelScope = 1;
    dfsBackpatch(e1->s_false, 0, -1, m);
    struct sem_rec *ret = node(
            0,
            0,
            merge(e1->back.s_true, e2->back.s_true),
            e2->s_false
    );
    return ret;
}

/*
 * con - constant reference in an expression
 */
struct sem_rec *con(char *x) {
    struct id_entry *idEntry;
    if ((idEntry = lookup(x, 0)) == NULL) {
        idEntry = install(x, 0);
        idEntry->i_type = T_INT;
        idEntry->i_scope = GLOBAL;
        idEntry->i_defined = 1;
    }

    int temp = nexttemp();
    printf("t%d := %s\n", temp, x);
    labelScope = 1;
    return node(temp, idEntry->i_type, NULL, NULL);
}

/*
 * dobreak - break statement
 * Check if break was called within a loop, else error. If it were,
 * merge the new n() sem_rec with the top of the stack (most current break LL).
 */
void dobreak() {
    if (breakStack.size > 0)
        merge(stackTop(&breakStack), n());
    else
        yyerror(" break statement not inside of a loop");
}

/*
 * docontinue - continue statement
 * Check if continue was called within a loop. If not, print an error.
 */
void docontinue() {
    if (continueStack.size > 0)
        merge(stackTop(&continueStack), n());
    else
        yyerror(" continue statement not inside of a loop");
}

/*
 * dodo - do statement
 */
void dodo(int m1, int m2, struct sem_rec *e, int m3) {
    // if true, return to start of statements
    dfsBackpatch(e->back.s_true, 1, m1, -1);
    // else, false, so leave loop
    dfsBackpatch(e->s_false, 0, -1, m3);

    // if continue is used, backpatch with start of while-loop
    dfsBackpatch(stackTop(&continueStack)->back.s_link, 1, m2, -1);
    // if break is used, backpatch with exit of loop
    dfsBackpatch(stackTop(&breakStack)->back.s_link, 1, m3, -1);

    endloopscope(-1);
}

/*
 * dofor - for statement
 */
void dofor(int m1, struct sem_rec *e2, int m2, struct sem_rec *n1,
           int m3, struct sem_rec *n2, int m4) {
    // check if the expression is true, if so, go to m3; else, go to m4 (out of for-loop)
    dfsBackpatch(e2->back.s_true, 1, m3, -1);
    dfsBackpatch(e2->s_false, 0, -1, m4);

    // if continue is used, backpatch with start of for-loop
    dfsBackpatch(stackTop(&continueStack)->back.s_link, 1, m2, -1); // B13

    // first, return to increment the loop counter, then check if it's still true by going n2->m2
    backpatch(n1, m1); // B7
    backpatch(n2, m2); // B14

    // if break is used, backpatch with exit of loop
    dfsBackpatch(stackTop(&breakStack)->back.s_link, 1, m4, -1); // B10

    endloopscope(-1);
}



/*
 * doif - one-arm if statement
 */
void doif(struct sem_rec *e, int m1, int m2) {
    // Go through the semantic record, e, and connect the m1 and m2 to the true and false lists
    // product quadruples: BX = LA ... BY = LB
    dfsBackpatch(e->back.s_true, 1, m1, m2);
    dfsBackpatch(e->s_false, 0, m1, m2);
}

/*
 * Added API to merge false lists. Merge API in sym.c attaches list to the
 * back union instead of the false list, which created problems with my navigation of LL.
 */
struct sem_rec *mergeFalse(struct sem_rec *p1, struct sem_rec *p2) {
    struct sem_rec *p;

    if (p1 == NULL)
        return (p2);
    if (p2 == NULL)
        return (p1);
    for (p = p1; p->s_false; p = p->s_false);
    p->s_false = p2;
    return (p1);
}

/*
 * Go through a sem_rec linked list and backpatch all the links
 * depending on which branch.
 */
void dfsBackpatch(struct sem_rec *rec, int true, int trueM, int falseM) {
    if (rec == NULL) return;

    if (true) {
        backpatch(rec, trueM);
        dfsBackpatch(rec->back.s_true, 1, trueM, falseM);

    } else {
        backpatch(rec, falseM);
        dfsBackpatch(rec->s_false, 0, trueM, falseM);
    }
}

/*
 * doifelse - if then else statement
 */
void doifelse(struct sem_rec *e, int m1, struct sem_rec *n,
              int m2, int m3) {
    dfsBackpatch(e->back.s_true, 1, m1, -1);
    dfsBackpatch(e->s_false, 0, -1, m2);
    backpatch(n, m3);
}

/*
 * doret - return statement
 */
void doret(struct sem_rec *e) {
    // Check the return type of the function versus this.
    // Global can be set when fname is called. Convert to whatever the global is.
    labelScope = 1;
    if (e != NULL) {
        if (currentFunctionType == T_INT && e->s_mode != T_INT) {
            e = cast(e, currentFunctionType);
        } else if (currentFunctionType == T_DOUBLE && e->s_mode != T_DOUBLE) {
            e = cast(e, currentFunctionType);
        }

        char type = e->s_mode == T_INT ? 'i' : 'f';
        printf("ret%c t%d\n", type, e->s_place);
    } else {
        char type = currentFunctionType == T_INT ? 'i' : 'f';
        printf("ret%c\n", type);
    }

}

/*
 * dowhile - while statement
 */
void dowhile(int m1, struct sem_rec *e, int m2, struct sem_rec *n,
             int m3) {
    dfsBackpatch(e->back.s_true, 1, m2, -1);
    dfsBackpatch(e->s_false, 0, -1, m3);

    // return to check if cexpr is still true
    backpatch(n, m1);

    // if continue is used, backpatch with start of while-loop
    dfsBackpatch(stackTop(&continueStack)->back.s_link, 1, m1, -1);
    // if break is used, backpatch with exit of loop
    dfsBackpatch(stackTop(&breakStack)->back.s_link, 1, m3, -1);

    endloopscope(-1);
}

/*
 * endloopscope - end the scope for a loop
 */
void endloopscope(int m) {
    stackPop(&continueStack);
    stackPop(&breakStack);
    leaveblock();
}

/*
 * exprs - form a list of expressions
 * Return l (list) after adding sem_rec e to the back of the list
 * each time. Traverse the list of pointers until the end.
 * The list s_link will act as the list.
 * Uses: argument list in calls
 */
struct sem_rec *exprs(struct sem_rec *l, struct sem_rec *e) {
    labelScope = 1;
    struct sem_rec *ptr = l;
    while (ptr->back.s_link)
        ptr = ptr->back.s_link;
    ptr->back.s_link = e;
    return l;
}

/*
 * fhead - beginning of function body
 * Create the instructions for all of the formal and local types declared.
 */
void fhead(struct id_entry *p) {
    printf("func %s %d\n", p->i_name, p->i_type);

    int i;
    for (i = 0; i < formalnum; i++) {
        if (formaltypes[i] == 'i')
            printf("formal %s %d %d\n", formalnames[i],
                   (int) T_INT, tsize(T_INT));
        else
            printf("formal %s %d %d\n", formalnames[i],
                   (int) T_DOUBLE, tsize(T_DOUBLE));
    }

    for (i = 0; i < localnum; i++) {
        int type = localtypes[i] == 'i' ? T_INT : T_DOUBLE;

        if (localwidths[i] > 1)
            type |= T_ARRAY;

        if (localtypes[i] == 'i')
            printf("localloc %s %d %d\n", localnames[i],
                   (int) type, localwidths[i] * tsize(T_INT));
        else
            printf("localloc %s %d %d\n", localnames[i],
                   (int) type, localwidths[i] * tsize(T_DOUBLE));
    }

}

/*
 * fname - function declaration
 * Enter block for new function scope.
 * Then install the name if it hasn't been, otherwise,
 * print an error.
 */
struct id_entry *fname(int t, char *id) {
    enterblock();
    currentFunctionType = t;

    struct id_entry *idEntry;
    if ((idEntry = lookup(id, 0)) == NULL) {
        idEntry = install(id, 0);
    } else if (idEntry->i_defined) {
        yyerror("procedure previously defined");
    } else if (idEntry->i_type) {
        yyerror("procedure type does not match");
    }

    idEntry->i_type = t;
    idEntry->i_scope = GLOBAL;
    idEntry->i_defined = 1;
    return idEntry;
}

/*
 * ftail - end of function body
 * Reset localnum and formalnum for array access. Then check unpatched labels
 * and clear the array so that
 */
void ftail() {
    printf("fend\n");
    leaveblock();
    localnum = 0;
    formalnum = 0;

    // go through bpArr and check for undefined labels
    checkUnpatchedLabels();
    clearLabelArray();
}

/*
 * Go through the backpatch labels and check if any were created without being declared.
 * If so, print an error.
 */
void checkUnpatchedLabels() {
    int i;
    for (i = 0; i < MAXLABELS; i++) {
        if (bpArr[i].patched == -1)
            fprintf(stderr, "label %s referenced in goto, but never declared\n", bpArr[i].labelName);
    }
}

/*
 * When done with the function, clear out the label array. Reset everything to 0.
 */
void clearLabelArray() {
    int i;
    for (i = 0; i < MAXLABELS; i++) {
        bpArr[i].patched = 0;
        bpArr[i].topLabel = 0;

        int j;
        for (j = 0; j < MAXBLANKS; j++)
            bpArr[i].labels[j] = 0;

        strcpy(bpArr[i].labelName, "");
    }

    currBpArr = 0;
}

/*
 * id - variable reference
 * Checks if ID has been declared. If not, install it as an INT.
 * Otherwise, get its offset, scope, name, and type for
 * addressing. Then create instruction.
 */
struct sem_rec *id(char *x) {
    struct id_entry *idEntry;
    if ((idEntry = lookup(x, 0)) == NULL) {
        yyerror(" undeclared identifier");
        idEntry = install(x, -1);
        idEntry->i_type = T_INT;
        idEntry->i_scope = LOCAL;
        idEntry->i_defined = 1;
    }

    int offset = idEntry->i_offset;
    char scope[7];

    if (idEntry->i_scope == LOCAL)
        strcpy(scope, "local");
    else if (idEntry->i_scope == PARAM)
        strcpy(scope, "param");
    else
        strcpy(scope, "global");


    int temp = nexttemp();
    if (idEntry->i_scope != GLOBAL)
        printf("t%d := %s %s %d\n", temp, scope, idEntry->i_name, offset);
    else
        printf("t%d := %s %s\n", temp, scope, idEntry->i_name);

    labelScope = 1;
    return node(temp, idEntry->i_type | T_ADDR, NULL, NULL);
}

/*
 * sindex - subscript
 * Create instruction for accessing array; cast to INT if necessary.
 */
struct sem_rec *sindex(struct sem_rec *x, struct sem_rec *i) {
    x->s_mode &= ~(T_ADDR | T_ARRAY);

    if (i->s_mode == T_DOUBLE)
        cast(i, T_INT);

    char type = x->s_mode == T_INT ? 'i' : 'f';
    int temp = nexttemp();
    printf("t%d := t%d []%c t%d\n", temp, x->s_place, type, i->s_place);

    labelScope = 1;
    return node(temp, x->s_mode, NULL, NULL);
}

/*
 * dogoto - goto statement
 * Checks if label has been installed. If it has, print out its label when goto.
 * If not, 1) either create a temporary DS for it or 2) add to temp's list for later BP.
 */
void dogoto(char *id) {
    labelScope = 1;
    struct id_entry *idEntry;
    if ((idEntry = lookup(id, 0)) == NULL) {
        // do n()
        // find the backpatchlabel in the arr that matches id
        // if found: add n() to end of back list
        // if not, add to the array
        int label = nextBlankLabel();
        labelScope = 1;
        printf("br B%d\n", label);
        BackpatchLabel *list = findLabel(id);
        if (list == NULL) {
            addLabel(label, id);
        } else {
            list->labels[list->topLabel++] = label;
        }
    } else {
        printf("br L%d\n", idEntry->i_offset);
    }
}

/*
 * Create backpatch structure and add it to the array.
 */
void addLabel(int labelNo, char *id) {
    if (currBpArr >= MAXLABELS) {
        fprintf(stderr, "exceeded max limit of labels\n");
        return;
    }

    BackpatchLabel bpLabel;
    int i;
    for (i = 0; i < MAXBLANKS; i++) {
        bpLabel.labels[i] = 0;
    }
    bpLabel.topLabel = 0;
    strcpy(bpLabel.labelName, id);
    bpLabel.labels[bpLabel.topLabel++] = labelNo;
    bpLabel.patched = -1;
    bpArr[currBpArr++] = bpLabel;
}

/*
 * labeldcl - process a label declaration
 * Check if label is in the symbol table.
 * If it's not, create a label and install it then check if it has been
 * called elsewhere from goto. If it's been declared twice, don't
 * create another and error.
 */
void labeldcl(char *id) {
    /* you may assume the maximum number of C label declarations is 50 */
    struct id_entry *idEntry;
    if ((idEntry = lookup(id, 0)) == NULL) {
        idEntry = install(id, -1);
        idEntry->i_type = T_LBL;
        idEntry->i_scope = LOCAL;
        idEntry->i_defined = 1;

        int label = m();
        labelScope = 1;

        idEntry->i_offset = label;
        BackpatchLabel *list = findLabel(id);

        if (list != NULL) {
            backpatchGotoLabels(list, label);
            setBackpatchedLabel(id);
        }
    } else {
        yyerror(" identifier error previously declared");
    }

}

/*
 * BackpatchLabel Helpers
 * backpatchGotoLabels: backpatches the labels in the structure's array
 * setBackpatchedLabel: sets the flag for a backpatch structure, so it doesn't BP again
 * findLabel: find the structure in the array if it has been initialized
 */
void backpatchGotoLabels(BackpatchLabel *list, int label) {
    int i;
    for (i = 0; i < list->topLabel; i++) {
        printf("B%d=L%d\n", list->labels[i], label);
    }
}

void setBackpatchedLabel(char *id) {
    int i;
    for (i = 0; i < MAXLABELS; i++) {
        if (strcmp(id, bpArr[i].labelName) == 0) {
            bpArr[i].patched = 1;
            return;
        }
    }
}

BackpatchLabel *findLabel(char *id) {
    int i;
    for (i = 0; i < MAXLABELS; i++) {
        if (strcmp(id, bpArr[i].labelName) == 0) {
            return &bpArr[i];
        }
    }

    return NULL;
}

/*
 * m - generate label and return next temporary number
 * If there are nested reductions, like in the control-flow
 * productions, then having a semaphore-like labelScope avoids redundant
 * label production. It's unlocked when other reductions are made.
 */
int m() {
    if (labelScope == 1) {
        printf("label L%d\n", ++labelNum);
        labelScope = 0;
    }
    return labelNum;
}

/*
 * n - generate goto and return backpatch pointer
 */
struct sem_rec *n() {
    int label = nextBlankLabel();
    labelScope = 1;
    printf("br B%d\n", label);
    return node(label, 0, NULL, NULL);
}

/*
 * op1 - unary operators
 * Reference, bitwise complement, and negate y-rec. If y is a DOUBLE, then it should
 * be cast to an int before using bitwise complement. Otherwise, print off their
 * respective instructions.
 */
struct sem_rec *op1(char *op, struct sem_rec *y) {
    labelScope = 1;
    if (*op == '@' && !(y->s_mode & T_ARRAY)) {
        y->s_mode &= ~T_ADDR;
        int temp = nexttemp();
        char type = y->s_mode & T_INT ? 'i' : 'f';
        printf("t%d := @%c t%d\n", temp, type, y->s_place);
        return node(temp, y->s_mode, NULL, NULL);
    } else if (*op == '-') {
        y->s_mode &= ~T_ADDR;
        int temp = nexttemp();
        char type = y->s_mode & T_INT ? 'i' : 'f';
        printf("t%d := -%c t%d\n", temp, type, y->s_place);
        return node(temp, y->s_mode, NULL, NULL);
    } else if (*op == '~') {
        y->s_mode &= ~T_ADDR;
        if (y->s_mode == T_DOUBLE)
            y = cast(y, T_INT);
        int temp = nexttemp();
        char type = y->s_mode & T_INT ? 'i' : 'f';
        printf("t%d := ~%c t%d\n", temp, type, y->s_place);
        return node(temp, y->s_mode, NULL, NULL);
    }

    return node(y->s_place, y->s_mode, NULL, NULL);
}

/*
 * op2 - arithmetic operators
 * If any operator is T_DOUBLE, convert the other operator that isn't. Then print of the quadruple
 * depending on the switch cases.
 */
struct sem_rec *op2(char *op, struct sem_rec *x, struct sem_rec *y) {
    char type;
    labelScope = 1;

    if (x->s_mode != T_DOUBLE && y->s_mode == T_DOUBLE) {
        x = cast(x, T_DOUBLE);
        type = 'f';
    } else if (x->s_mode == T_DOUBLE && y->s_mode != T_DOUBLE) {
        y = cast(y, T_DOUBLE);
        type = 'f';
    } else {
        type = x->s_mode == T_DOUBLE ? 'f' : 'i';
    }

    int temp = nexttemp();
    switch (*op) {
        case '+':
            printf("t%d := t%d +%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '-':
            printf("t%d := t%d -%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '*':
            printf("t%d := t%d *%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '/':
            printf("t%d := t%d /%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '%':
            if (x->s_mode == T_DOUBLE || y->s_mode == T_DOUBLE) yyerror(" cannot %% floating-point values");
            printf("t%d := t%d %c%c t%d\n", temp, x->s_place, '%', type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        default:
            fprintf(stderr, "sem: op2 not implemented\n");
            return ((struct sem_rec *) NULL);
    }
}

/*
 * opb - bitwise operators
 * If any of the operators are double, they should be converted to T_INT if working with bit operators.
 * Then, select which operator based on the string comparison in switch.
 */
struct sem_rec *opb(char *op, struct sem_rec *x, struct sem_rec *y) {
    char type = 'i';
    labelScope = 1;

    if (y->s_mode == T_DOUBLE)
        y = cast(y, T_INT);

    if (x->s_mode == T_DOUBLE)
        x = cast(x, T_INT);

    int temp = nexttemp();
    switch (*op) {
        case '|':
            printf("t%d := t%d |%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '^':
            printf("t%d := t%d ^%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '&':
            printf("t%d := t%d &%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '<':
            printf("t%d := t%d <<%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        case '>':
            printf("t%d := t%d >>%c t%d\n", temp, x->s_place, type, y->s_place);
            return node(temp, x->s_mode, NULL, NULL);
        default:
            fprintf(stderr, "sem: opb not implemented\n");
            return ((struct sem_rec *) NULL);
    }
}

/*
 * rel - relational operators
 * Do a cast if any of the variables in a relation are double onto the other variable.
 * Then print out the quadruple after comparing the operation strings.
 */
struct sem_rec *rel(char *op, struct sem_rec *x, struct sem_rec *y) {
    char type;
    if (x->s_mode == T_INT && y->s_mode != T_INT) {
        x = cast(x, T_DOUBLE);
        type = 'f';
    } else if (x->s_mode == T_DOUBLE && y->s_mode != T_DOUBLE) {
        y = cast(y, T_DOUBLE);
        type = 'f';
    } else {
        type = x->s_mode == T_DOUBLE ? 'f' : 'i';
    }

    int temp = nexttemp();

    if (strcmp(op, "==") == 0) {
        printf("t%d := t%d ==%c t%d\n", temp, x->s_place, type, y->s_place);
    } else if (strcmp(op, "!=") == 0) {
        printf("t%d := t%d !=%c t%d\n", temp, x->s_place, type, y->s_place);
    } else if (strcmp(op, "<=") == 0) {
        printf("t%d := t%d <=%c t%d\n", temp, x->s_place, type, y->s_place);
    } else if (strcmp(op, ">=") == 0) {
        printf("t%d := t%d >=%c t%d\n", temp, x->s_place, type, y->s_place);
    } else if (strcmp(op, "<") == 0) {
        printf("t%d := t%d <%c t%d\n", temp, x->s_place, type, y->s_place);
    } else { // ">"
        printf("t%d := t%d >%c t%d\n", temp, x->s_place, type, y->s_place);
    }

    int trueLabel = nextBlankLabel(), falseLabel = nextBlankLabel();
    printf("bt t%d B%d\n", temp, trueLabel);
    printf("br B%d\n", falseLabel);

    struct sem_rec *returnValue = node(temp, x->s_mode, node(0, 0, NULL, NULL), node(0, 0, NULL, NULL));
    returnValue->back.s_true->s_place = trueLabel;
    returnValue->s_false->s_place = falseLabel;

    labelScope = 1;
    return returnValue;
}

int selectOp(char *op) {
    return (*op == '+' || *op == '-' || *op == '*' || *op == '/' || *op == '%') ? OP2 : OPB;
}

/*
 * set - assignment operators
 * Check if the operator is anything other than regular =. If it is, then do the op2 or bitwise op
 * on the variables. If the RHS differs from LHS, then cast the RHS to LHS' type before returning.
 */
struct sem_rec *set(char *op, struct sem_rec *x, struct sem_rec *y) {
    struct sem_rec *ptrX = x;
    int arrayFlag = 0;
    if (x->s_mode & T_ARRAY)
        arrayFlag = 1;

    if (*op != '\0') {
        ptrX->s_mode &= ~T_ARRAY;
        ptrX = op1("@", ptrX);
        if (arrayFlag) {
            ptrX->s_mode |= T_ARRAY;
            x->s_mode |= T_ARRAY;
        }

        if (selectOp(op) == OPB)
            y = opb(op, ptrX, y);
        else
            y = op2(op, ptrX, y);
    }

    x->s_mode &= ~T_ADDR;
    if (x->s_mode == T_INT && y->s_mode == T_DOUBLE)
        cast(y, T_INT);
    else if (x->s_mode == T_DOUBLE && y->s_mode != T_DOUBLE)
        cast(y, T_DOUBLE);
    else if ((x->s_mode & T_ARRAY) == T_ARRAY && y->s_mode == T_DOUBLE)
        cast(y, x->s_mode & ~T_ARRAY);

    int temp = nexttemp();
    char type;
    if ((x->s_mode) == (T_DOUBLE | T_ARRAY) || x->s_mode == T_DOUBLE)
        type = 'f';
    else
        type = 'i';
    printf("t%d := t%d =%c t%d\n", temp, x->s_place, type, y->s_place);
    labelScope = 1;
    return node(temp, x->s_mode, NULL, NULL);
}


/*
 * startloopscope - start the scope for a loop
 * Create a new continueStack and breakStack to hold all the nested
 * loop breaks and continues. Must enter a new block when entering loop scope.
 */
void startloopscope() {
    /* you may assume the maximum number of loops in a loop nest is 50 */
    enterblock();

    // create new continue list sem_rec
    stackPush(&continueStack, node(0, 0, NULL, NULL));
    stackPush(&breakStack, node(0, 0, NULL, NULL));
}

/*
 * string - generate code for a string
 * Add it to symbol table for later use.
 */
struct sem_rec *string(char *s) {
    struct id_entry *idEntry;
    if ((idEntry = lookup(s, 0)) == NULL) {
        idEntry = install(s, 0);
        idEntry->i_type = T_STR;
        idEntry->i_scope = GLOBAL;
        idEntry->i_defined = 1;
    }

    int temp = nexttemp();
    printf("t%d := %s\n", temp, s);
    labelScope = 1;
    return node(temp, idEntry->i_type, NULL, NULL);
}

/*
 * Convert passed in rec to the cast type.
 * Update the t-variable and return with t-type.
 */
struct sem_rec *cast(struct sem_rec *x, int t) {
    int temp = nexttemp();
    char type = t == T_INT ? 'i' : 'f';
    printf("t%d := cv%c t%d\n", temp, type, x->s_place);
    x->s_place = temp;
    x->s_mode = t;

    return x;
}

/* Stack Helper Functions */
/*
 * stackTop: retrieve top of stack
 * stackPush: add item to stack
 * stackPop: remove from top of stack
 */
struct sem_rec *stackTop(Stack *stack) {
    if (stack->size == 0) {
        fprintf(stderr, "Error: stack empty\n");
        return node(0, 0, NULL, NULL);
    }

    return stack->data[stack->size - 1];
}

void stackPush(Stack *stack, struct sem_rec *rec) {
    if (stack->size < MAXRECS)
        stack->data[stack->size++] = rec;
    else
        fprintf(stderr, "Error: stack full\n");
}

void stackPop(Stack *stack) {
    if (stack->size == 0)
        fprintf(stderr, "Error: stack empty\n");
    else
        stack->size--;
}

