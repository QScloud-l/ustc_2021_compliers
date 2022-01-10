#include "ConstPropagation.hpp"
#include "logging.hpp"

// 给出了返回整形值的常数折叠实现，大家可以参考，在此基础上拓展
// 当然如果同学们有更好的方式，不强求使用下面这种方式
ConstantInt *ConstFolder::compute(
    Instruction::OpID op,
    ConstantInt *value1,
    ConstantInt *value2)
{

    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();
    switch (op)
    {
    case Instruction::add:
        return ConstantInt::get(c_value1 + c_value2, module_);

        break;
    case Instruction::sub:
        return ConstantInt::get(c_value1 - c_value2, module_);
        break;
    case Instruction::mul:
        return ConstantInt::get(c_value1 * c_value2, module_);
        break;
    case Instruction::sdiv:
        return ConstantInt::get((int)(c_value1 / c_value2), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantFP *ConstFolder::fcompute(
    Instruction::OpID op,
    ConstantFP *value1,
    ConstantFP *value2)
{

    float c_value1 = value1->get_value();
    float c_value2 = value2->get_value();
    switch (op)
    {
    case Instruction::fadd:
        return ConstantFP::get(c_value1 + c_value2, module_);

        break;
    case Instruction::fsub:
        return ConstantFP::get(c_value1 - c_value2, module_);
        break;
    case Instruction::fmul:
        return ConstantFP::get(c_value1 * c_value2, module_);
        break;
    case Instruction::fdiv:
        return ConstantFP::get((float)(c_value1 / c_value2), module_);
        break;
    default:
        return nullptr;
        break;
    }
}

// 用来判断value是否为ConstantFP，如果不是则会返回nullptr
ConstantFP *cast_constantfp(Value *value)
{
    auto constant_fp_ptr = dynamic_cast<ConstantFP *>(value);
    if (constant_fp_ptr)
    {
        return constant_fp_ptr;
    }
    else
    {
        return nullptr;
    }
}
ConstantInt *cast_constantint(Value *value)
{
    auto constant_int_ptr = dynamic_cast<ConstantInt *>(value);
    if (constant_int_ptr)
    {
        return constant_int_ptr;
    }
    else
    {
        return nullptr;
    }
}

//deal with cmp(int)
ConstantInt *ConstFolder::compute_cmp(
    CmpInst::CmpOp op,
    ConstantInt *value1,
    ConstantInt *value2)
{
    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();
    switch (op)
    {
        case CmpInst::EQ:
            return ConstantInt::get(c_value1 == c_value2, module_);
            break;
        case CmpInst::NE:
            return ConstantInt::get(c_value1 != c_value2, module_);
            break;
        case CmpInst::GT:
            return ConstantInt::get(c_value1 > c_value2, module_);
            break;
        case CmpInst::GE:
            return ConstantInt::get(c_value1 >= c_value2, module_);
            break;
        case CmpInst::LT:
            return ConstantInt::get(c_value1 < c_value2, module_);
            break;
        case CmpInst::LE:
            return ConstantInt::get(c_value1 <= c_value2, module_);
            break;
        default:
            return nullptr;
            break;
    }
}

//deal with cmp(float)
ConstantInt *ConstFolder::compute_cmpFP(
    FCmpInst::CmpOp op,
    ConstantFP *value1,
    ConstantFP *value2)
{
    float c_value1 = value1->get_value();
    float c_value2 = value2->get_value();
    float temp = abs(c_value1 - c_value2);
    switch (op)
    {
        case CmpInst::EQ:
            return ConstantInt::get(temp < 1e-7, module_);
            break;
        case CmpInst::NE:
            return ConstantInt::get(temp > 1e-7, module_);
            break;
        case CmpInst::GT:
            return ConstantInt::get(c_value1 > c_value2, module_);
            break;
        case CmpInst::GE:
            return ConstantInt::get(c_value1 >= c_value2, module_);
            break;
        case CmpInst::LT:
            return ConstantInt::get(c_value1 < c_value2, module_);
            break;
        case CmpInst::LE:
            return ConstantInt::get(c_value1 <= c_value2, module_);
            break;
        default:
            return nullptr;
            break;
    }
}

void ConstPropagation::run()
{   
    // 从这里开始吧！
    std::vector<Instruction*> wait_delete;
    ConstFolder cf(m_);
    for(auto func:m_->get_functions())
    {
        for(auto block:func->get_basic_blocks())
        {
            std::vector<Instruction*> wait_delete;
            for(auto instr:block->get_instructions())
            {
                if(instr->is_add()||instr->is_sub()||instr->is_mul()||instr->is_div()||instr->is_cmp())
                {
                    auto l_val = instr->get_operand(0);
                    auto r_val = instr->get_operand(1);
                    if(cast_constantint(l_val)&&cast_constantint(r_val))
                    {
                        if(instr->is_cmp())
                        {
                            auto op=static_cast<CmpInst *>(instr)->get_cmp_op();
                            auto judge=cf.compute_cmp(op,cast_constantint(l_val),cast_constantint(r_val));
                            instr->replace_all_use_with(judge);
                        }
                        else
                        {
                            Instruction::OpID op = instr->get_instr_type();
                            auto unite=cf.compute(op,cast_constantint(l_val),cast_constantint(r_val));
                            instr->replace_all_use_with(unite);
                        }
                        wait_delete.push_back(instr);
                    }
                }
                if(instr->is_fadd()||instr->is_fsub()||instr->is_fmul()||instr->is_fdiv()||instr->is_fcmp())
                {
                    auto l_val = instr->get_operand(0);
                    auto r_val = instr->get_operand(1);
                    if(cast_constantfp(l_val)&&cast_constantfp(r_val))
                    {
                        if(instr->is_fcmp())
                        {
                            auto op=dynamic_cast<FCmpInst *>(instr)->get_cmp_op();
                            auto judge=cf.compute_cmpFP(op,cast_constantfp(l_val),cast_constantfp(r_val));
                            instr->replace_all_use_with(judge);
                        }
                        else
                        {
                            auto op = instr->get_instr_type();
                            auto unite=cf.fcompute(op,cast_constantfp(l_val),cast_constantfp(r_val));
                            instr->replace_all_use_with(unite);
                        }
                        wait_delete.push_back(instr);
                    }
                }
                if(instr->is_fp2si())
                {
                    auto l_val = instr->get_operand(0);
                    if(cast_constantfp(l_val))
                    {
                        int temp=cast_constantfp(l_val)->get_value();
                        auto t=ConstantInt::get(temp, m_);
                        instr->replace_all_use_with(t);
                        wait_delete.push_back(instr);
                    }
                }
                if(instr->is_si2fp())
                {
                    auto l_val = instr->get_operand(0);
                    if(cast_constantint(l_val))
                    {
                        float temp=cast_constantint(l_val)->get_value();
                        auto t=ConstantFP::get(temp, m_);
                        instr->replace_all_use_with(t);
                        wait_delete.push_back(instr);
                    }
                }
            }
            for(auto in: wait_delete){
                block->delete_instr(in);
            }
        }
    }
}