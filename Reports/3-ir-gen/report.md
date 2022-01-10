# Lab3 实验报告

队长：吕涵祺 PB19111654

队员：
- 赵玉宇 PB19111653
- 孙佳桢 PB19111651


## 实验难点

   1. 类型的判定和转换。

   2. 怎样区分并获取两种数组的元素地址。

   3. 函数调用时参数为数组时，处理为指针。

   4. 全局变量的定义

   5. CompoundStmt的作用域

   6. 函数形参的内存空间

## 实验设计

1. ### 如何设计全局变量

   本次实验一共设计了6个全局变量：

   ```c++
   Value *currVal;         //最近一次得到的expression的值
   Function *currFunc;     //当前函数
   BasicBlock *currRetBB;  //当前函数的返回块
   Value *currRetaddr;     //当前函数的的返回值的地址
   int compoundStmtDepth = 0;  //复合语句嵌套的深度，由0变为1时不进入新的作用域
   int labelNum = 0;       //对label计数，防止重名
   ```

   $currVal$：最近一次得到的$expression$的值，用于在不同的$visit$函数之间传递值（$Value$）。

   $currFunc$：最近一次访问的函数，用于在不同的$visit$函数之间传递当前函数。

   $currRetBB$：最近一次访问的函数的返回块，用于在不同的$visit$函数之间传递当前函数的返回块。

   $currRetaddr$：当前函数的返回值地址。

   $scopeDepth$：函数内复合语句（指能开启新作用域的，如$if-else$和$while$）的嵌套层次。

   还定义了如下宏：
   ```c++
   #define I32_TYPE Type::get_int32_type(module.get())      //获取i32的类型
   #define FLOAT_TYPE Type::get_float_type(module.get())    //float
   #define I32P_TYPE Type::get_int32_ptr_type(module.get()) //i32的指针
   #define FP_TYPE Type::get_float_ptr_type(module.get())   //float的指针
   #define GET_LABEL_NAME(name) (std::string(name) + std::to_string(labelNum++)) //用于给基本块命名，保证名称可读性的同时避免重名
   ```

