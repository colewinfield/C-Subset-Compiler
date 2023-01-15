#include "quad.h"
#include "sym.h"
#include <iostream>
#include <regex>

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>


using namespace llvm;

typedef SmallVector<BasicBlock *, 16> BBList;
typedef SmallVector<Value *, 16> ValList;

/*
 * The order of the declaration of static variables matter.
 */
static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

/*
 * MY HELPER FUNCTION PROTOTYPES
 */

void processQuadLine(struct quadline *ptr, struct id_entry *fn);
void referenceParam(char *name, char *destination);
void referenceLocal(char *name, char *destination);
void referenceGlobal(char *name, char *destination);
void assign(char *name, const char *value);
void store(char *name, char *LHS, char *RHS);
void cast(char *destination, inst_type castType, char *address);
void call(struct quadline *ptr);
void referenceString(char *destination, char *str);
void doReturn(char *retTemp, struct id_entry *fn);
void binop(char *destinationTemp, char *left, const char *op, char *right);
void unary(char *destinationTemp, char *op, char *right);
void indexArray(char *destinationTemp, char *arr, char *index);
void branch(char *truthTemp, char *trueLabel, char *falseLabel, struct id_entry *fn);
void jump(char *destination, struct id_entry *fn);
void doNoop(struct id_entry *fn);
void doEmptyFuncEnd(struct id_entry *fn);


/* https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html#choosing-a-target */
void InitializeModuleAndPassManager() {

    auto TargetTriple = sys::getDefaultTargetTriple();
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
        errs() << Error;
        exit(1);
    }
    auto CPU = "generic";
    auto Features = "";
    TargetOptions opt;
    auto RM = Optional<Reloc::Model>();
    auto TargetMachine = Target->createTargetMachine(
            TargetTriple, CPU, Features, opt, RM);

    // Open a new module.
    TheModule = std::make_unique<Module>("QuadReader", TheContext);
    TheModule->setDataLayout(TargetMachine->createDataLayout());
    TheModule->setTargetTriple(TargetTriple);

    // We rely on printf function call
    char str[] = "printf";
    struct id_entry *iptr = install(str, GLOBAL);
    FunctionType *printfTy = FunctionType::get(Builder.getInt32Ty(),
                                               Builder.getInt8PtrTy(), true);
    iptr->v.f = Function::Create(printfTy, Function::ExternalLinkage,
                                 str, *TheModule);
    iptr->v.f->setCallingConv(CallingConv::C);
    iptr->v.f->getArg(0)->setName("f");

    // Create exit function
    char exitStr[] = "exit";
    auto exitPtr = install(exitStr, GLOBAL);
    FunctionType *exitTy = FunctionType::get(Builder.getVoidTy(), Builder.getInt32Ty(), false);
    exitPtr->v.f = Function::Create(exitTy, Function::ExternalLinkage,
                                 exitStr, *TheModule);
    exitPtr->v.f->setCallingConv(CallingConv::C);
    exitPtr->v.f->getArg(0)->setName("exitF");

    // Create getchar function
    char getCharStr[] = "getchar";
    auto getCharPtr = install(getCharStr, GLOBAL);
    FunctionType *getCharTy = FunctionType::get(Builder.getInt32Ty(),false);
    getCharPtr->v.f = Function::Create(getCharTy, Function::ExternalLinkage,
                                    getCharStr, *TheModule);
    getCharPtr->v.f->setCallingConv(CallingConv::C);
}

void OutputModule() {
    TheModule->print(outs(), nullptr);
}

