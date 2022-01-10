# Lab4 实验报告
- PB19111654 吕涵祺
- PB19111651 孙佳桢
- PB19111653 赵玉宇

## 实验要求

学习开发三个基本优化Pass
- 常量传播&死代码删除
  > 如果一个变量的值可以在编译优化阶段直接计算出，那么就直接将该变量替换为常量
- 循环不变式外提
  > 实现将与循环无关的表达式提取到循环的外面
- 活跃变量分析
  > 实现分析 bb 块的入口和出口的活跃变量
## 实验难点

- 常量传播&死代码删除
  > 1. 常量的识别
  > 2. 删除死代码的操作
- 循环不变式外提
  > 1. 识别可以外提的循环不变式
  > 2. 如何将嵌套循环中的不变式尽可能地外提
- 活跃变量分析
  > 1. 需要对phi指令做特殊处理
  > 2. phi指令的变量需要与普通变量明确区分开，因为最终会被迭代混合到in中，需要分析是否是某两个块之间的phi变量
  > 3. 需要判断什么时候迭代结束

## 实验设计

* ### 常量传播

  #### 实现思路：

  在常量传播部分，需要进行操作的有：

  ​	1.**整形**和**浮点型**` BinaryInst`的两常量折叠：改为传播两操作数计算结果的值；

  ​	2.**整形**和**浮点型**` CmpInst`的两常量折叠：改为传播两操作数比较结果的值（均为`Int`类型）；

  ​	3.类型转换指令的常量传播：需改变类型。

  ​	4.完成以上操作后，通过`replace_all_use_with()`函数将此常量传播到其他所有引用处。

  在死代码删除部分，讨论有两种思路如下：

  ###### 思路一

  ​	将常量传播和死代码删除分离，待所有常量传播完成后再进行死代码删除，步骤如下：

  ​	1.循环开始，创建一个标记（`flag`）赋值为**假**；

  ​	2.遍历所有指令；

  ​	3.对每一条指令，取得它的使用情况（使用`get_use_list()`函数）；

  ​	4.如果该指令没有出现在`use_list_`里的所有指令的所有operand中（判断条件这么复杂是因为`use_list_`好像并不会因为常量传播而变空？），则删除该指令，`flag`赋值为**真**；

  ​	5.如果`flag`为**真**：跳转至`1`；如果`flag`为**假**，结束。

  ###### 思路二

  ​	1.在常量传播部分，直接将常量传播之后的该指令（显然该指令已经是死代码）传入一个待删除的链表；

  ​	2.在所有常量传播都做完后全部删除。

  最终成品采取的是更简洁的思路二，不过思路一的代码我们也成功实现了。

  #### 相应代码：

  ##### 常量传播

  值得讨论的设计有：浮点型` CmpInst`的两常量折叠函数，和类型转换的实现。

  ```c++
  //浮点型CmpInst的两常量折叠函数
  ConstantInt *ConstFolder::compute_cmpFP(
      FCmpInst::CmpOp op,
      ConstantFP *value1,
      ConstantFP *value2)
  {
      float c_value1 = value1->get_value();
      float c_value2 = value2->get_value();
      float temp = abs(c_value1 - c_value2);//用于处理等号和不等号
      switch (op)
      {
          case CmpInst::EQ:
              return ConstantInt::get(temp < 1e-7, module_);//两数相差小于10^(-7)，则可认为相等
              break;
          case CmpInst::NE:
              return ConstantInt::get(temp > 1e-7, module_);//反之
              break;
          case CmpInst::GT:...
          case CmpInst::GE:...
          case CmpInst::LT:...
          case CmpInst::LE:...
          default:...
      }
  }
  ```

  ```c++
  //类型转换
  if(instr->is_fp2si())
  {
  	auto l_val = instr->get_operand(0);
  	if(cast_constantfp(l_val))//如果操作数是常量
  	{
  		int temp=cast_constantfp(l_val)->get_value();//用一个i32的变量获取该浮点数的值，实际上实现了类型转换
  		auto t=ConstantInt::get(temp, m_);//以temp的值来创建一个i32常量t
  		instr->replace_all_use_with(t);//常量传播
  	}
      wait_delete.push_back(instr);//死代码删除思路二的实现
  }
  ```

  ##### 死代码删除

  思路一的完整代码如下：

  ```c++
  while(1)//直到检测不到死代码后，才会退出循环
  {
      bool flag = false;
      for(auto func:m_->get_functions())
      {
          for(auto block:func->get_basic_blocks())
          {
              std::unordered_set<Value *> instrs_to_del;
              for(auto instr:block->get_instructions())
              {
                  bool used = false;
                  switch(instr->get_instr_type())
                  {
                  case Instruction::add:
                  case Instruction::sub:
                  case Instruction::mul:
                  case Instruction::sdiv:
                  case Instruction::fadd:
                  case Instruction::fsub:
                  case Instruction::fmul:
                  case Instruction::fdiv:
                  case Instruction::cmp:
                  case Instruction::fcmp:
                  case Instruction::zext:
                  case Instruction::fptosi:
                  case Instruction::sitofp:
                      
                      for(auto use : instr->get_use_list())
                      {
                          if(instrs_to_del.find(use.val_) == instrs_to_del.end())
                          {
                              for(auto use_opre: static_cast<User *>(use.val_)->get_operands())
                                  //判断是不是真的是死代码
                              {
                                  if(use_opre == instr)//找到了被引用的情况，则不是死代码
                                  {
                                      used = true;
                                      break;
                                  }
                              }
                              if (used == true)
                                  break;
                          }
                      }
                      if(!used)
                      {
                          instrs_to_del.emplace(instr);
                          flag = true;
                      }
                      break;
                  default:
                      break;
                  }
              }
              for(auto instr: instrs_to_del)//删死代码
              {
                  block->delete_instr((Instruction *)instr);
              }
          }
      }
      if(!flag)
          break;
  }
  ```

  #### 优化前后的IR对比（举一个例子）并辅以简单说明：

  以第三个测试样例为例。

  ```
  //优化前
  @opa = global i32 zeroinitializer
  @opb = global i32 zeroinitializer
  @opc = global i32 zeroinitializer
  @opd = global i32 zeroinitializer
  declare i32 @input()
  
  declare void @output(i32)
  
  declare void @outputFloat(float)
  
  declare void @neg_idx_except()
  
  define i32 @max() {
  label_entry:
    %op0 = mul i32 0, 1
    %op1 = mul i32 %op0, 2
    %op2 = mul i32 %op1, 3
    %op3 = mul i32 %op2, 4
    %op4 = mul i32 %op3, 5
    %op5 = mul i32 %op4, 6
    %op6 = mul i32 %op5, 7
    store i32 %op6, i32* @opa
    %op7 = mul i32 1, 2
    %op8 = mul i32 %op7, 3
    %op9 = mul i32 %op8, 4
    %op10 = mul i32 %op9, 5
    %op11 = mul i32 %op10, 6
    %op12 = mul i32 %op11, 7
    %op13 = mul i32 %op12, 8
    store i32 %op13, i32* @opb
    %op14 = mul i32 2, 3
    %op15 = mul i32 %op14, 4
    %op16 = mul i32 %op15, 5
    %op17 = mul i32 %op16, 6
    %op18 = mul i32 %op17, 7
    %op19 = mul i32 %op18, 8
    %op20 = mul i32 %op19, 9
    store i32 %op20, i32* @opc
    %op21 = mul i32 3, 4
    %op22 = mul i32 %op21, 5
    %op23 = mul i32 %op22, 6
    %op24 = mul i32 %op23, 7
    %op25 = mul i32 %op24, 8
    %op26 = mul i32 %op25, 9
    %op27 = mul i32 %op26, 10
    store i32 %op27, i32* @opd
    %op28 = load i32, i32* @opa
    %op29 = load i32, i32* @opb
    %op30 = icmp slt i32 %op28, %op29
    %op31 = zext i1 %op30 to i32
    %op32 = icmp ne i32 %op31, 0
    br i1 %op32, label %label33, label %label39
  label33:                                                ; preds = %label_entry
    %op34 = load i32, i32* @opb
    %op35 = load i32, i32* @opc
    %op36 = icmp slt i32 %op34, %op35
    %op37 = zext i1 %op36 to i32
    %op38 = icmp ne i32 %op37, 0
    br i1 %op38, label %label40, label %label46
  label39:                                                ; preds = %label_entry, %label46
    ret i32 0
  label40:                                                ; preds = %label33
    %op41 = load i32, i32* @opc
    %op42 = load i32, i32* @opd
    %op43 = icmp slt i32 %op41, %op42
    %op44 = zext i1 %op43 to i32
    %op45 = icmp ne i32 %op44, 0
    br i1 %op45, label %label47, label %label49
  label46:                                                ; preds = %label33, %label49
    br label %label39
  label47:                                                ; preds = %label40
    %op48 = load i32, i32* @opd
    ret i32 %op48
  label49:                                                ; preds = %label40
    br label %label46
  }
  define void @main() {
  label_entry:
    br label %label1
  label1:                                                ; preds = %label_entry, %label6
    %op15 = phi i32 [ 0, %label_entry ], [ %op9, %label6 ]
    %op3 = icmp slt i32 %op15, 200000000
    %op4 = zext i1 %op3 to i32
    %op5 = icmp ne i32 %op4, 0
    br i1 %op5, label %label6, label %label10
  label6:                                                ; preds = %label1
    %op7 = call i32 @max()
    %op9 = add i32 %op15, 1
    br label %label1
  label10:                                                ; preds = %label1
    %op11 = load i32, i32* @opa
    call void @output(i32 %op11)
    %op12 = load i32, i32* @opb
    call void @output(i32 %op12)
    %op13 = load i32, i32* @opc
    call void @output(i32 %op13)
    %op14 = load i32, i32* @opd
    call void @output(i32 %op14)
    ret void
  }
  ```

  ```
  //优化后
  @opa = global i32 zeroinitializer
  @opb = global i32 zeroinitializer
  @opc = global i32 zeroinitializer
  @opd = global i32 zeroinitializer
  declare i32 @input()
  
  declare void @output(i32)
  
  declare void @outputFloat(float)
  
  declare void @neg_idx_except()
  
  define i32 @max() {
  label_entry:
    store i32 0, i32* @opa
    store i32 40320, i32* @opb
    store i32 362880, i32* @opc
    store i32 1814400, i32* @opd
    %op28 = load i32, i32* @opa
    %op29 = load i32, i32* @opb
    %op30 = icmp slt i32 %op28, %op29
    %op31 = zext i1 %op30 to i32
    %op32 = icmp ne i32 %op31, 0
    br i1 %op32, label %label33, label %label39
  label33:                                                ; preds = %label_entry
    %op34 = load i32, i32* @opb
    %op35 = load i32, i32* @opc
    %op36 = icmp slt i32 %op34, %op35
    %op37 = zext i1 %op36 to i32
    %op38 = icmp ne i32 %op37, 0
    br i1 %op38, label %label40, label %label46
  label39:                                                ; preds = %label_entry, %label46
    ret i32 0
  label40:                                                ; preds = %label33
    %op41 = load i32, i32* @opc
    %op42 = load i32, i32* @opd
    %op43 = icmp slt i32 %op41, %op42
    %op44 = zext i1 %op43 to i32
    %op45 = icmp ne i32 %op44, 0
    br i1 %op45, label %label47, label %label49
  label46:                                                ; preds = %label33, %label49
    br label %label39
  label47:                                                ; preds = %label40
    %op48 = load i32, i32* @opd
    ret i32 %op48
  label49:                                                ; preds = %label40
    br label %label46
  }
  define void @main() {
  label_entry:
    br label %label1
  label1:                                                ; preds = %label_entry, %label6
    %op15 = phi i32 [ 0, %label_entry ], [ %op9, %label6 ]
    %op3 = icmp slt i32 %op15, 200000000
    %op4 = zext i1 %op3 to i32
    %op5 = icmp ne i32 %op4, 0
    br i1 %op5, label %label6, label %label10
  label6:                                                ; preds = %label1
    %op7 = call i32 @max()
    %op9 = add i32 %op15, 1
    br label %label1
  label10:                                                ; preds = %label1
    %op11 = load i32, i32* @opa
    call void @output(i32 %op11)
    %op12 = load i32, i32* @opb
    call void @output(i32 %op12)
    %op13 = load i32, i32* @opc
    call void @output(i32 %op13)
    %op14 = load i32, i32* @opd
    call void @output(i32 %op14)
    ret void
  }
  ```

  可以看到所有右操作数均为常数的死代码已被删净，不过由于对`load`指令没有考虑周全，导致优化后的代码比`baseline`还要多5行死代码，其余基本相同。

