#include "cminusf_builder.hpp"
#include <vector>

// use these macros to get constant value
#define CONST_FP(num) \
    ConstantFP::get((float)num, module.get())
#define CONST_INT(num) \
    ConstantInt::get(num, module.get())

#define I32_TYPE Type::get_int32_type(module.get())
#define FLOAT_TYPE Type::get_float_type(module.get())
#define I32P_TYPE Type::get_int32_ptr_type(module.get())
#define FP_TYPE Type::get_float_ptr_type(module.get())
#define GET_LABEL_NAME(name) (std::string(name) + std::to_string(labelNum++))

// You can define global variables here
// to store state

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value *currVal;         //最近一次得到的expression的值
Function *currFunc;     //最近一次访问的函数
BasicBlock *currRetBB;  //最近一次访问的函数的返回块
Value *currRetaddr;     //最近一次的返回地址（？）

int compoundStmtDepth = 0;  //复合语句嵌套的深度，0->1时不进入新的作用域
int labelNum = 0;       //对label计数，防止重名

void CminusfBuilder::visit(ASTProgram &node) {
    for(auto dec: node.declarations){
        dec->accept(*this);
    }
}

void CminusfBuilder::visit(ASTNum &node) {
    if(node.type == TYPE_INT){
        currVal = ConstantInt::get(node.i_val, module.get());
    }
    else if(node.type == TYPE_FLOAT){
        currVal = ConstantFP::get(node.f_val, module.get());
    }
}

void CminusfBuilder::visit(ASTVarDeclaration &node) {
    GlobalVariable* glo;
    auto i32Initializer = ConstantZero::get(I32_TYPE, module.get());
    auto floatInitializer = ConstantZero::get(FLOAT_TYPE, module.get());
    if(node.num == nullptr){                //var is not a array, set the global var
        if(node.type == TYPE_INT){
            glo = GlobalVariable::create(node.id, module.get(), Type::get_int32_type(module.get()), false, i32Initializer);
        }
        else if(node.type == TYPE_FLOAT){
            glo = GlobalVariable::create(node.id, module.get(), Type::get_float_type(module.get()), false, floatInitializer);
        }
    }   
    else{                                   //var is a array, create the array type and set the global array var
        if(node.type == TYPE_INT){
            auto arrtype = ArrayType::get(Type::get_int32_type(module.get()), node.num->i_val);
            glo = GlobalVariable::create(node.id, module.get(), arrtype, false, i32Initializer);
        }
        else if(node.type == TYPE_FLOAT){
            auto arrtype = ArrayType::get(Type::get_float_type(module.get()), node.num->i_val);
            glo = GlobalVariable::create(node.id, module.get(), arrtype, false, floatInitializer);
        }
    }
    scope.push(node.id, glo);               //push the global var to the scope
}