2. ### 遇到的难点以及解决方案

   难点已在[**实验难点**](#实验难点)部分叙述。

   >   ## 难点1
   >
   >>   本次实验存在五种情况下的类型转换
   >>
   >>   * 赋值时
   >>   * 返回值类型和函数签名中的返回类型不一致时
   >>   * 函数调用时实参和函数签名中的形参类型不一致时
   >>   * 二元运算的两个参数类型不一致时
   >>   * 下标计算时
   >
   >   ### 解决方案
   >
   >   ```c++
   >   //二元运算的两个参数类型不一致时
   >   node.additive_expression_l->accept(*this);
   >   auto exp_l = currVal;
   >   node.additive_expression_r->accept(*this);
   >   auto exp_r = currVal;
   >   if(exp_l->get_type() == I32_TYPE && exp_r->get_type() == I32_TYPE)
   >   {
   >       //两个操作数均为整形时，不需要类型转换，直接创建整形运算指令
   >   }
   >   else
   >   {
   >       if(exp_l->get_type() == I32_TYPE) //类型转换
   >           exp_l = builder->create_sitofp(exp_l, FLOAT_TYPE);
   >       if(exp_r->get_type() == I32_TYPE)
   >           exp_r = builder->create_sitofp(exp_r, FLOAT_TYPE);
   >       //创建浮点型运算指令
   >   }
   >   ```
   >
   >   ```c++
   >   //赋值时
   >   //currVal是右侧表达式求值的结果
   >   if((currVal->get_type() == I32_TYPE && var_address->get_type() == I32P_TYPE)
   >   || (currVal->get_type() == FLOAT_TYPE && var_address->get_type() == FP_TYPE))
   >   {   
   >   	//左右值类型相同，不需要类型转换
   >       builder->create_store(currVal, var_address);
   >   }
   >   else if(currVal->get_type() == I32_TYPE && >var_address->get_type() == FP_TYPE)
   >   {
   >   	//需要float类型,i32转float
   >       auto converted_val = builder->create_sitofp(currVal, FLOAT_TYPE);
   >       builder->create_store(converted_val, var_address);
   >       currVal = converted_val;
   >   }
   >   else
   >   {
   >   	//需要i32类型，float转i32
   >       auto converted_val = builder->create_fptosi(currVal, I32_TYPE);
   >       builder->create_store(converted_val, var_address);
   >       currVal = converted_val;
   >   }
   >   ```
   >
   >   ```c++
   >   //函数调用时实参和函数签名中的形参类型不一致时
   >    Function* functy = (Function*)scope.find(node.id); //从scope中找到函数的类型
   >    auto args_list=functy->get_args();
   >    std::vector<Value *> callargs;
   >    auto args_type=args_list.begin();
   >    for (auto arg: node.args)                          //对实参求值，然后将结果类型与相  应的形参类型比较
   >    {
   >        arg->accept(*this);
   >        if((*args_type)->get_type()!=currVal->get_type())
   >        {
   >            if(currVal->get_type()==I32_TYPE)
   >               currVal = builder->create_sitofp(currVal, FLOAT_TYPE);
   >            else if(currVal->get_type()==FLOAT_TYPE)
   >               currVal = builder->create_fptosi(currVal, I32_TYPE);
   >            else if((*args_type)->get_type()->is_pointer_type())
   >            {
   >                //需要找到数组首地址的指针，见难点3
   >            }
   >        }
   >        callargs.push_back(currVal);
   >        args_type++;
   >    }
   >   ```
   >
   >   ```c++
   >   //返回值类型和函数签名中的返回类型不一致时
   >   //currVal是表达式的求值结果
   >   if(currVal->get_type()==I32_TYPE&&currRetaddr->get_type()==FP_TYPE)
   >   {
   >       //需要float类型,i32转float
   >   }
   >   else if(currVal->get_type()==FLOAT_TYPE&&>currRetaddr->get_type()==I32P_TYPE)
   >   {
   >       //需要i32类型，float转i32
   >   }
   >   else
   >   {
   >       //不需要类型转换
   >   }
   >   ```
   >
   >   ```c++
   >   //下标计算时
   >   //currVal是下标表达式的求值结果
   >   Value *indexVal;
   >   if(currVal->get_type() == FLOAT_TYPE)
   >       //float转i32
   >   else
   >       indexVal = currVal;
   >   ```
   ---
   >   ## 难点2
   >
   >   数组变量可能有两种情况：可能是同一个作用域定义的数组类型，也可能是通过函数调用传过来的指 针类型。
   >
   >   ## 解决方案
   >
   >   首先判断是哪种类型。如果是前者，则get_pointer_element_type()指向数组类型，arrayType也 就指向数组元素的类型；如果是后者，则get_pointer_element_type()直接指向指针数组的元素类型， arrayType则为空，于是便可成功区分。
   >
   >   ```c++
   >   auto baseAddress = scope.find(node.var->id);
   >   ...
   >   auto pointerType = baseAddress->get_type()->get_pointer_element_type();
   >   auto arrayType = pointerType->get_array_element_type();
   >   if(!arrayType)
   >   {
   >   	var_address = builder->create_gep(builder->create_load>(baseAddress), {indexVal})   ;
   >   }
   >   else
   >   {
   >   	var_address = builder->create_gep(baseAddress, {CONST_INT(0), indexVal});
   >   }
   >   ```
   ---
   >   ## 难点3
   >
   >   检测到实参与函数声明形参类型不一致，且形参（*args_type）为指针形式（is_pointer_type） ，实参为数组形式。
   >
   >   ## 解决方案
   >
   >   首先将实参数组指针用static_cast强制类型转换为ASTSimpleExpression，再利用其找到数组名，   再利用数组名寻找到scope中定义的数组变量，即可使用gep指令转换为数组首地址。
   >
   >   ```c++
   >   else if((*args_type)->get_type()!=currVal->get_type())
   >   {
   >   	if((*args_type)->get_type()->is_pointer_type())
   >   	{
   >   		auto arg_exp = static_cast<ASTSimpleExpression *>(arg.get());
   >   		auto arg_id = static_cast<ASTVar*>  (arg_exp->additive_expression_l->term->factor.get())->id;
   >   		auto arrayBaseAddr = scope.find(arg_id);
   >   		currVal = builder->create_gep(arrayBaseAddr, {CONST_INT(0), CONST_INT(0)});
   >   	}
   >   }
   >   ```
   ---
   >   ## 难点4
   >
   >   cminusf中变量有全局变量和局部变量之分，但是文法中统一用var-declaration来表示。
   >
   >   ### 解决方案
   >   我们的ASTVarDeclaration节点只存储全局变量，别的var-declaration推理出的变量不通过递归调   用visit函数来生成。所以在ASTVarDeclaration的visit中把所有的var视作全局变量，之后出现的 var-declaration直接压入scope并生成alloca指令。
   > 
   > ```c++
   > void CminusfBuilder::visit(ASTVarDeclaration &node) {
   >     GlobalVariable* glo;
   >     auto i32Initializer = ConstantZero::get(I32_TYPE, module.get());
   >     auto floatInitializer = ConstantZero::get(FLOAT_TYPE, module.get());
   >     if(node.num == nullptr){                //var is not a array, set the global var
   >         if(node.type == TYPE_INT){
   >             glo = GlobalVariable::create(node.id, module.get(), Type::get_int32_type (module.get()), false, i32Initializer);
   >         }
   >         else if(node.type == TYPE_FLOAT){
   >             glo = GlobalVariable::create(node.id, module.get(), Type::get_float_type (module.get()), false, floatInitializer);
   >         }
   >     }   
   >     else{                                   //var is a array, create the array type  and set the global array var
   >         if(node.type == TYPE_INT){
   >             auto arrtype = ArrayType::get(Type::get_int32_type(module.get()), node.  num->i_val);
   >             glo = GlobalVariable::create(node.id, module.get(), arrtype, false,   i32Initializer);
   >         }
   >         else if(node.type == TYPE_FLOAT){
   >             auto arrtype = ArrayType::get(Type::get_float_type(module.get()), node.  num->i_val);
   >             glo = GlobalVariable::create(node.id, module.get(), arrtype, false,   floatInitializer);
   >         }
   >     }
   >     scope.push(node.id, glo);               //push the global var to the scope
   > }
   > ```
   ---
   >   ## 难点5
   >
   >   文档要求函数形参必须拥有和函数声明的复合语句相同的作用域，但其他的非函数块的复合语句则没 有这个要求。
   >
   >   ### 解决方案
   >   我们在函数声明中先scope.enter再压入变量，然后进入CompoundStmt，再scope.exit。而对于其 他的CompoundStmt,他们也需要新的作用域，所以要在CompoundStmt的开头判断是函数声明中的  CompoundStmt还是其他的CompoundStmt。
   > 
   > ```c++
   > if(compoundStmtDepth > 0)                       //此全局变量为0时说明该复合语句的作用域 已由visit(ASTFunDeclaration &node)创建
   >     scope.enter();
   > compoundStmtDepth++;
   > ```
   ---
   >   ## 难点6
   >
   >   在函数定义中，我们要为形参分配域内的内存空间。
   >
   >   ### 解决方案
   >   我们通过遍历生成的函数的参数列表，按参数类型分配空间，并将形参的值存到参数空间内，然后压 入scope。
   > ```c++
   > auto arg = func->arg_begin();
   >     for(auto par: node.params){             //allocate the spcae of the function  params in the compound scope
   >         if(par->isarray){
   >             auto parvar = builder->create_alloca(par->type == TYPE_INT?  PointerType::get_int32_ptr_type(module.get()): \
   >                                                     PointerType::get_float_ptr_type  (module.get()));
   >             builder->create_store(*arg, parvar);
   >             scope.push(par->id, parvar);    //push the param to the compound scope
   >         }
   >         else{
   >             AllocaInst *parvar;
   >             if(par->type == TYPE_INT){
   >                 parvar = builder->create_alloca(Type::get_int32_type(module.get()));
   >             }
   >             else if(par->type == TYPE_FLOAT){
   >                 parvar = builder->create_alloca(Type::get_float_type(module.get()));
   >             }
   >             builder->create_store(*arg, parvar);
   >             scope.push(par->id, parvar);
   >         }
   >         arg++;
   >     }
   > ```

3. ### 如何降低生成 IR 中的冗余
   在对函数定义处理结束后，遍历函数的基本块列表，找到无用的基本块并删除。

   无用的基本块是指，去掉冗余的跳转指令——即第一条跳转指令后的跳转指令——之后，整个基本块只有一条无条件跳转指令。

   具体做法：
   ```c++
   std::vector<BasicBlock *> emptyBBs;//保存上述的“无用的基本块”
   auto basic_blocks = func->get_basic_blocks();
   for(auto bb : basic_blocks)        //找出移除多余跳转指令后只有一条无条件跳转指令的本块
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
      if(bb->get_num_of_instr() == 1/*只剩一条指令*/ &&
         bb->get_terminator()->get_num_operand() == 1/*这条指令的操作数为1，即为无条件跳转指令*/)
         emptyBBs.push_back(bb);    //加入空块的列表中
   }
   for(auto bb: basic_blocks)         //修改目标为空块的跳转指令
   {
      BasicBlock* br_to_1, *br_to_2; //记录跳转指令的跳转目标
      Value* cond;                   //记录跳转指令的判断条件
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
        gain1:
      for(auto emptybb: emptyBBs)    //在空块列表里搜索目标块，若存在则将目标更改为此块的目标，类似在链表中删除结点
      {
         if(br_to_1 == emptybb)
         {
            br_to_1 = static_cast<BasicBlock*>(emptybb->get_terminator(->get_operand(0));
            goto again1;           //这里要重复至跳转目标不在空块列表里为止
         }
      }
        gain2:
      if(br_to_2)                  //同上
      {
         for(auto emptybb: emptyBBs)
         {
            if(br_to_2 == emptybb)
            {
               br_to_2 = static_cast<BasicBlock*>(emptybb->get_terminator(->get_operand(0));
               goto again2;
            }
         }
      }
      bb->delete_instr(terminator);  //替换跳转指令
      if(br_to_2)
      {
         BranchInst::create_cond_br(cond, br_to_1, br_to_2, bb);
      }
      else
      {
         BranchInst::create_br(br_to_1, bb);
      }
      for(auto emptybb: emptyBBs)    //删除空块
      {
         func->remove(emptybb);
      }
    }
   ```

   


## 实验总结

本次实验不仅考察了我们对于编译原理语法、语义内容的掌握程度，也让我们对于C++有了更深刻的认识，给了我们宝贵的实践机会；同时还能够起到对以往学习的其他课程一定程度上的复习作用，受益匪浅。

## 实验反馈 （可选 不会评分）



## 组间交流 （可选）

本次实验未和其他组交流信息。