static void createGlobal(struct id_entry *iptr) {
    if (iptr->i_type & T_ARRAY) {
        if (iptr->i_type & T_INT) {
            auto vecType = ArrayType::get(
                    Type::getInt32Ty(TheContext), iptr->i_numelem);
            iptr->u.ltype = vecType;
        } else {
            auto vecType = ArrayType::get(
                    Type::getDoubleTy(TheContext), iptr->i_numelem);
            iptr->u.ltype = vecType;
        }
    } else {
        if (iptr->i_type & T_INT)
            iptr->u.ltype = Builder.getInt32Ty();
        else
            iptr->u.ltype = Builder.getDoubleTy();
    }
    TheModule->getOrInsertGlobal(iptr->i_name, iptr->u.ltype);
    iptr->gvar = TheModule->getNamedGlobal(iptr->i_name);
    iptr->gvar->setLinkage(GlobalVariable::CommonLinkage);
    iptr->gvar->setAlignment(MaybeAlign(16));
    if (iptr->u.ltype->isArrayTy())
        iptr->gvar->setInitializer(ConstantAggregateZero::get(iptr->u.ltype));
}

static void createFunction(struct id_entry *fn, struct quadline **ptr) {
    std::vector<std::string> args;
    std::vector<llvm::Type *> typeVec;

    for (*ptr = (*ptr)->next; *ptr && (*ptr)->type == FORMAL_ALLOC; *ptr = (*ptr)->next) {
        auto iptr = lookup((*ptr)->items[1], PARAM);
        assert(iptr && "parameter is missing");
        if (iptr->i_type & T_INT) {
            iptr->u.ltype = Builder.getInt32Ty();
        } else {
            iptr->u.ltype = Builder.getDoubleTy();
        }
        typeVec.push_back(iptr->u.ltype);
        args.push_back(iptr->i_name);
    }

    if (fn->i_type & T_INT)
        fn->u.ftype = FunctionType::get(Builder.getInt32Ty(),
                                        typeVec, false);
    else
        fn->u.ftype = FunctionType::get(Builder.getDoubleTy(),
                                        typeVec, false);

    Function *F =
            Function::Create(fn->u.ftype, Function::ExternalLinkage,
                             fn->i_name, TheModule.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(args[Idx++]);
    fn->v.f = F;
}

static void allocaFormals(struct quadline **ptr, llvm::Function *fn) {
    for (; (*ptr != NULL) && ((*ptr)->type == FORMAL_ALLOC);
         *ptr = (*ptr)->next) {
        auto id_ptr = lookup((*ptr)->items[1], PARAM);
        assert(id_ptr && "local is missing");
        //Kaleidoscope addresses the initializer at this point, but we can't do that yet...
        id_ptr->v.v = Builder.CreateAlloca(
                id_ptr->u.ltype, nullptr, id_ptr->i_name);

        for (auto &Arg : fn->args()) {
            auto name = Arg.getName();
            if (strcmp(name.data(), id_ptr->i_name) == 0) {
                Builder.CreateStore(&Arg, id_ptr->v.v);
                break;
            }
        }
    }
}

/*
 * Slightly modified allocaLocals from template:
 * Original did not allocate arrays in local; they were allocated as simple
 * doubles or integers. Added a check for T_ARRAY and created the local
 * allocations appropriately for array-type.
 */
static void allocaLocals(struct quadline **ptr) {
    for (; (*ptr != NULL) && ((*ptr)->type == LOCAL_ALLOC);
         *ptr = (*ptr)->next) {
        auto id_ptr = lookup((*ptr)->items[1], LOCAL);
        assert(id_ptr && "local is missing");

        if (id_ptr->i_type & T_ARRAY) {     // addition: check for array type; if true, allocate as array
            auto arraySize = ConstantInt::get(Type::getInt32Ty(TheContext), id_ptr->i_numelem);
            if (id_ptr->i_type & T_INT) {
                auto vecType = ArrayType::get(
                        Type::getInt32Ty(TheContext), id_ptr->i_numelem);
                id_ptr->u.ltype = vecType;
                id_ptr->v.v = Builder.CreateAlloca(vecType, arraySize, id_ptr->i_name);
            } else {
                auto vecType = ArrayType::get(
                        Type::getDoubleTy(TheContext), id_ptr->i_numelem);
                id_ptr->u.ltype = vecType;
                id_ptr->v.v = Builder.CreateAlloca(vecType, arraySize, id_ptr->i_name);
            }

        } else {
            if (id_ptr->i_type & T_INT) {
                id_ptr->v.v = Builder.CreateAlloca(llvm::Type::getInt32Ty(
                                                           TheContext),
                                                   nullptr, id_ptr->i_name);
            }
            else {
                id_ptr->v.v = Builder.CreateAlloca(llvm::Type::getDoubleTy(
                                                           TheContext),
                                                   nullptr, id_ptr->i_name);
            }
        }
    }
}

/*
 * Slightly modified createLoad:
 * createLoad did not account for doubles when loading; had to add a
 * type check. Also had to determine if the variable was a global
 * or formal/param.
 */
void createLoad(struct quadline *ptr) {
    auto loadAddr = lookup(ptr->items[3], LOCAL);
    auto loadVal = install(ptr->items[0], LOCAL);
    assert(loadAddr && loadVal && "Load instruction generation fails");

    Value *theValue = loadAddr->i_scope == GLOBAL ? loadAddr->gvar : loadAddr->v.v;
    if (theValue->getType()->getPointerElementType()->isIntegerTy()) {
        loadVal->v.v = Builder.CreateLoad(llvm::Type::getInt32Ty(TheContext), theValue, ptr->items[0]);
        loadVal->u.ltype = Type::getInt32Ty(TheContext);
        loadVal->i_type = T_INT;
    } else {
        loadVal->v.v = Builder.CreateLoad(llvm::Type::getDoubleTy(TheContext), theValue, ptr->items[0]);
        loadVal->u.ltype = Type::getDoubleTy(TheContext);
        loadVal->i_type = T_DOUBLE;
    }
    return;
}

/*
 * createBitcode:
 * Processes a block of instructions at a time. While-loop until ptr
 * is null (as the main loop). Each quadline is an instruction line that
 * needs to be processed. Before the main loop, check if the block is
 * either 1) empty (ptr == null) or 2) only contains "FUNC_END".
 * In either case, they need to be handled with no-ops.
 */
void createBitcode(struct quadline *ptr, struct id_entry *fn) {
    // if ptr is null, there are no lines in the block
    // create a noop instruction (dummy alloca and br to successor)
    if (ptr == nullptr) {
        doNoop(fn);
        return;
    }

    // if the block doesn't contain any instructions (just fend),
    // then do a no-op and return
    if (strcmp(ptr->items[0], "fend") == 0) {
        doEmptyFuncEnd(fn);
        return;
    }

    // each time createBitcode is called, it's a block
    // process the block by going quadline-by-quadline
    struct quadline *lastQuad = ptr->blk->lineend;
    while (ptr) {
        processQuadLine(ptr, fn);
        if (ptr->type == BRANCH) {
            break;
        }

        ptr = ptr->next;
    }

    // if last instruction wasn't a block exit, make one to the next block
    if (lastQuad != nullptr && lastQuad->type != RETURN && lastQuad->type != JUMP && lastQuad->type != FUNC_END) {
        jump(lastQuad->blk->succs->ptr->label, fn);
    }

}

/*
 * references:
 * referenceParam: when a parameter is referenced, look up the name and get values/types.
 * referenceLocal: when a local is referenced, look up the name and get values/types.
 * referenceGlobal: get the global variable (gvar) and set it to the reference result;
 * v.v should also be set for funcs.
 */
void referenceParam(char *name, char *destination) {
    struct id_entry *address = lookup(name, PARAM);
    auto type = address->i_type == T_INT ? Type::getInt32Ty(TheContext) : Type::getDoubleTy(TheContext);
    auto value = address->v.v;

    struct id_entry *reference = install(destination, PARAM);
    reference->v.v = value;
    reference->u.ltype = type;
    reference->i_type = address->i_type;
    reference->i_scope = PARAM;
}

void referenceLocal(char *name, char *destination) {
    struct id_entry *address = lookup(name, LOCAL);
    auto type = address->i_type == T_INT ? Type::getInt32Ty(TheContext) : Type::getDoubleTy(TheContext);
    auto value = address->v.v;

    struct id_entry *reference = install(destination, LOCAL);
    reference->v.v = value;
    reference->u.ltype = type;
    reference->i_type = address->i_type;
    reference->i_scope = LOCAL;
}

void referenceGlobal(char *name, char *destination) {
    struct id_entry *address = lookup(name, GLOBAL);

    struct id_entry *reference = install(destination, GLOBAL);
    reference->i_type = address->i_type;
    reference->v.v = address->v.v;
    reference->gvar = address->gvar;
    reference->i_scope = GLOBAL;
}

/*
 * Assign a constant integer to an address. This is later
 * converted to a double if necessary.
 */
void assign(char *name, const char *value) {
    int realVal = std::stoi(value);
    struct id_entry *address = install(name, LOCAL);
    address->v.v = ConstantInt::get(Type::getInt32Ty(TheContext), realVal);
    address->i_type = T_INT;
    address->i_scope = LOCAL;
    address->u.ltype = Type::getInt32Ty(TheContext);
}

/*
 * Store the value of RHS and its type into the LHS.
 * Check if the values are globals; if they are,
 * the values should be gvar instead of v.v.
 */
void store(char *name, char *LHS, char *RHS) {
    struct id_entry *rhs = lookup(RHS, 0);
    struct id_entry *lhs = lookup(LHS, 0);
    Value *rhsValue = rhs->i_scope == GLOBAL ? rhs->gvar : rhs->v.v;
    Value *lhsValue = lhs->i_scope == GLOBAL ? lhs->gvar : lhs->v.v;

    struct id_entry *address = install(name, 0);
    Builder.CreateStore(rhsValue, lhsValue);
    address->v.v = rhsValue;
    address->i_scope = rhs->i_scope;
    address->i_type = rhs->i_type;
    address->u.ltype = rhs->u.ltype;
}

/*
 * Depending on the castType, convert the temp variable
 * into the appropriate double or integer. Then set its type to
 * int or double.
 */
void cast(char *destination, inst_type castType, char *address) {
    struct id_entry *addr = lookup(address, 0);
    struct id_entry *dest = install(destination, 0);
    if (castType == CVF) { // cast to cvf
        dest->u.ltype = Type::getDoubleTy(TheContext);
        addr->v.v = Builder.CreateSIToFP(addr->v.v, Type::getDoubleTy(TheContext));
    } else { // cast to cvi
        dest->u.ltype = Type::getInt32Ty(TheContext);
        addr->v.v = Builder.CreateFPToSI(addr->v.v, Type::getInt32Ty(TheContext));
    }

    dest->v.v = addr->v.v;
}

/*
 * Look-up if the string has been added to the global table. If it has been,
 * simply get a reference to the string's gvar. If it hasn't been added, add it as an array
 * of i8 chars. Convert each of the characters to i8 before adding them to an array for processing.
 * If there's an escape character, then process it as a single character before adding it to the
 * char-array.
 */
void referenceString(char *destinationTemp, char *str) {
    struct id_entry *myStr;
    struct id_entry *address = install(destinationTemp, GLOBAL);
    if ((myStr = lookup(str, 0)) == nullptr) {
        // myStr not in the symbol table, so include it
        // Also declare it as a global variable
        myStr = install(str, -1);

        // Create the char array
        auto charType = IntegerType::get(TheContext, 8);
        size_t strSize = strlen(str);
        std::vector<Constant *> chars;
        for (int i = 0; i < strSize; i++) {
            if ((i + 1) < strSize && str[i] == '\\' && str[i + 1] == 'n') {
                chars.push_back(ConstantInt::get(charType, '\n'));
                i++;
            } else if ((i + 1) < strSize && str[i] == '\\' && str[i + 1] == 't') {
                chars.push_back(ConstantInt::get(charType, '\t'));
                i++;
            } else if ((i + 1) < strSize && str[i] == '\\' && str[i + 1] == 'r') {
                chars.push_back(ConstantInt::get(charType, '\r'));
                i++;
            } else if ((i + 1) < strSize && str[i] == '\\' && str[i + 1] == '\\') {
                chars.push_back(ConstantInt::get(charType, '\\'));
                i++;
            } else {
                chars.push_back(ConstantInt::get(charType, str[i]));
            }
        }
        chars.push_back(ConstantInt::get(charType, 0));

        auto vecType = ArrayType::get(
                charType, chars.size());
        myStr->u.ltype = vecType;

        // Add the string to the Module for global reference
        auto globalVar = (GlobalVariable *) TheModule->getOrInsertGlobal("", vecType);
        globalVar->setInitializer(ConstantArray::get(vecType, chars));
        globalVar->setConstant(true);
        globalVar->setLinkage(GlobalVariable::PrivateLinkage);
        globalVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        globalVar->setAlignment(MaybeAlign(1));
        myStr->gvar = globalVar;

        address->gvar = myStr->gvar;
        address->v.v = myStr->gvar;
        address->u.ltype = myStr->u.ltype;
    } else {
        address->gvar = myStr->gvar;
        address->v.v = myStr->gvar;
        address->u.ltype = myStr->u.ltype;
    }
}

/*
 * Process the call quadline. Get the argument count, the destination,
 * and the reference to the function. With the argument count,
 * create an std::vec of args. Finally, check if the return
 * type of the function is void or not. Then pass in the args
 * to the call API.
 */
void call(struct quadline *ptr) {
    char *functionRef = ptr->items[3];
    struct id_entry *func = lookup(functionRef, GLOBAL);

    char *tempDestination = ptr->items[0];
    struct id_entry *address = install(tempDestination, GLOBAL);

    char *argCount = ptr->items[4];
    int numArgs = std::stoi(argCount);
    int index = 5;

    std::vector<Value *> args;
    while (numArgs > 0) {
        auto temp = lookup(ptr->items[index++], 0);
        if (temp->v.v && temp->v.v->getType()->isPointerTy() &&
            temp->v.v->getType()->getPointerElementType()->isArrayTy()) {
            Value *i32zero = ConstantInt::get(TheContext, APInt(32, 0));
            Value *indices[2] = {i32zero, i32zero};
            args.push_back(Builder.CreateInBoundsGEP(temp->v.v->getType()->getPointerElementType(), temp->v.v, indices));
        } else {
            args.push_back(temp->v.v);
        }

        numArgs--;
    }

    // check if the function is void or not
    if (func->v.f->getReturnType()->isVoidTy()) {
        address->v.v = Builder.CreateCall(func->v.f, args);
        address->u.ltype = func->v.f->getReturnType();
    } else {
        address->v.v = Builder.CreateCall(func->v.f, args, tempDestination);
        address->u.ltype = func->v.f->getReturnType();
    }
}

/*
 * Simply create a return instruction. If it's an auto-generated
 * return (because there wasn't one), CSEM auto-creates a reti;
 * this is bothersome if the function is a double. Thus, there's
 * an extra conversion check in return.
 */
void doReturn(char *retTemp, struct id_entry *fn) {
    auto retValue = lookup(retTemp, 0);

    if (fn->v.f->getReturnType() != retValue->v.v->getType()) {
        if (fn->v.f->getReturnType()->isDoubleTy())
            retValue->v.v = Builder.CreateSIToFP(retValue->v.v, Type::getDoubleTy(TheContext));
        else
            retValue->v.v = Builder.CreateFPToSI(retValue->v.v, Type::getInt32Ty(TheContext));
    }

    Builder.CreateRet(retValue->v.v);
}

/*
 * Depending on the operation, either do a negation or a bitwise
 * complement. First, check if the value is an integer or a double
 * before doing negation. Bitwise-NOT should not be a double. Adjust result's type.
 */
void unary(char *destinationTemp, char *op, char *right) {
    auto rightOp = lookup(right, 0);
    auto result = install(destinationTemp, LOCAL);

    if (*op == '-') {
        if (rightOp->u.ltype->isIntegerTy())
            result->v.v = Builder.CreateNeg(rightOp->v.v, "");
        else
            result->v.v = Builder.CreateFNeg(rightOp->v.v, "");
    } else if (*op == '~') {
        result->v.v = Builder.CreateNot(rightOp->v.v, "");
    }

    result->u.ltype = rightOp->u.ltype;
}

/*
 * Check if the op is a single-character operation with the switch before
 * checking if it's a left- or right-shift or one of the relational operators
 * (<, >, <=, >=, !=, ==).
 */
void binop(char *destinationTemp, char *left, const char *op, char *right) {
    auto leftOp = lookup(left, 0);
    auto rightOp = lookup(right, 0);
    auto result = install(destinationTemp, LOCAL);

    switch(*op) {
        case '+':
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateAdd(leftOp->v.v, rightOp->v.v, "");
            else
                result->v.v = Builder.CreateFAdd(leftOp->v.v, rightOp->v.v, "");
            break;
        case '-':
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateSub(leftOp->v.v, rightOp->v.v, "");
            else
                result->v.v = Builder.CreateFSub(leftOp->v.v, rightOp->v.v, "");
            break;
        case '*':
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateMul(leftOp->v.v, rightOp->v.v, "");
            else
                result->v.v = Builder.CreateFMul(leftOp->v.v, rightOp->v.v, "");
            break;
        case '/':
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateSDiv(leftOp->v.v, rightOp->v.v, "");
            else
                result->v.v = Builder.CreateFDiv(leftOp->v.v, rightOp->v.v, "");
            break;
        case '%':
            result->v.v = Builder.CreateSRem(leftOp->v.v, rightOp->v.v, "");
            break;
        case '&':
            result->v.v = Builder.CreateAnd(leftOp->v.v, rightOp->v.v, "");
            break;
        case '|':
            result->v.v = Builder.CreateOr(leftOp->v.v, rightOp->v.v, "");
            break;
        case '^':
            result->v.v = Builder.CreateXor(leftOp->v.v, rightOp->v.v, "");
            break;
        default:
            break;
    }

    /*
     * Comparator stuff and left-shift and right-shift
     */
    if (op[0] == '<') {
        if (op[1] == '<') {         // do left-shift (T_INT only)
            result->v.v = Builder.CreateShl(leftOp->v.v, rightOp->v.v, "");
        } else if (op[1] == '=') {  // do <= (LE)
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateICmpSLE(leftOp->v.v, rightOp->v.v, destinationTemp);
            else
                result->v.v = Builder.CreateFCmpOLE(leftOp->v.v, rightOp->v.v, destinationTemp);
        } else {                    // do regular less than (because the other two cases failed)
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateICmpSLT(leftOp->v.v, rightOp->v.v, destinationTemp);
            else
                result->v.v = Builder.CreateFCmpOLT(leftOp->v.v, rightOp->v.v, destinationTemp);
        }
    }

    if (op[0] == '>') {
        if (op[1] == '>') {         // do right-shift (T_INT)
            result->v.v = Builder.CreateAShr(leftOp->v.v, rightOp->v.v, "");
        } else if (op[1] == '=') {  // do >= (GE)
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateICmpSGE(leftOp->v.v, rightOp->v.v, destinationTemp);
            else
                result->v.v = Builder.CreateFCmpOGE(leftOp->v.v, rightOp->v.v, destinationTemp);
        } else {                    // do regular > (GT), other cases false
            if (leftOp->u.ltype->isIntegerTy())
                result->v.v = Builder.CreateICmpSGT(leftOp->v.v, rightOp->v.v, destinationTemp);
            else
                result->v.v = Builder.CreateFCmpOGT(leftOp->v.v, rightOp->v.v, destinationTemp);
        }
    }

    if (op[0] == '=' && op[1] == '=') {     // equal comparator
        if (leftOp->u.ltype->isIntegerTy())
            result->v.v = Builder.CreateICmpEQ(leftOp->v.v, rightOp->v.v, destinationTemp);
        else
            result->v.v = Builder.CreateFCmpOEQ(leftOp->v.v, rightOp->v.v, destinationTemp);
    }

    if (op[0] == '!' && op[1] == '=') {     // NE comparator
        if (leftOp->u.ltype->isIntegerTy())
            result->v.v = Builder.CreateICmpNE(leftOp->v.v, rightOp->v.v, destinationTemp);
        else
            result->v.v = Builder.CreateFCmpONE(leftOp->v.v, rightOp->v.v, destinationTemp);
    }

    result->u.ltype = leftOp->u.ltype;
}

/*
 * Determine if the array is a global-var or a param/local.
 * Then create a GEP to index into the array. The indices should consist
 * of a constant 32-bit int and the index-pointer's value. Don't give
 * the GEP a name to automatically produce a @x.
 */
void indexArray(char *destinationTemp, char *arr, char *index) {
    auto array = lookup(arr, 0);
    auto iPtr = lookup(index, 0);
    auto result = install(destinationTemp, LOCAL);

    Value *i32zero = ConstantInt::get(TheContext, APInt(32, 0));
    Value *indices[2] = {i32zero, iPtr->v.v};
    Value *theValue = array->i_scope == GLOBAL ? array->gvar : array->v.v;
    result->v.v = Builder.CreateInBoundsGEP(theValue->getType()->getPointerElementType(), theValue, indices, "");
    result->u.ltype = theValue->getType()->getPointerElementType();
}

/*
 * Get reference to the truth comparison, true-block, and false-block from
 * symbol table. If either v.b is null, it needs to be created and added
 * to the function's insertion point. If either has been created,
 * simply create the conditional break with it.
 */
void branch(char *truthTemp, char *trueLabel, char *falseLabel, struct id_entry *fn) {
    auto trueCmp = lookup(truthTemp, 0);

    // create the truth block
    auto truthBlock = lookup(trueLabel, 0);
    if (truthBlock->v.b == nullptr)
        truthBlock->v.b = BasicBlock::Create(TheContext, truthBlock->i_name, fn->v.f);

    // get the next br label, then create the false block with it
    auto falseBlock = lookup(falseLabel, 0);
    if (falseBlock->v.b == nullptr)
        falseBlock->v.b = BasicBlock::Create(TheContext, falseBlock->i_name, fn->v.f);

    Builder.CreateCondBr(trueCmp->v.v, truthBlock->v.b, falseBlock->v.b);
}

/*
 * Check if the destination block's v.b is null, then create it if true.
 * Otherwise, add it to the unconditional jump. Before processing the jump,
 * make sure that the destination block has lines and that it doesn't
 * only consist of 'fend' (FUNC_END). Do not jump to it if true.
 */
void jump(char *destination, struct id_entry *fn) {
    auto destinationBlock = lookup(destination, 0);
    auto destBlk = destinationBlock->blk;
    if (destBlk->lines && strcmp(destBlk->lines->items[0], "fend") == 0) return;

    if (destinationBlock->v.b == nullptr)
        destinationBlock->v.b = BasicBlock::Create(TheContext, destinationBlock->i_name, fn->v.f);

    Builder.CreateBr(destinationBlock->v.b);
}

/*
 * Create a useless instruction, dummy-noop. This is used in empty blocks. The block then
 * needs an exit point, which is its successor. If the block is "entry," then
 * we use the top's successor (which is always L1). Otherwise,
 * look-up the current block's successor and jump to it.
 */
void doNoop(struct id_entry *fn) {
    Builder.CreateAlloca(Type::getInt32Ty(TheContext), nullptr, "dummy-noop");

    StringRef currentBlock = Builder.GetInsertBlock()->getName();
    char name[256];
    strcpy(name, currentBlock.data());
    if (strcmp(name, "entry") == 0) {
        extern struct bblk *top;
        jump(top->succs->ptr->label, fn);
        return;
    }

    auto cb = lookup(name, 0);
    auto successor = cb->blk->down;
    if (successor) jump(successor->label, fn);
}

/*
 * Block that only consist of FUNC_END need an additional instruction: return.
 * Create the return, depending on the type of the function.
 */
void doEmptyFuncEnd(struct id_entry *fn) {
    if (fn->v.f->getReturnType()->isIntegerTy())
        Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(TheContext), 0));
    else
        Builder.CreateRet(ConstantFP::get(Type::getDoubleTy(TheContext), 0));
}

