#include "ActiveVars.hpp"

void ActiveVars::run()
{
    std::ofstream output_active_vars;
    output_active_vars.open("active_vars.json", std::ios::out);
    output_active_vars << "[";
    for (auto &func : this->m_->get_functions()) {
        if (func->get_basic_blocks().empty()) {
            continue;
        }
        else
        {
            func_ = func;  

            func_->set_instr_name();
            live_in.clear();
            live_out.clear();
            
            // 在此分析 func_ 的每个bb块的活跃变量，并存储在 live_in live_out 结构内
            //std::map<BasicBlock*, std::set<Value*>> use;
            std::map<BasicBlock*, std::set<Value*>> use, def, phi_all;
            std::map<BasicBlock*, std::map<BasicBlock*, std::set<Value*>>> phi_use;
            /*for(auto bb: func_->get_basic_blocks()){
                use.insert(bb, {});
                def.insert(bb, {});
            }*/
            for(auto bb: func_->get_basic_blocks()){
                for(auto inst: bb->get_instructions()){
                    if(inst->isBinary() || inst->is_cmp() || inst->is_fcmp() || inst->is_gep() || inst->is_zext()\
                    || inst->is_fp2si() || inst->is_si2fp() || inst->is_load()){
                        for(auto op: inst->get_operands()){
                            auto is_int_const = dynamic_cast<ConstantInt*>(op);
                            auto is_float_const = dynamic_cast<ConstantFP*>(op);
                            auto is_func = op->get_type()->is_function_type();
                            if(!is_float_const && !is_int_const && !is_func){
                                if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                            }
                        }
                        def[bb].insert(inst);
                        continue;
                    }
                    if(inst->is_ret()){
                        if(inst->get_num_operand() == 1){
                            auto op = inst->get_operand(0);
                            auto is_int_const = dynamic_cast<ConstantInt*>(op);
                            auto is_float_const = dynamic_cast<ConstantFP*>(op);
                            auto is_func = op->get_type()->is_function_type();
                            if(!is_float_const && !is_int_const && !is_func){
                                if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                            }
                        }
                        continue;
                    }
                    if(inst->is_br()){
                        for(auto op: inst->get_operands()){
                            if(!op->get_type()->is_label_type()){
                                if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                            }
                        }
                        continue;
                    }
                    if(inst->is_alloca()){
                        def[bb].insert(inst);
                        continue;
                    }
                    if(inst->is_store()){
                        for(auto op: inst->get_operands()){
                            auto is_int_const = dynamic_cast<ConstantInt*>(op);
                            auto is_float_const = dynamic_cast<ConstantFP*>(op);
                            auto is_func = op->get_type()->is_function_type();
                            if(!is_float_const && !is_int_const && !is_func){
                                if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                            }
                        }
                        continue;
                    }
                    if(inst->is_call()){
                        for(auto op: inst->get_operands()){
                            auto is_int_const = dynamic_cast<ConstantInt*>(op);
                            auto is_float_const = dynamic_cast<ConstantFP*>(op);
                            auto is_func = op->get_type()->is_function_type();
                            if(!is_float_const && !is_int_const && !is_func){
                                if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                            }
                        }
                        if(!inst->is_void())def[bb].insert(inst);
                        continue;
                    }
                    if(inst->is_phi()){
                        for(int i = 0; i < inst->get_num_operand(); i++){
                            auto op = inst->get_operand(i);
                            auto is_int_const = dynamic_cast<ConstantInt*>(op);
                            auto is_float_const = dynamic_cast<ConstantFP*>(op);
                            auto is_func = op->get_type()->is_function_type();
                            if(!is_float_const && !is_int_const && !is_func){
                                if(!op->get_type()->is_label_type()){
                                    //if(def[bb].find(op) == def[bb].end()) use[bb].insert(op);
                                    //phi_out[dynamic_cast<BasicBlock*>(inst->get_operand(i+1))].insert(op);
                                    //phi_use[bb].insert(op);
                                    phi_use[bb][dynamic_cast<BasicBlock*>(inst->get_operand(i + 1))].insert(op);
                                    phi_all[bb].insert(op);
                                    //std::cout<<bb->get_name()<<"|"<<op->get_name()<<std::endl;
                                }
                            }
                        }
                        def[bb].insert(inst);
                        continue;
                    }
                }
            }
            
            bool flag = true;
            //std::cout << "hello!\n" ;
            while(flag){
                flag = false;
                for(auto bb: func_->get_basic_blocks()){
                    std::set<Value *> tem_in, tem_out;
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
                    /*for(auto us: phi_use[bb]){
                        tem_in.insert(us);
                    }*/
                    if(live_in[bb] != tem_in)flag = true;
                    live_in[bb] = tem_in;
                }
            } 

            output_active_vars << print();
            output_active_vars << ",";
        }
    }
    output_active_vars << "]";
    output_active_vars.close();
    return ;
}

std::string ActiveVars::print()
{
    std::string active_vars;
    active_vars +=  "{\n";
    active_vars +=  "\"function\": \"";
    active_vars +=  func_->get_name();
    active_vars +=  "\",\n";

    active_vars +=  "\"live_in\": {\n";
    for (auto &p : live_in) {
        if (p.second.size() == 0) {
            continue;
        } else {
            active_vars +=  "  \"";
            active_vars +=  p.first->get_name();
            active_vars +=  "\": [" ;
            for (auto &v : p.second) {
                active_vars +=  "\"%";
                active_vars +=  v->get_name();
                active_vars +=  "\",";
            }
            active_vars += "]" ;
            active_vars += ",\n";   
        }
    }
    active_vars += "\n";
    active_vars +=  "    },\n";
    
    active_vars +=  "\"live_out\": {\n";
    for (auto &p : live_out) {
        if (p.second.size() == 0) {
            continue;
        } else {
            active_vars +=  "  \"";
            active_vars +=  p.first->get_name();
            active_vars +=  "\": [" ;
            for (auto &v : p.second) {
                active_vars +=  "\"%";
                active_vars +=  v->get_name();
                active_vars +=  "\",";
            }
            active_vars += "]";
            active_vars += ",\n";
        }
    }
    active_vars += "\n";
    active_vars += "    }\n";

    active_vars += "}\n";
    active_vars += "\n";
    return active_vars;
}