void CminusfBuilder::visit(ASTFunDeclaration &node) {
    std::vector<Type *> funcparams;         //the function params vector
    for(auto par: node.params){
        if(par->isarray){                   //if the param is a array, push the certain pointer type to the vector
            if(par->type == TYPE_INT){
                funcparams.push_back(PointerType::get_int32_ptr_type(module.get()));
            }
            else if(par->type == TYPE_FLOAT){
                funcparams.push_back(PointerType::get_float_ptr_type(module.get()));
            }
        }
        else{                               //else push the value type
            if(par->type == TYPE_INT){
                funcparams.push_back(Type::get_int32_type(module.get()));
            }
            else if(par->type == TYPE_FLOAT){
                funcparams.push_back(Type::get_float_type(module.get()));
            }
        }
    }
    FunctionType *functy;                   //define the function type
    if(node.type == TYPE_INT){
        functy = FunctionType::get(Type::get_int32_type(module.get()), funcparams);
    }
    else if (node.type == TYPE_FLOAT){
        functy = FunctionType::get(Type::get_float_type(module.get()), funcparams);
    }
    else{
        functy = FunctionType::get(Type::get_void_type(module.get()), funcparams);
    }
    auto func = Function::create(functy, node.id, module.get());
    currFunc = func;                        //set the currFunc
    scope.push(node.id, func);              //push the function id 
    scope.enter();                          //enter a new scope
    auto startBB = BasicBlock::create(module.get(), GET_LABEL_NAME("func_start"), func);
    //startBB contains the param define and compound_stmt
    auto funcretBB = currRetBB = BasicBlock::create(module.get(), GET_LABEL_NAME("func_ret"), func);    //set the currRetBB
    //funcrretBB contains the return operation
    builder->set_insert_point(startBB);
    Value *retvar;                          //the addr of the return value
    if(node.type != TYPE_VOID){             //allocate the space of the return value
        if(node.type == TYPE_INT){
            retvar = builder->create_alloca(Type::get_int32_type(module.get()));
        }
        else if(node.type == TYPE_FLOAT){
            retvar = builder->create_alloca(Type::get_float_type(module.get()));
        }
    }
    currRetaddr = retvar;                   //set the currRetaddr
    auto arg = func->arg_begin();
    for(auto par: node.params){             //allocate the spcae of the function params in the compound scope
        if(par->isarray){
            auto parvar = builder->create_alloca(par->type == TYPE_INT? PointerType::get_int32_ptr_type(module.get()): \
                                                    PointerType::get_float_ptr_type(module.get()));
            builder->create_store(*arg, parvar);
            scope.push(par->id, parvar);    //push the param to the compound scope
        }
        else{
            AllocaInst *parvar;
            if(par->type == TYPE_INT){
                parvar = builder->create_alloca(Type::get_int32_type(module.get()));
            }
            else if(par->type == TYPE_FLOAT){
                parvar = builder->create_alloca(Type::get_float_type(module.get()));
            }
            builder->create_store(*arg, parvar);
            scope.push(par->id, parvar);
        }
        arg++;
    }
    node.compound_stmt->accept(*this);
    auto currBB = builder->get_insert_block();
    if(!currBB->get_terminator())builder->create_br(funcretBB); //if the current BB have not a ret or br instruction then add a br
    builder->set_insert_point(funcretBB);                       //write the funcretBB
    if(node.type == TYPE_INT){
        auto retreg = builder->create_load(Type::get_int32_type(module.get()), retvar);
        builder->create_ret(retreg);
    }
    else if(node.type == TYPE_FLOAT){
        auto retreg = builder->create_load(Type::get_float_type(module.get()), retvar);
        builder->create_ret(retreg);
    }
    else {
        builder->create_void_ret();
    }
    std::vector<BasicBlock *> emptyBBs;
    auto basic_blocks = func->get_basic_blocks();
    for(auto bb : basic_blocks)                                 //找出移除多余跳转指令后只有一条指令的基本块
    {
        bool endOfBB = false;
        auto inst_list = bb->get_instructions();
        for(auto inst: inst_list)
        {
            if(endOfBB)
            {
                bb->delete_instr(inst);
                continue;
            }
            if(inst->isTerminator())endOfBB = true;
        }
        if(bb->get_num_of_instr() == 1 && bb->get_terminator()->get_num_operand() == 1)
            emptyBBs.push_back(bb);
    }
    for(auto bb: basic_blocks)                                  //修改目标为空块的跳转指令
    {
        /*bool isEmpty = false;
        for(auto emptybb: emptyBBs)
        {
            isEmpty = true;
        }
        if(isEmpty)continue;*/
        BasicBlock* br_to_1, *br_to_2;
        Value* cond;
        auto terminator = bb->get_terminator();
        if(!terminator->is_br())
            continue;
        if(terminator->get_num_operand() == 1)
        {
            br_to_1 = static_cast<BasicBlock*>(terminator->get_operand(0));
            br_to_2 = nullptr;
            cond = nullptr;
        }
        else
        {
            cond = terminator->get_operand(0);
            br_to_1 = static_cast<BasicBlock*>(terminator->get_operand(1));
            br_to_2 = static_cast<BasicBlock*>(terminator->get_operand(2));
        }
        again1:
        for(auto emptybb: emptyBBs)
        {
            if(br_to_1 == emptybb)
            {
                br_to_1 = static_cast<BasicBlock*>(emptybb->get_terminator()->get_operand(0));
                goto again1;
            }
        }
        again2:
        if(br_to_2)
        {
            for(auto emptybb: emptyBBs)
            {
                if(br_to_2 == emptybb)
                {
                    br_to_2 = static_cast<BasicBlock*>(emptybb->get_terminator()->get_operand(0));
                    goto again2;
                }
            }
        }
        bb->delete_instr(terminator);
        if(br_to_2)
        {
            BranchInst::create_cond_br(cond, br_to_1, br_to_2, bb);
        }
        else
        {
            BranchInst::create_br(br_to_1, bb);
        }
        for(auto emptybb: emptyBBs)                             //删除空块
        {
            func->remove(emptybb);
        }
    }
    scope.exit();
}