* ### 循环不变式外提

  #### 实现思路：

  循环不变式外提的关键在于识别哪些计算是可以外提的，主要有以下两个方面：
  1. 如果某个计算指令的所有操作数在循环中没有被重新定值，那么这个指令就是循环不变的。
  2. 如果一个指令在循环中可能执行，也可能不执行，那么它不应该被外提。
  
  对于第1点，注意到在Mem2Reg执行后，除了数组和全局变量，循环中所有被定值的值都对应循环开始（即循环的base结点）处的一条phi指令。

  对于第二点，我们只需要检查该条指令所在的基本块是否支配循环的结尾（这里的“结尾”指，循环的base块的两个前驱块中在循环内的那个）。  
  （其实这样处理有一个漏洞，就是循环可能一次都不执行，这种情况可以用龙书P410的方式解决。但是助教提供的cmiunsf_builder并不会生成这样的循环，所以在这里暂且认为循环至少执行一次。）

  另外一个问题是关于嵌套循环的。如果一条指令从一个循环中被外提后处于另一个外层循环中，且该指令在外层循环中仍为循环不变的，那么应该继续外提。为了在做到这一点的同时避免对所有循环的反复遍历，这里将嵌套的循环结构看作一棵树，然后按照后序遍历的顺序依次处理每个循环，具体见下方的3。

  #### 相应代码：

  1. 
    ```c++
    //辅助函数，获得loop的直接子循环
    std::list<BBset_t *> get_sub_loops(BBset_t *loop, LoopSearch &ls)
    {
        std::list<BBset_t *> sub_loops;
        for(auto bb: *loop)
        {
            auto inner_loop = ls.get_inner_loop(bb);
            if(inner_loop && ls.get_parent_loop(inner_loop) == loop)
                sub_loops.push_back(ls.get_inner_loop(bb));
        }
        return sub_loops;
    }
    ```
  2. 
    ```c++
    //辅助函数，获得bb支配的所有基本块，存在doms里
    void get_all_doms(Dominators &d, BasicBlock *bb, std::unordered_set<BasicBlock *> &doms)
    {
        for(auto succ: d.get_dom_tree_succ_blocks(bb))
        {
            doms.emplace(succ);
            get_all_doms(d, succ, doms);
        }
    }
    ```
    这里需要用到`Dominators`的分析结果，所以在`LoopInvHoist::run()`函数开始处需要实例化一个`Dominators`并调用其`run()`。

  3. 
    ```c++
    //将所有循环的嵌套结构看作以没有外层循环的循环作为根的森林，对其中的每棵树生成后序遍历的顺序
    std::stack<BBset_t *> order;
    for(auto loop: loop_searcher)
    {
        if(loop_searcher.get_parent_loop(loop) == nullptr)
        {
            //辅助栈，用来对树进行深度优先遍历，每访问一个结点将其压入order栈
            std::stack<BBset_t *> make_order;
            make_order.push(loop);
            while(!make_order.empty())
            {
                auto l_ = make_order.top();
                make_order.pop();
                order.push(l_);
                for(auto sub: get_sub_loops(l_, loop_searcher))
                {
                    make_order.push(sub);
                }
            }
        }
    }
    ```
    这样处理结束后，order的出栈顺序即为后续处理循环的顺序。即
    ```c++
    //按照order中的顺序处理每一个循环，外提其中的不变式
    while(!order.empty())
    {
        auto loop = order.top();
        order.pop();
        //处理 loop
    }
    ```

    下面几段均为上述while循环中对一个loop的具体处理

  4. 
    ```c++
    //对一个循环的具体处理1
    //预处理，需要保存循环的base块、前驱块和结束块，并将前驱块的跳转指令保存后暂时移除

    auto base = loop_searcher.get_loop_base(loop);
    //循环的结束块和前驱块
    BasicBlock *end_of_loop, *pre_of_loop;
    for(auto pre: base->get_pre_basic_blocks())
    {
        if(loop->find(pre) != loop->end())
            end_of_loop = pre;
        else
            pre_of_loop = pre;
    }
    auto pre_terminator = pre_of_loop->get_terminator();
    pre_of_loop->delete_instr(pre_terminator);
    ```
  5. 
    ```c++
    //对一个循环的具体处理2
    //找循环中的定值，详细说明见下方
    std::unordered_set<Value *> changed_values;
    bool add_new = true;
    while(add_new)
    {
        add_new = false;
        for(auto bb: *loop)
        {
            for(auto instr: bb->get_instructions())
            {
                if(changed_values.find(instr) == changed_values.end())
                {
                    if(instr->is_load() || instr->is_phi())
                    {
                        changed_values.emplace(instr);
                        add_new = true;
                        continue;
                    }
                    for(auto operand: instr->get_operands())
                    {
                        if(changed_values.find(operand) != changed_values.end())
                        {
                            changed_values.emplace(instr);
                            add_new = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    ```
    对循环中的每条指令，如果  
      - 它是phi或load指令  
      - 他的某个操作数在`changed_values`集合中  
    则将其加入`changed_values`集合中。不断重复上述操作，直至`changed_values`集合不再发生变化。 

    这样处理后，如果一条计算指令的所有操作数都不在`changed_values`集合中，则可以将其外提。

  6. 
    ```c++
    //对一个循环的具体处理3
    //外提指令
    for(auto bb: *loop)
    {
        //获得bb支配的块
        std::unordered_set<BasicBlock *> bb_doms;
        get_all_doms(dom, bb, bb_doms);
        //只有在bb支配循环结束块时才对其进行处理
        if(bb == end_of_loop || bb_doms.find(end_of_loop) != bb_doms.end())
        {
            //将需要外提的指令存入一个列表中，然后移动
            std::list<Instruction *> instrs_to_move;
            for(auto instr: bb->get_instructions())
            {
                bool not_changed = true;
                switch(instr->get_instr_type())
                {
                case Instruction::add:
                case Instruction::sub:
                case Instruction::mul:
                case Instruction::sdiv:
                case Instruction::fadd:
                case Instruction::fsub:
                case Instruction::fmul:
                case Instruction::fdiv:
                case Instruction::cmp:
                case Instruction::fcmp:
                case Instruction::zext:
                case Instruction::fptosi:
                case Instruction::sitofp:
                    for(auto operand: instr->get_operands())
                    {
                        //遍历操作数，看它们在不在changed_values中
                        if(changed_values.find(operand) != changed_values.end())
                        {
                            not_changed = false;
                            break;
                        }
                    }
                    if(not_changed)
                        instrs_to_move.push_back(instr);
                    break;
                default:
                    break;
                }
            }
            for(auto instr: instrs_to_move)
            {
                pre_of_loop->add_instruction(instr);
                bb->delete_instr(instr);
            }
        }
    }
    //结束后将前驱的跳转指令补回
    pre_of_loop->add_instruction(pre_terminator);
    ```

  #### 优化前后的IR对比（举一个例子）并辅以简单说明：
  以testcases-2为例（为节约篇幅，将原本的`ret = (a*a*a*a*a*a*a*a*a*a)/a/a/a/a/a/a/a/a/a/a`简化为`ret = a*a`）：
  ```C
  void main(void){
    int i;
    int j;
    int a;
    int ret;

    i = 0;
    a = 2;

    while(i<10000000)
    {
        j = 0;

        while(j<2)
        {
            ret = a*a;
            j=j+1;
        }
        i=i+1;
    }
	  output(ret);
    return ;
  }
  ```
