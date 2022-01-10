#include <algorithm>
#include "logging.hpp"
#include "LoopSearch.hpp"
#include "LoopInvHoist.hpp"
#include "Dominators.h"
#include <stack>

std::list<BBset_t *> get_sub_loops(BBset_t *loop, LoopSearch &ls);
void get_all_doms(Dominators &d, BasicBlock *bb, std::unordered_set<BasicBlock *> &doms);

void LoopInvHoist::run()
{
    // 先通过LoopSearch获取循环的相关信息
    LoopSearch loop_searcher(m_, false);
    loop_searcher.run();
    Dominators dom(m_);
    dom.run();

    // 接下来由你来补充啦！

    //将所有循环的嵌套结构看作以没有外层循环的循环作为根的森林，对其中的每棵树生成后序遍历的顺序
    std::stack<BBset_t *> order;
    for(auto loop: loop_searcher)
    {
        if(loop_searcher.get_parent_loop(loop) == nullptr)
        {
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
    //按照order中的顺序处理每一个循环，外提其中的不变式
    while(!order.empty())
    {
        auto loop = order.top();
        order.pop();
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
        //找循环中的定值
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
                    //if
                    }
                }
            }
        }
        for(auto bb: *loop)
        {
            //获得bb支配的块
            std::unordered_set<BasicBlock *> bb_doms;
            get_all_doms(dom, bb, bb_doms);
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
        pre_of_loop->add_instruction(pre_terminator);
    }
}

//获得bb支配的所有基本块，存在doms里
void get_all_doms(Dominators &d, BasicBlock *bb, std::unordered_set<BasicBlock *> &doms)
{
    for(auto succ: d.get_dom_tree_succ_blocks(bb))
    {
        doms.emplace(succ);
        get_all_doms(d, succ, doms);
    }
}

//获得loop的直接子循环
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