void CminusfBuilder::visit(ASTParam &node) {
    //there is nothing to do
}

void CminusfBuilder::visit(ASTCompoundStmt &node) {
    if(compoundStmtDepth > 0)                       //此全局变量为0时说明该复合语句的作用域已由visit(ASTFunDeclaration &node)创建
        scope.enter();
    compoundStmtDepth++;
    for(auto loc: node.local_declarations){         //visit the local_declarations
        //loc->accept(*this);
        if(loc->type == TYPE_INT){                  //int
            if(loc->num == nullptr){                //is not a array
                auto intvar = builder->create_alloca(Type::get_int32_type(module.get()));
                scope.push(loc->id, intvar);
            }
            else{                                   //is a array
                auto arrtype = ArrayType::get(Type::get_int32_type(module.get()), loc->num->i_val);
                auto intarrvar = builder->create_alloca(arrtype);
                scope.push(loc->id, intarrvar);
            }
        }
        else if(loc->type == TYPE_FLOAT){           //float
            if(loc->num == nullptr){
                auto floatvar = builder->create_alloca(Type::get_float_type(module.get()));
                scope.push(loc->id, floatvar);
            }
            else{
                auto arrtype = ArrayType::get(Type::get_float_type(module.get()), loc->num->i_val);
                auto floatarrvar = builder->create_alloca(arrtype);
                scope.push(loc->id, floatarrvar);
            }
        }
    }
    for(auto sta: node.statement_list){             //visit the stmt
        sta->accept(*this);
    }
    compoundStmtDepth--;
    if(compoundStmtDepth > 0)
        scope.exit();
}

void CminusfBuilder::visit(ASTExpressionStmt &node) {   //visit the expression
    if(node.expression) node.expression->accept(*this);
}

