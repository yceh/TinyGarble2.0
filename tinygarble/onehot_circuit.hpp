#include "emp-tool/utils/block.h"
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>
#include <vector>
#include "./ggm_prf_tree.hpp"
#include "emp-tool/utils/hash.h"
std::vector<std::vector<bool>> clear_text_outer_product(std::vector<bool> vect_n,std::vector<bool> vect_b){
	std::vector<std::vector<bool>> out(vect_b.size());
	for(int b_idx=0;b_idx<vect_b.size();b_idx++){
		out[b_idx].resize(vect_n.size());
		for (int n_idx=0; n_idx<vect_n.size(); n_idx++) {
			out[b_idx][n_idx]=vect_n[n_idx]&&vect_b[b_idx];
		}
	}
	return out;
}
std::vector<bool> to_bool_vec(int val, int size){
	std::vector<bool> out(size);
	for(int idx=0;idx<size;idx++){
		out[idx]=val&1;
		val>>=1;
	}
	std::reverse(out.begin(), out.end());
	return out;
}
std::vector<std::vector<bool>> identity_truth_table(int bw){
    std::vector<std::vector<bool>> out(bw);
	for (int bit_idx=0; bit_idx<bw; bit_idx++) {
		out[bit_idx].resize(1<<bw);
	}
	for (int leaf_idx=0; leaf_idx<1<<bw; leaf_idx++) {
		auto temp=to_bool_vec(leaf_idx, bw);
		for (int bit_idx=0; bit_idx<bw; bit_idx++) {
			out[bit_idx][leaf_idx]=temp[bit_idx];
		}
	}
	return out;
}
struct One_Hot_Garble{
    std::vector<size_t> input_a;
    std::vector<size_t> input_b;
    //even first odd secend
    std::vector<size_t> level_text;
    std::vector<size_t> output_text;
    std::vector<std::vector<size_t>> output;//[|b|][n]
    // The truth table
    std::vector<std::vector<bool>> truth_table;// [|n|] [1<<|a|]
    // GT: to_evaluator
    // label: label for clear text value zero
    void allocate_idx(int a_bw,int b_bw,int& next_label_idx, int& next_text_ex_idx){
        input_a.resize(a_bw);
        input_b.resize(b_bw);
        output.resize(b_bw);
        //input a wire label indexes
        for (int a_idx=0; a_idx<a_bw; a_idx++) {
            input_a[a_idx]=next_label_idx++;
        }
        //input b wire label indexes
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            input_b[b_idx]=next_label_idx++;
        }
        #ifdef ONE_HOT_OUTER_PRODUCT
        // The 2^|a|*|m| onehot outer product label index
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            output[b_idx].resize(1<<a_bw);
            for (int a_n=0; a_n<(1<<a_bw); a_n++) {
                output[b_idx][a_n]=next_label_idx++;
            }
        }
        #else
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            output[b_idx].resize(truth_table.size());
            for (int a_n=0; a_n<(truth_table.size()); a_n++) {
                output[b_idx][a_n]=next_label_idx++;
            }
        }
        #endif
        //The 2*(n-1) cipher text to complete the tree
        level_text.resize(input_a.size());
        for (int a_idx=1; a_idx<a_bw; a_idx++) {
            level_text[a_idx]=next_text_ex_idx;
            next_text_ex_idx+=2;
        }
        //The m cipher text to reconstruct output
        output_text.resize(input_b.size());
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            output_text[b_idx]=next_text_ex_idx++;
        }
    }

    void garble(block* label, block* to_evaluator,const SequentialC2PC_SH& hash_provider){
        GGM_Hash_Tree_t hash_tree_for_0;
        compute_ggm_prf(label[input_a[0]], hash_tree_for_0, input_a.size(), hash_provider);
        //puts("Garbler zero tree 0\n");
        //print_tree(hash_tree_for_0);
        GGM_Hash_Tree_t hash_tree_for_1;
        block root_1=xorBlocks(*(label+input_a[0]), hash_provider.Delta);
        compute_ggm_prf(root_1, hash_tree_for_1, input_a.size(), hash_provider);
        //puts("Garbler zero tree 1\n");
        //print_tree(hash_tree_for_1);
        
        //|Output level cipher pairs|=|input_a|-1
        for (int level_idx=1; level_idx<input_a.size(); level_idx++) {
            block odd_temp=label[input_a[level_idx]];
            //print128_num("odd init", odd_temp);
            block even_temp=xorBlocks(odd_temp, hash_provider.Delta);
            //print128_num("even init", even_temp);
            for (int leaves_idx=0; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                //printf("level %d, even xor with %d leave\n",level_idx,leaves_idx);
                even_temp=xorBlocks(even_temp, hash_tree_for_0[level_idx][leaves_idx]);
                even_temp=xorBlocks(even_temp, hash_tree_for_1[level_idx][leaves_idx]);
                //print128_num("even temp", even_temp);
            }
            for (int leaves_idx=1; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                //printf("level %d, odd xor with %d leave\n",level_idx,leaves_idx);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_0[level_idx][leaves_idx]);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_1[level_idx][leaves_idx]);
                //print128_num("odd temp", odd_temp);
            }
            //printf("level %d, odd write to %lu even write to %lu\n",level_idx,level_text[level_idx]+1,level_text[level_idx]);
            to_evaluator[level_text[level_idx]]=even_temp;
            to_evaluator[level_text[level_idx]+1]=odd_temp;
        }
        
        for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
            to_evaluator[output_text[out_idx]]=label[input_b[out_idx]];
        }
        
        std::vector<block> all_leaves(hash_tree_for_1.back());
        std::vector<std::vector<block>> temp_one_hot_output_label;
        all_leaves.insert(all_leaves.end(),hash_tree_for_0.back().begin(),hash_tree_for_0.back().end());
        temp_one_hot_output_label.resize(all_leaves.size());
        for (int leaves_idx=0;leaves_idx<all_leaves.size();leaves_idx++) {
            temp_one_hot_output_label[leaves_idx].resize(input_b.size());
            for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
                block this_out;
                hash_provider.Hash(this_out, all_leaves[leaves_idx], out_idx);
                to_evaluator[output_text[out_idx]]=xorBlocks(to_evaluator[output_text[out_idx]], this_out );
                temp_one_hot_output_label[leaves_idx][out_idx]=this_out;
            }    
        }
        
        for (int output_idx=0; output_idx<input_b.size(); output_idx++) {
            for (int out_n_idx=0; out_n_idx<truth_table.size(); out_n_idx++) {
                block temp=zero_block();
                for (int leaf_idx=0; leaf_idx<all_leaves.size(); leaf_idx++) {
                    if (truth_table[out_n_idx][leaf_idx]) {
                        temp=xorBlocks(temp, temp_one_hot_output_label[leaf_idx][output_idx]);
                    }
                }
                label[output[output_idx][out_n_idx]]=temp;
            }
        }
    }
    /*
    * label : evaluated wire labels
    */
    void eval(block* label, const std::vector<bool>& clear_text_a ,block* from_garbler,const SequentialC2PC_SH& hash_provider){
        GGM_Hash_Tree_t hash_tree_a;
        GGM_Hash_Tree_t hash_tree_a_bar;
        compute_ggm_prf(label[input_a[0]], hash_tree_a, input_a.size(), hash_provider);
        //puts("Evaluator a tree");
        //print_tree(hash_tree_a);
        hash_tree_a_bar.resize(input_a.size());
        int missing_idx=0;
        for(int level_idx=1;level_idx<input_a.size();level_idx++){
            //fprintf(stderr, "level %d missing idx %d\n",level_idx,missing_idx);
            hash_tree_a_bar[level_idx].resize(1<<level_idx);
            //Fill entries on hash_tree_a_bar whose parent is not missing
            for (int leaves_idx=0; leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx++ ) {
                if(leaves_idx>>1==missing_idx){
                    hash_tree_a_bar[level_idx][leaves_idx]=zero_block();
                }else {
                    //printf("compute a bar level %d, idx %d from prev_level\n",level_idx,leaves_idx);
                    hash_provider.Hash(hash_tree_a_bar[level_idx][leaves_idx], hash_tree_a_bar[level_idx-1][leaves_idx/2],leaves_idx&1);
                }
            }
            //Fill entires that is not on the path from the 1 leaf to root on this level
            //printf("level %d access gabler in %zu\n",level_idx,level_text[level_idx]+!clear_text_a[level_idx]);
            auto garbler_text=xorBlocks(from_garbler[level_text[level_idx]+!clear_text_a[level_idx]],label[input_a[level_idx]]);
            //print128_num("from garbler", garbler_text);
            for(int leaves_idx=!clear_text_a[level_idx];leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx+=2){
                //printf("xoring with %d\n",leaves_idx);
                garbler_text=xorBlocks(hash_tree_a_bar[level_idx][leaves_idx],garbler_text);
                garbler_text=xorBlocks(hash_tree_a[level_idx][leaves_idx],garbler_text);
                //print128_num("eval garbler temp", garbler_text);
            }
            missing_idx*=2;
            hash_tree_a_bar[level_idx][missing_idx+!clear_text_a[level_idx]]=garbler_text;
            //printf("fill a bar level %d, idx %d from generator text\n",level_idx,missing_idx+!clear_text_a[level_idx]);
            missing_idx+=clear_text_a[level_idx];
        }
        block all_leaves_xor=from_garbler[output_text[0]];
        for (auto& a : hash_tree_a.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        for (auto& a : hash_tree_a_bar.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        //fprintf(stderr, "missing idx %d at the end\n",missing_idx);
        
        hash_tree_a_bar.back()[missing_idx]=all_leaves_xor;
        //puts("Evaluator a bar tree");
        //print_tree(hash_tree_a_bar);
        
        const auto& first_tree=clear_text_a[0]?hash_tree_a:hash_tree_a_bar;
        const auto& second_tree=clear_text_a[0]?hash_tree_a_bar:hash_tree_a;
        std::vector<block> last_level_leaves(first_tree.back());
        missing_idx+=clear_text_a[0]<<(clear_text_a.size()-1);
        last_level_leaves.insert(last_level_leaves.end(),second_tree.back().begin(),second_tree.back().end());
        std::vector<std::vector<block>> one_hot_outer_temp(last_level_leaves.size());
        for (int leaves_idx=0; leaves_idx<last_level_leaves.size(); leaves_idx++) {
            one_hot_outer_temp[leaves_idx].resize(input_b.size());
            if(leaves_idx==missing_idx){
                continue;
            }
            for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
                block this_out;
                hash_provider.Hash(this_out, last_level_leaves[leaves_idx], out_idx);
                from_garbler[output_text[out_idx]]=xorBlocks(from_garbler[output_text[out_idx]], this_out );
                one_hot_outer_temp[leaves_idx][out_idx]=this_out;
            }
        }
        for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
            one_hot_outer_temp[missing_idx][out_idx]=xorBlocks(from_garbler[output_text[out_idx]], label[input_b[out_idx]] );
            for(int output_n_idx=0;output_n_idx<truth_table.size();output_n_idx++){
                block temp=zero_block();
                for (int leaf_idx=0; leaf_idx<last_level_leaves.size(); leaf_idx++) {
                    if (truth_table[output_n_idx][leaf_idx]) {
                        temp=xorBlocks(temp, one_hot_outer_temp[leaf_idx][out_idx]);
                    }
                }
                label[output[out_idx][output_n_idx]]=temp;
            }
        }

    }
};
struct General_One_Hot_Outer_Prod{
    std::vector<size_t> input_a;
    std::vector<size_t> input_b;
    std::vector<std::vector<size_t>> output;//[|b|][n]
    One_Hot_Garble a_alp_outer_b;
    One_Hot_Garble b_beta_outer_alph;
    void copy_label(block* label,const std::vector<size_t>& src_idx,const std::vector<size_t>& dst_idx){
        if (src_idx.size()!=dst_idx.size()) {
            raise(SIGTRAP);
        }
        for (size_t idx=0; idx<src_idx.size(); idx++) {
            label[dst_idx[idx]]=label[src_idx[idx]];
        }
    }
    void flip_label_on_color_bit(block* label,const std::vector<size_t>& src_idx,const std::vector<size_t>& dst_idx,const SequentialC2PC_SH& hash_provider){
        if (src_idx.size()!=dst_idx.size()) {
            raise(SIGTRAP);
        }
        for (size_t idx=0; idx<src_idx.size(); idx++) {
            auto src_block=label[src_idx[idx]];
            label[dst_idx[idx]]=getLSB(src_block)?xorBlocks(src_block, hash_provider.Delta):src_block;
        }
    }
    std::vector<bool> get_color_bits(block* label,const std::vector<size_t>& label_idx){
        std::vector<bool> out(label_idx.size());
        for (int idx=0; idx<label_idx.size(); idx++) {
            out[idx]=getLSB(label[label_idx[idx]]);
        }
        return out;
    }
    void allocate_idx(int a_bw,int b_bw,int& next_label_idx, int& next_text_ex_idx){
        input_a.resize(a_bw);
        input_b.resize(b_bw);
        output.resize(b_bw);
        //input a wire label indexes
        for (int a_idx=0; a_idx<a_bw; a_idx++) {
            input_a[a_idx]=next_label_idx++;
        }
        //input b wire label indexes
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            input_b[b_idx]=next_label_idx++;
        }
        
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            output[b_idx].resize(a_bw);
            for (int a_n=0; a_n<(a_bw); a_n++) {
                output[b_idx][a_n]=next_label_idx++;
            }
        }
        a_alp_outer_b.truth_table=identity_truth_table(a_bw);
        b_beta_outer_alph.truth_table=identity_truth_table(b_bw);
        a_alp_outer_b.allocate_idx(a_bw, b_bw, next_label_idx, next_text_ex_idx);
        b_beta_outer_alph.allocate_idx(b_bw, a_bw, next_label_idx, next_text_ex_idx);
    }
    void garble(block* label, block* to_evaluator,const SequentialC2PC_SH& hash_provider){
        //The evalulator is going to take its share of a as if it is its share of a xor alpha
        // if alpha (color bit of a ) =1
        //      if a=0, a xor alpha=1, (eval taking same label as a) , W_(a xor alpha) ^ 1 = W_a ^0
        //      if a=1, a xor alpha=0,  , W_(a xor alpha) ^ 0 = W_a ^1
        // if alpha =0 
        //      if a=0, a xor alpha=0,   W_(a xor alpha) ^ 0 = W_a ^0
        //      if a=1, a xor alpha=1,   W_(a xor alpha) ^ 1 = W_a ^1
        // So if alpha =1, the label is flipped

        flip_label_on_color_bit(label, input_a, a_alp_outer_b.input_a, hash_provider);
        copy_label(label, input_b, a_alp_outer_b.input_b);

        flip_label_on_color_bit(label, input_b, b_beta_outer_alph.input_a, hash_provider);
        // Evaluator is going to always take its share of beta as zero. So
        // if beta=0 W_beta^0=0,W_beta^1=delta
        // if beta=1 W_beta^0=delta,W_beta^1=0, so eval always have the right share
        for (int beta_idx=0; beta_idx<b_beta_outer_alph.input_b.size(); beta_idx++) {
            label[b_beta_outer_alph.input_b[beta_idx]]=(getLSB(label[input_a[beta_idx]]))?hash_provider.Delta:zero_block();
        }

        a_alp_outer_b.garble(label, to_evaluator, hash_provider);
        b_beta_outer_alph.garble(label, to_evaluator, hash_provider);
        //compute alpha outer beta and see whether to flip the output
        auto alpha_xor_beta=clear_text_outer_product(get_color_bits(label, input_a), get_color_bits(label, input_b));
        for(int b_idx=0;b_idx<input_b.size();b_idx++){
            for(int a_idx=0;a_idx<input_a.size();a_idx++){
                label[output[b_idx][a_idx]]=xorBlocks(label[a_alp_outer_b.output[b_idx][a_idx]],label[b_beta_outer_alph.output[a_idx][b_idx]]);
                if (alpha_xor_beta[b_idx][a_idx]) {
                    label[output[b_idx][a_idx]]=xorBlocks(label[output[b_idx][a_idx]], hash_provider.Delta);
                }
            }
        }
    }
    void eval(block* label, block* to_evaluator,const SequentialC2PC_SH& hash_provider){
        copy_label(label, input_a, a_alp_outer_b.input_a);
        copy_label(label, input_b, a_alp_outer_b.input_b);

        copy_label(label, input_b, b_beta_outer_alph.input_a);
        for (int beta_idx=0; beta_idx<b_beta_outer_alph.input_b.size(); beta_idx++) {
            label[b_beta_outer_alph.input_b[beta_idx]]=zero_block();
        }

        a_alp_outer_b.eval(label, get_color_bits(label, input_a),to_evaluator, hash_provider);
        b_beta_outer_alph.eval(label, get_color_bits(label, input_b), to_evaluator, hash_provider);

        for(int b_idx=0;b_idx<input_b.size();b_idx++){
            for(int a_idx=0;a_idx<input_a.size();a_idx++){
                label[output[b_idx][a_idx]]=xorBlocks(label[a_alp_outer_b.output[b_idx][a_idx]],label[b_beta_outer_alph.output[a_idx][b_idx]]);
            }
        }
    }

};