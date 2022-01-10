# Lab4 实验报告-阶段一

吕涵祺 PB19111654

孙佳桢 PB19111651

赵玉宇 PB19111653

## 实验要求

阅读LoopSearch和Mem2Reg两部分优化代码，理解优化目的和操作过程，并回答思考题，学习利用IR接口写优化代码的方法。。

## 思考题
### LoopSearch
1. LoopSearch中直接用于描述一个循环的数据结构是什么？需要给出其具体类型。
   > CFGNodePtrSet，即std::unordered_set<CFGNode*>，CFGNode定义见代码。
2. 循环入口是重要的信息，请指出LoopSearch中如何获取一个循环的入口？需要指出具体代码，并解释思路。
   > LoopSearch认为，对于一个循环的任意一个BasicBlock，如果他有一个前驱不在循环内，那么他就是这个循环的入口，见代码如下：
   > ```c++
   > CFGNodePtr base = nullptr;
   >  for (auto n : *set)
   >  {
   >      for (auto prev : n->prevs)
   >      {
   >          if (set->find(prev) == set->end())
   >          {
   >              base = n;
   >          }
   >      }
   >  }
   >  if (base != nullptr)
   >      return base;
   > ```
   > set为loop内的bb集合，对于每个bb，如果它的前驱不在set中，即将它赋值给base，赋值成功则return。
   >
   >
   > 而对于所有结点前驱都在set内的loop，由于base在run的while循环中被删去许多，我们去reserved中寻找后继在set内的结点，将它的后继赋值给loop的base。见代码如下：
   > ```c++
   > for (auto res : reserved)
   >  {
   >      for (auto succ : res->succs)
   >      {
   >          if (set->find(succ) != set->end())
   >          {
   >              base = succ;
   >          }
   >      }
   >  }
   > 
   >  return base; 
3. 仅仅找出强连通分量并不能表达嵌套循环的结构。为了处理嵌套循环，LoopSearch在Tarjan algorithm的基础之上做了什么特殊处理？
   > LoopSearch多次调用了Tarjan algorithm。每调用一次Tarjan algorithm，LoopSearch便删掉每个loop的base结点，破坏了外层循环，从而通过再次调用Tarjan algorithm得到新的内层loop。而且我们调用Tarjan algorithm时，删掉了size为1的强连通分量，意味着留下来的都是loop而不是其他bb。

4. 某个基本块可以属于多层循环中，LoopSearch找出其所属的最内层循环的思路是什么？这里需要用到什么数据？这些数据在何时被维护？需要指出数据的引用与维护的代码，并简要分析。
   > 对于每个基本块，需要通过bb2base获取最内层循环的base，再通过base2loop得到最内层循环。需要用到bb2base和base2loop。这些数据在每次调用Tarjan algorithm后得到新的loop时更新维护。见代码如下：
   > ```c++
   > loop_set.insert(bb_set);
   > func2loop[func].insert(bb_set);
   > base2loop.insert({base->bb, bb_set});
   > loop2base.insert({bb_set, base->bb});
   >                      
   > // step 5: map each node to loop base
   > for (auto bb : *bb_set)
   > {
   >     if (bb2base.find(bb) == bb2base.end())
   >         bb2base.insert({bb, base->bb});
   >     else
   >         bb2base[bb] = base->bb;
   > }
   > ```
   > 前四行都是插入新得到的内层loop的信息。对于step5，若bb不在map中，插入bb-base对；若bb已经在map中，证明它在外层循环中已经存入过map，为了让map中存储bb内层loop的base，需要替换掉旧的值。
### Mem2reg
1. 请简述概念：支配性、严格支配性、直接支配性、支配边界。
   > - 支配性：若从一个流图的入口到达结点n一定会经过结点d，则称d支配n
   > - 严格支配性：若a支配b且a不等于b，则称a严格支配b
   > - 直接支配性：在所有严格支配结点a的结点中，设b为最接近结点a的结点，则称b直接支配a
   > - 支配边界：对于结点n，若n支配a的某个前驱，但n不严格支配a，则称a为n的支配边界
2. phi节点是SSA的关键特征，请简述phi节点的概念，以及引入phi节点的理由。
   > phi节点是一个指令，在有不同分支多次赋值一个变量的情况下，用来选择给变量赋哪个值。
   > 
   > 因为SSA要求每个被赋值的变量都必须具有不同的名字，所以对于分支赋值的情况，运行前无法判断最终会被哪条分支赋值，所以引入phi节点新命名一个运行时确定的变量用来继续操作。另外，phi指令能够复用变量，提供了一种删去多余load和store指令的机会。
3. 下面给出的cminus代码显然不是SSA的，后面是使用lab3的功能将其生成的LLVM IR（未加任何Pass），说明对一个变量的多次赋值变成了什么形式？
   > 非SSA的IR中对同一个变量的多次赋值形式为多次load该变量至一临时变量中，进行运算后再store回原地址。
4. 对下面给出的cminus程序，使用lab3的功能，分别关闭/开启Mem2Reg生成LLVM IR。对比生成的两段LLVM IR，开启Mem2Reg后，每条load, store指令发生了变化吗？变化或者没变化的原因是什么？请分类解释。
   > 大部分的load和store都被删除了，原因是Mem2Reg在将涉及到非GLOBAL_VARIABLE和非GEP_INSTR的load指令结果的变量重命名后，在从非GLOBAL_VARIABLE和非GEP_INSTR的store指令中获取新名字后，将这些load和store指令放入了wait_delete中，最终被删除。
   > 
   > 对于这个例子而言，剩下```store i32 1, i32* @globVar```是因为globVar为全局变量，剩下```store i32 999, i32* %op5```是因为%op5为gep结果。

5. 指出放置phi节点的代码，并解释是如何使用支配树的信息的。需要给出代码中的成员变量或成员函数名称。
   > 放置phi节点的代码如下：
   > ```c++
   > std::vector<BasicBlock *> work_list;
   > work_list.assign(live_var_2blocks[var].begin(), live_var_2blocks[var].end());
   > for (int i =0 ; i < work_list.size() ; i++ )
   > {   
   >     auto bb = work_list[i];
   >     for ( auto bb_dominance_frontier_bb : dominators_->get_dominance_frontier(bb))
   >     {
   >         if ( bb_has_var_phi.find({bb_dominance_frontier_bb, var}) == bb_has_var_phi.end() )
   >         { 
   >             // generate phi for bb_dominance_frontier_bb & add bb_dominance_frontier_bb to work list
   >             auto phi = PhiInst::create_phi(var->get_type()->get_pointer_element_type(), bb_dominance_frontier_bb);
   >             phi->set_lval(var);
   >             bb_dominance_frontier_bb->add_instr_begin( phi );
   >             work_list.push_back( bb_dominance_frontier_bb );
   >             bb_has_var_phi[{bb_dominance_frontier_bb, var}] = true;
   >         }
   >     }
   > }
   > ```
   > 首先对于var而言，先将work_list用支配树的live_var_2blocks[var]初始化，再遍历work_list。对于每个work_list中的bb，调用get_dominance_frontier()从支配树中获取bb的支配边界集，再用bb_dominance_frontier_bb遍历这个支配边界集。如果bb_dominance_frontier_bb中没有对应bb和var的phi节点，就在bb_dominance_frontier_bb插入一个新phi节点，lvar = var，并且将bb_dominance_frontier_bb插入到var的work_list中。
### 代码阅读总结

> 对于第一部分，理解了代码分析中流图的抽象，学习了tarjan算法在实际中的应用和扩展，对循环的流图表现形式有了更明确的认知。对于第二部分，理解了SSA相对于原生IR的优化机制和操作流程，对phi节点、支配树和重命名等技术有了了解，也学习到了更多的IR函数和使用方法，对优化有了部分感性认知。此外，还学到了一些编写优化代码的方法。

### 实验反馈 （可选 不会评分）

如果不是为了增加实验难度的话，代码的注释有一点少，有些变量很难理解意义，如loopsearch中的reserved。第二部分的附加材料对支配树的一些基础概念缺少介绍，需要自己检索。

### 组间交流 （可选）

无