优化前的IR：
```llvm
declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define void @main() {
label_entry:
  br label %label4
label4:                                                ; preds = %label_entry, %label23
  %op26 = phi i32 [ %op29, %label23 ], [ undef, %label_entry ]
  %op27 = phi i32 [ 0, %label_entry ], [ %op25, %label23 ]
  %op28 = phi i32 [ %op30, %label23 ], [ undef, %label_entry ]
  %op6 = icmp slt i32 %op27, 10000000
  %op7 = zext i1 %op6 to i32
  %op8 = icmp ne i32 %op7, 0
  br i1 %op8, label %label9, label %label10
label9:                                                ; preds = %label4
  br label %label12
label10:                                                ; preds = %label4
  call void @output(i32 %op26)
  ret void
label12:                                                ; preds = %label9, %label17
  %op29 = phi i32 [ %op26, %label9 ], [ %op20, %label17 ]
  %op30 = phi i32 [ 0, %label9 ], [ %op22, %label17 ]
  %op14 = icmp slt i32 %op30, 2
  %op15 = zext i1 %op14 to i32
  %op16 = icmp ne i32 %op15, 0
  br i1 %op16, label %label17, label %label23
label17:                                                ; preds = %label12
  %op20 = mul i32 2, 2  ;在优化时将被外提的指令
  %op22 = add i32 %op30, 1
  br label %label12
label23:                                                ; preds = %label12
  %op25 = add i32 %op27, 1
  br label %label4
}

```
优化后：
```llvm
declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define void @main() {
label_entry:
  %op20 = mul i32 2, 2  ;被外提的指令
  br label %label4
label4:                                                ; preds = %label_entry, %label23
  %op26 = phi i32 [ %op29, %label23 ], [ undef, %label_entry ]
  %op27 = phi i32 [ 0, %label_entry ], [ %op25, %label23 ]
  %op28 = phi i32 [ %op30, %label23 ], [ undef, %label_entry ]
  %op6 = icmp slt i32 %op27, 10000000
  %op7 = zext i1 %op6 to i32
  %op8 = icmp ne i32 %op7, 0
  br i1 %op8, label %label9, label %label10
label9:                                                ; preds = %label4
  br label %label12
label10:                                                ; preds = %label4
  call void @output(i32 %op26)
  ret void
label12:                                                ; preds = %label9, %label17
  %op29 = phi i32 [ %op26, %label9 ], [ %op20, %label17 ]
  %op30 = phi i32 [ 0, %label9 ], [ %op22, %label17 ]
  %op14 = icmp slt i32 %op30, 2
  %op15 = zext i1 %op14 to i32
  %op16 = icmp ne i32 %op15, 0
  br i1 %op16, label %label17, label %label23
label17:                                                ; preds = %label12
  %op22 = add i32 %op30, 1
  br label %label12
label23:                                                ; preds = %label12
  %op25 = add i32 %op27, 1
  br label %label4
}
```
可以看到，优化前原本在`label17`中的`%op20 = mul i32 2, 2`被提至了`label_entry`中

