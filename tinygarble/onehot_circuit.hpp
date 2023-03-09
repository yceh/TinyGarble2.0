#include "emp-tool/utils/block.h"
#include <cstddef>
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>
#include <vector>
#include "./ggm_prf_tree.hpp"
#include "emp-tool/utils/hash.h"
struct One_Hot_Garble{
    std::vector<size_t> input_a;
    std::vector<size_t> input_b;
    //even first odd secend
    std::vector<size_t> level_text;
    std::vector<size_t> output_text;
    std::vector<std::vector<size_t>> output;
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
        // The 2^|a|*|m| onehot outer product label index
        for (int b_idx=0; b_idx<b_bw; b_idx++) {
            output[b_idx].resize(1<<a_bw);
            for (int a_n=0; a_n<(1<<a_bw); a_n++) {
                output[b_idx][a_n]=next_label_idx++;
            }
        }
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
        puts("Garbler zero tree 0\n");
        print_tree(hash_tree_for_0);
        GGM_Hash_Tree_t hash_tree_for_1;
        block root_1=xorBlocks(*(label+input_a[0]), hash_provider.Delta);
        compute_ggm_prf(root_1, hash_tree_for_1, input_a.size(), hash_provider);
        puts("Garbler zero tree 1\n");
        print_tree(hash_tree_for_1);
        
        //|Output level cipher pairs|=|input_a|-1
        for (int level_idx=1; level_idx<input_a.size(); level_idx++) {
            block odd_temp=label[input_a[level_idx]];
            print128_num("odd init", odd_temp);
            block even_temp=xorBlocks(odd_temp, hash_provider.Delta);
            print128_num("even init", even_temp);
            for (int leaves_idx=0; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                printf("level %d, even xor with %d leave\n",level_idx,leaves_idx);
                even_temp=xorBlocks(even_temp, hash_tree_for_0[level_idx][leaves_idx]);
                even_temp=xorBlocks(even_temp, hash_tree_for_1[level_idx][leaves_idx]);
                print128_num("even temp", even_temp);
            }
            for (int leaves_idx=1; leaves_idx<hash_tree_for_0[level_idx].size(); leaves_idx+=2) {
                printf("level %d, odd xor with %d leave\n",level_idx,leaves_idx);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_0[level_idx][leaves_idx]);
                odd_temp=xorBlocks(odd_temp, hash_tree_for_1[level_idx][leaves_idx]);
                print128_num("odd temp", odd_temp);
            }
            printf("level %d, odd write to %lu even write to %lu\n",level_idx,level_text[level_idx]+1,level_text[level_idx]);
            to_evaluator[level_text[level_idx]]=even_temp;
            to_evaluator[level_text[level_idx]+1]=odd_temp;
        }
        
        for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
            to_evaluator[output_text[out_idx]]=label[input_b[out_idx]];
        }
        
        std::vector<block> all_leaves(hash_tree_for_1.back());
        all_leaves.insert(all_leaves.end(),hash_tree_for_0.back().begin(),hash_tree_for_0.back().end());
        for (int leaves_idx=0;leaves_idx<all_leaves.size();leaves_idx++) {
            for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
                block this_out;
                hash_provider.Hash(this_out, all_leaves[leaves_idx], out_idx);
                to_evaluator[output_text[out_idx]]=xorBlocks(to_evaluator[output_text[out_idx]], this_out );
                label[output[out_idx][leaves_idx]]=this_out;
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
        puts("Evaluator a tree");
        print_tree(hash_tree_a);
        hash_tree_a_bar.resize(input_a.size());
        int missing_idx=0;
        for(int level_idx=1;level_idx<input_a.size();level_idx++){
            fprintf(stderr, "level %d missing idx %d\n",level_idx,missing_idx);
            hash_tree_a_bar[level_idx].resize(1<<level_idx);
            //Fill entries on hash_tree_a_bar whose parent is not missing
            for (int leaves_idx=0; leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx++ ) {
                if(leaves_idx>>1==missing_idx){
                    hash_tree_a_bar[level_idx][leaves_idx]=zero_block();
                }else {
                    printf("compute a bar level %d, idx %d from prev_level\n",level_idx,leaves_idx);
                    hash_provider.Hash(hash_tree_a_bar[level_idx][leaves_idx], hash_tree_a_bar[level_idx-1][leaves_idx/2],leaves_idx&1);
                }
            }
            //Fill entires that is not on the path from the 1 leaf to root on this level
            printf("level %d access gabler in %zu\n",level_idx,level_text[level_idx]+!clear_text_a[level_idx]);
            auto garbler_text=xorBlocks(from_garbler[level_text[level_idx]+!clear_text_a[level_idx]],label[input_a[level_idx]]);
            print128_num("from garbler", garbler_text);
            for(int leaves_idx=!clear_text_a[level_idx];leaves_idx<hash_tree_a_bar[level_idx].size();leaves_idx+=2){
                printf("xoring with %d\n",leaves_idx);
                garbler_text=xorBlocks(hash_tree_a_bar[level_idx][leaves_idx],garbler_text);
                garbler_text=xorBlocks(hash_tree_a[level_idx][leaves_idx],garbler_text);
                print128_num("eval garbler temp", garbler_text);
            }
            missing_idx*=2;
            hash_tree_a_bar[level_idx][missing_idx+!clear_text_a[level_idx]]=garbler_text;
            printf("fill a bar level %d, idx %d from generator text\n",level_idx,missing_idx+!clear_text_a[level_idx]);
            missing_idx+=clear_text_a[level_idx];
        }
        block all_leaves_xor=from_garbler[output_text[0]];
        for (auto& a : hash_tree_a.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        for (auto& a : hash_tree_a_bar.back()) {
            all_leaves_xor=xorBlocks(all_leaves_xor, a);
        }
        fprintf(stderr, "missing idx %d at the end\n",missing_idx);
        
        hash_tree_a_bar.back()[missing_idx]=all_leaves_xor;
        puts("Evaluator a bar tree");
        print_tree(hash_tree_a_bar);
        
        const auto& first_tree=clear_text_a[0]?hash_tree_a:hash_tree_a_bar;
        const auto& second_tree=clear_text_a[0]?hash_tree_a_bar:hash_tree_a;
        std::vector<block> last_level_leaves(first_tree.back());
        missing_idx+=clear_text_a[0]<<(clear_text_a.size()-1);
        last_level_leaves.insert(last_level_leaves.end(),second_tree.back().begin(),second_tree.back().end());
        for (int leaves_idx=0; leaves_idx<last_level_leaves.size(); leaves_idx++) {
            if(leaves_idx==missing_idx){
                continue;
            }
            for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
                block this_out;
                hash_provider.Hash(this_out, last_level_leaves[leaves_idx], out_idx);
                from_garbler[output_text[out_idx]]=xorBlocks(from_garbler[output_text[out_idx]], this_out );
                label[output[out_idx][leaves_idx]]=this_out;
            }
        }
        for (int out_idx=0; out_idx<input_b.size(); out_idx++) {
            label[output[out_idx][missing_idx]]=xorBlocks(from_garbler[output_text[out_idx]], label[input_b[out_idx]] );
        }        
    }
};