/*
 * Depending on the quadline's type, do a procedure.
 */
void processQuadLine(struct quadline *ptr, struct id_entry *fn) {
    inst_type type = ptr->type;
    char *destination = ptr->items[0];
    switch (type) {
        case ASSIGN:
            assign(destination, ptr->items[2]);
            break;
        case UNARY:
            unary(destination, ptr->items[2], ptr->items[3]);
            break;
        case BINOP:
            binop(destination, ptr->items[2], ptr->items[3], ptr->items[4]);
            break;
        case GLOBAL_REF:
            referenceGlobal(ptr->items[3], destination);
            break;
        case PARAM_REF:
            referenceParam(ptr->items[3], destination);
            break;
        case LOCAL_REF:
            referenceLocal(ptr->items[3], destination);
            break;
        case ADDR_ARRAY_INDEX:
            indexArray(destination, ptr->items[2], ptr->items[4]);
            break;
        case STORE:
            store(destination, ptr->items[2], ptr->items[4]);
            break;
        case LOAD:
            createLoad(ptr);
            break;
        case FUNC_CALL:
            call(ptr);
            break;
        case STRING:
            referenceString(destination, ptr->items[2]);
            break;
        case CVF:
            cast(destination, CVF, ptr->items[3]);
            break;
        case CVI:
            cast(destination, CVI, ptr->items[3]);
            break;
        case BRANCH:
            branch(ptr->items[1], ptr->items[2], ptr->next->items[1], fn);
            break;
        case JUMP:
            jump(ptr->items[1], fn);
            break;
        case RETURN:
            doReturn(ptr->items[1], fn);
            break;
        default:
            break;
    }
}