* ### 活跃变量分析

  #### 实现思路：

  由书上的算法，为了实现活跃变量分析，需要先求出每个基本块的use和def变量集。通过遍历基本块中的每条指令，根据指令类型和操作数一一将各变量加入use和def中。对于特殊的phi指令，我们将phi指令的操作数统一存入该基本块的phi_use中，并且还要将该操作数存入跳转前的基本块对于的phi_out中，方便后面的活跃变量迭代分析。

  计算出use和def，以及phi_use和phi_out后，开始迭代计算活跃变量。由书上算法，我们不能确切知道循环的次数，所以我们在判断到一次迭代后各基本块的IN和OUT都不变时跳出循环。基本迭代算法同书上一样，phi指令的迭代算法和文档一样，但为了实现方便，除了phi_use外还定义了phi_all变量，具体见代码分析。

  #### 相应的代码：

  首先是指令分类，我把指令分成这几类，分别处理他们对use和def的影响。

  ```c++
  if(inst->isBinary() || inst->is_cmp() || inst->is_fcmp() || inst->is_gep() || inst->is_zext() || inst->is_fp2si() || inst->is_si2fp() || inst->is_load()){...}
  if(inst->is_ret()){...}
  if(inst->is_br()){...}
  if(inst->is_alloca()){...}
  if(inst->is_store()){...}
  if(inst->is_call()){...}
  if(inst->is_phi()){...}
  ```

  其次考虑各常量、函数等类型的操作数，我们用如下代码分辨。

  ```c++
  auto op = inst->get_operand(0);
  auto is_int_const = dynamic_cast<ConstantInt*>(op);
  auto is_float_const = dynamic_cast<ConstantFP*>(op);
  auto is_func = op->get_type()->is_function_type();
  ```

  对于phi指令，我们进行如下操作。首先明确phi_use[s][b]即为s中指令参数中label为b的对应变量集，phi_all[bb]即为bb中用到的phi操作数集。所以在检测到phi指令时，将对应操作数填入对应的集合中。

  ```c++
  if(!is_float_const && !is_int_const && !is_func){
      if(!op->get_type()->is_label_type()){
          phi_use[bb][dynamic_cast<BasicBlock*>(inst->get_operand(i + 1))].insert(op);
          phi_all[bb].insert(op);
      }
  }
  ```

  在进行迭代循环计算IN、OUT时，我们先用tem_in、tem_out存储临时in、out，通过比较tem_in和in、tem_out和out来判断循环是否需要终止(flag)。

  ```c++
  while(flag){
      flag = false;
      ...
      if(live_out[bb] != tem_out)flag = true;
      ...
      if(live_in[bb] != tem_in)flag = true;
      ...
  }
  ```

  先计算out。首先非phi操作数的in应当加入tem_out中，phi中操作数不加入tem_out的唯一情况是，phi_all[bb_succ]中有这个变量，但phi_use[bb_succ][bb]中没有，即这是bb_succ的另一个前驱产生的变量，所以不加入bb的tem_out。然后我们还要将phi_use[bb_succ][bb]中的变量加入tem_out。

  ```c++
  for(auto bb_succ: bb->get_succ_basic_blocks()){
      for(auto in: live_in[bb_succ]){
          if(phi_all[bb_succ].find(in) != phi_all[bb_succ].end() && 
              phi_use[bb_succ][bb].find(in) == phi_use[bb_succ][bb].end()){
                  continue;
              }
          tem_out.insert(in);
      }
      for(auto us: phi_use[bb_succ][bb]){
          tem_out.insert(us);
      }
  }
  if(live_out[bb] != tem_out)flag = true;
  live_out[bb] = tem_out;
  ```

  再计算in。先计算出tem_out-def，再将非phi的use加入，再将phi_use加入。

  ```c++
  for(auto delete_out: def[bb]){
      if(tem_out.find(delete_out) != tem_out.end()){
          tem_out.erase(delete_out);
      }
  }
  tem_in = tem_out;
  for(auto us: use[bb]){
      tem_in.insert(us);
  }
  for(auto bb_pre: bb->get_pre_basic_blocks()){
      for(auto us: phi_use[bb][bb_pre]){
          tem_in.insert(us);
      }
  }
  if(live_in[bb] != tem_in)flag = true;
  live_in[bb] = tem_in;
  ```

  这样完成了活跃变量分析。

### 实验总结

- 学习了c++更深层次的语法以及各种指针函数的使用
- 对使用复杂数据结构开发工程的流程有了一定了解
- 体会到了数据流分析的基本方法和处理逻辑
- 对编译的优化流程有了一定的了解，对常量传播、死代码删除、循环不变式外提和活跃变量分析有了更深刻的认识
- 提高了debug能力和团队合作能力

### 实验反馈 （可选 不会评分）

对本次实验的建议

### 组间交流 （可选）

本次实验和哪些组（记录组长学号）交流了哪一部分信息