void CminusfBuilder::visit(ASTSelectionStmt &node) {    
    Value* cmpresult;
    node.expression->accept(*this);                     //visit the expression, the value will be in the currVal
    if(currVal->get_type() == I32_TYPE){
        cmpresult = builder->create_icmp_ne(currVal, CONST_INT(0));
    }
    else if(currVal->get_type() == FLOAT_TYPE){         //if the vale of the expression is a float type, Convert it to int
        auto floattoint = builder->create_fptosi(currVal, I32_TYPE);
        cmpresult = builder->create_icmp_ne(floattoint, CONST_INT(0));
    }
    auto trueBB = BasicBlock::create(module.get(), GET_LABEL_NAME("if_true"), currFunc);
    auto endBB = BasicBlock::create(module.get(), GET_LABEL_NAME("if_end"), currFunc);
    if(node.else_statement == nullptr){
        builder->create_cond_br(cmpresult, trueBB, endBB);
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        builder->create_br(endBB);
    }
    else{
        auto falseBB = BasicBlock::create(module.get(), GET_LABEL_NAME("if_false"), currFunc);
        builder->create_cond_br(cmpresult, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        builder->create_br(endBB);
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        builder->create_br(endBB);
    }
    builder->set_insert_point(endBB);
}

void CminusfBuilder::visit(ASTIterationStmt &node) {
    auto judgeBB = BasicBlock::create(module.get(), GET_LABEL_NAME("while_judge"), currFunc);
    builder->create_br(judgeBB);
    builder->set_insert_point(judgeBB);
    node.expression->accept(*this);
    Value* judge;
    if(currVal->get_type() == I32_TYPE)
    {
        judge = builder->create_icmp_ne(currVal, CONST_INT(0));
    }
    else if(currVal->get_type() == FLOAT_TYPE){
        judge = builder->create_fcmp_ne(currVal, CONST_FP(0));
    }
    auto loopBB = BasicBlock::create(module.get(), GET_LABEL_NAME("while_loop"), currFunc);
    auto endBB = BasicBlock::create(module.get(), GET_LABEL_NAME("while_end"), currFunc);
    builder->create_cond_br(judge, loopBB, endBB);
    builder->set_insert_point(loopBB);
    node.statement->accept(*this);
    builder->create_br(judgeBB);
    builder->set_insert_point(endBB);
}

void CminusfBuilder::visit(ASTReturnStmt &node) { 
    if(node.expression!=nullptr)
    {
        node.expression->accept(*this);
        if(currVal->get_type()==I32_TYPE&&currRetaddr->get_type()==FP_TYPE)
        {
            auto converted_val = builder->create_sitofp(currVal, FLOAT_TYPE);
            builder->create_store(converted_val, currRetaddr);
            currVal = converted_val;
        }
        else if(currVal->get_type()==FLOAT_TYPE&&currRetaddr->get_type()==I32P_TYPE)
        {
            auto converted_val = builder->create_fptosi(currVal, I32_TYPE);
            builder->create_store(converted_val, currRetaddr);
            currVal = converted_val;
        }
        else
        {
            builder->create_store(currVal, currRetaddr);
        }
    }
    builder->create_br(currRetBB);
}

void CminusfBuilder::visit(ASTVar &node) {
    auto baseAddress = scope.find(node.id);
    if(node.expression == nullptr)
        currVal = builder->create_load(baseAddress);
    else
    {
        //下标求值及类型转换
        node.expression->accept(*this);
        Value *indexVal;
        if(currVal->get_type() == FLOAT_TYPE)
            indexVal = builder->create_fptosi(currVal, I32_TYPE);
        else
            indexVal = currVal;
        //判断下标合法性
        auto isIndexValid = builder->create_icmp_ge(indexVal, CONST_INT(0));
        auto invalidBB = BasicBlock::create(module.get(), GET_LABEL_NAME("idx_neg"), currFunc);
        auto validBB = BasicBlock::create(module.get(), GET_LABEL_NAME("idx_valid"), currFunc);
        builder->create_cond_br(isIndexValid, validBB, invalidBB);
        //下标为负调用neg_idx_except
        builder->set_insert_point(invalidBB);
        builder->create_call(scope.find("neg_idx_except"), {});
        builder->create_br(currRetBB);
        builder->set_insert_point(validBB);
        //判断指针类型
        auto pointerType = baseAddress->get_type()->get_pointer_element_type();
        auto arrayType = pointerType->get_array_element_type();
        if(!arrayType)
        {
            //不是指向数组，此时baseAddress是函数中数组类型的形参的值，也就是指向存放数组首地址的变量的指针
            auto actAddr = builder->create_gep(builder->create_load(baseAddress), {indexVal});
            currVal = builder->create_load(actAddr);
        }
        else
        {
            //指向数组
            auto actAddr = builder->create_gep(baseAddress, {CONST_INT(0), indexVal});
            currVal = builder->create_load(actAddr);
        }
    }
}

void CminusfBuilder::visit(ASTAssignExpression &node) {
    auto baseAddress = scope.find(node.var->id);
    Value *var_address;
    if(node.var->expression == nullptr)
        var_address = baseAddress;
    else
    {
        //找到数组元素的地址，和ASTVar的visit相同
        node.var->expression->accept(*this);
        Value *indexVal;
        if(currVal->get_type() == FLOAT_TYPE)
            indexVal = builder->create_fptosi(currVal, I32_TYPE);
        else
            indexVal = currVal;
        auto isIndexValid = builder->create_icmp_ge(indexVal, CONST_INT(0));
        auto invalidBB = BasicBlock::create(module.get(), GET_LABEL_NAME("idx_neg"), currFunc);
        auto validBB = BasicBlock::create(module.get(), GET_LABEL_NAME("idx_valid"), currFunc);
        builder->create_cond_br(isIndexValid, validBB, invalidBB);
        builder->set_insert_point(invalidBB);
        builder->create_call(scope.find("neg_idx_except"), {});
        builder->create_br(currRetBB);
        builder->set_insert_point(validBB);
        auto pointerType = baseAddress->get_type()->get_pointer_element_type();
        auto arrayType = pointerType->get_array_element_type();
        if(!arrayType)
        {
            var_address = builder->create_gep(builder->create_load(baseAddress), {indexVal});
        }
        else
        {
            var_address = builder->create_gep(baseAddress, {CONST_INT(0), indexVal});
        }
    }
    
    node.expression->accept(*this);
    //类型转换
    if((currVal->get_type() == I32_TYPE && var_address->get_type() == I32P_TYPE)
    || (currVal->get_type() == FLOAT_TYPE && var_address->get_type() == FP_TYPE))
        builder->create_store(currVal, var_address);
    else if(currVal->get_type() == I32_TYPE && var_address->get_type() == FP_TYPE)
    {
        auto converted_val = builder->create_sitofp(currVal, FLOAT_TYPE);
        builder->create_store(converted_val, var_address);
        currVal = converted_val;
    }
    else
    {
        auto converted_val = builder->create_fptosi(currVal, I32_TYPE);
        builder->create_store(converted_val, var_address);
        currVal = converted_val;
    }
 }

void CminusfBuilder::visit(ASTSimpleExpression &node) {
    node.additive_expression_l->accept(*this);
    
    if(node.additive_expression_r != nullptr)
    {   
        auto exp_l = currVal;
        node.additive_expression_r->accept(*this);
        auto exp_r = currVal;
        Value* result;
        if(exp_l->get_type() == I32_TYPE && exp_r->get_type() == I32_TYPE)
        {
            switch (node.op)
            {
            case OP_LE:
                result = builder->create_icmp_le(exp_l, exp_r);
                break;
            case OP_LT:
                result = builder->create_icmp_lt(exp_l, exp_r);
                break;
            case OP_GT:
                result = builder->create_icmp_gt(exp_l, exp_r);
                break;
            case OP_GE:
                result = builder->create_icmp_ge(exp_l, exp_r);
                break;
            case OP_EQ:
                result = builder->create_icmp_eq(exp_l, exp_r);
                break;
            case OP_NEQ:
                result = builder->create_icmp_ne(exp_l, exp_r);
                break;
            default:
                break;
            }
        }
        else
        {
            if(exp_l->get_type() == I32_TYPE)                       //类型转换
                exp_l = builder->create_sitofp(exp_l, FLOAT_TYPE);
            if(exp_r->get_type() == I32_TYPE)
                exp_r = builder->create_sitofp(exp_r, FLOAT_TYPE);
            switch (node.op)
            {
            case OP_LE:
                result = builder->create_fcmp_le(exp_l, exp_r);
                break;
            case OP_LT:
                result = builder->create_fcmp_lt(exp_l, exp_r);
                break;
            case OP_GT:
                result = builder->create_fcmp_gt(exp_l, exp_r);
                break;
            case OP_GE:
                result = builder->create_fcmp_ge(exp_l, exp_r);
                break;
            case OP_EQ:
                result = builder->create_fcmp_eq(exp_l, exp_r);
                break;
            case OP_NEQ:
                result = builder->create_fcmp_ne(exp_l, exp_r);
                break;
            default:
                break;
            }
        }
        currVal = builder->create_zext(result, I32_TYPE);
    }
    else return;
 }

void CminusfBuilder::visit(ASTAdditiveExpression &node) {
    if(node.additive_expression == nullptr)
        node.term->accept(*this);
    else
    {
        node.additive_expression->accept(*this);
        auto exp_l = currVal;
        node.term->accept(*this);
        auto exp_r = currVal;
        Value *result;
        if(exp_l->get_type() == I32_TYPE && exp_r->get_type() == I32_TYPE)
        {
            switch (node.op)
            {
            case OP_PLUS:
                result = builder->create_iadd(exp_l, exp_r);
                break;
            case OP_MINUS:
                result = builder->create_isub(exp_l, exp_r);
                break;
            default:
                break;
            }
        }
        else
        {
            if(exp_l->get_type() == I32_TYPE)
                exp_l = builder->create_sitofp(exp_l, FLOAT_TYPE);
            if(exp_r->get_type() == I32_TYPE)
                exp_r = builder->create_sitofp(exp_r, FLOAT_TYPE);
            switch (node.op)
            {
            case OP_PLUS:
                result = builder->create_fadd(exp_l, exp_r);
                break;
            case OP_MINUS:
                result = builder->create_fsub(exp_l, exp_r);
                break;
            default:
                break;
            }
        }
        currVal = result;
    }
 }

void CminusfBuilder::visit(ASTTerm &node) { 
    if (node.term != nullptr)
    {
        node.term->accept(*this);
        auto exp_l = currVal;
        node.factor->accept(*this);
        auto exp_r = currVal;
        Value* mul_result;
        if(exp_l->get_type() == I32_TYPE && exp_r->get_type() == I32_TYPE)
        {
            switch (node.op)
            {
            case OP_MUL:
                mul_result = builder->create_imul(exp_l, exp_r);
                break;
            case OP_DIV:
                mul_result = builder->create_isdiv(exp_l, exp_r);
                break;
            default:
                break;
            }
            currVal = mul_result;
        }
        else
        {
            
            if(exp_l->get_type() == I32_TYPE)
                exp_l = builder->create_sitofp(exp_l, FLOAT_TYPE);
            if(exp_r->get_type() == I32_TYPE)
                exp_r = builder->create_sitofp(exp_r, FLOAT_TYPE);
            switch (node.op)
            {
            case OP_MUL:
                mul_result = builder->create_fmul(exp_l, exp_r);
                break;
            case OP_DIV:
                mul_result = builder->create_fdiv(exp_l, exp_r);
                break;
            default:
                break;
            }
            currVal = mul_result;
        }
    }
    else
    {
        node.factor->accept(*this);
    }
    return;
}

void CminusfBuilder::visit(ASTCall &node) {
    Function* functy = (Function*)scope.find(node.id);
    auto args_list=functy->get_args();
    std::vector<Value *> callargs;
    auto args_type=args_list.begin();
    for (auto arg: node.args) 
    {
        arg->accept(*this);
        if((*args_type)->get_type()!=currVal->get_type())
        {
            if(currVal->get_type()==I32_TYPE)
                currVal = builder->create_sitofp(currVal, FLOAT_TYPE);
            else if(currVal->get_type()==FLOAT_TYPE)
                currVal = builder->create_fptosi(currVal, I32_TYPE);
            else if((*args_type)->get_type()->is_pointer_type())
            {
                auto arg_exp = static_cast<ASTSimpleExpression*>(arg.get());
                auto arg_id = static_cast<ASTVar*>(arg_exp->additive_expression_l->term->factor.get())->id;
                auto arrayBaseAddr = scope.find(arg_id);
                currVal = builder->create_gep(arrayBaseAddr, {CONST_INT(0), CONST_INT(0)});
            }
        }
        callargs.push_back(currVal);
        args_type++;
    }
    currVal=builder->create_call(functy,callargs);
 }