void bitcodegen() {
    struct bblk *blk;
    struct quadline *ptr;
    struct id_entry *iptr;
    extern struct bblk *top;

    // any global, then define
    for (ptr = top->lines; ptr && ptr->type == GLOBAL_ALLOC; ptr = ptr->next) {
        iptr = lookup(ptr->items[1], GLOBAL);
        assert(iptr && "global is not defined");
        createGlobal(iptr);
    }

    // generate function signature
    assert(ptr && (ptr->type == FUNC_BEGIN) && "Function definition is expected");
    auto fn = lookup(ptr->items[1], GLOBAL);
    assert(fn && "function name is not present");
    createFunction(fn, &ptr);

    BasicBlock *BB = BasicBlock::Create(TheContext, "entry", fn->v.f);
    top->lbblk = BB;
    Builder.SetInsertPoint(BB);

    // allocate storage for the param
    // allocate storage for locals
    // generate bitcode for globals, function header, params, and locals
    for (; ptr->prev && ptr->prev->type == FORMAL_ALLOC; ptr = ptr->prev)
        ;
    allocaFormals(&ptr, fn->v.f);
    allocaLocals(&ptr);
    createBitcode(ptr, fn);

    for (auto bblk = top->down; bblk; bblk = bblk->down) {
        auto bb = lookup(bblk->label, LOCAL);
        if (bb->v.b == nullptr)
            bb->v.b = BasicBlock::Create(TheContext, bblk->label, fn->v.f);
        Builder.SetInsertPoint(bb->v.b);
        createBitcode(bblk->lines, fn);
    }
    return;
}

