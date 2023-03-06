#include "sequential_2pc_sh.h"
#include <cstdio>
typedef std::vector<std::vector<block>> GGM_Hash_Tree_t;
void print128_num(char* prefix, block var) 
{
    int64_t v64val[2];
    memcpy(v64val, &var, sizeof(v64val));
    printf("%s: %ld %ld\n",prefix, v64val[1], v64val[0]);
}
void print_tree(const std::vector<std::vector<block>>& in){
    for (const auto& level : in) {
        for(const auto & ele:level){
            int64_t v64val[2];
            memcpy(v64val, &ele, sizeof(v64val));
            printf("\t\t%ld %ld", v64val[1], v64val[0]);
        }
        puts("");
    }
    puts("===========================");
}
void compute_ggm_prf(block root, GGM_Hash_Tree_t& out, int level,const SequentialC2PC_SH& hash_provider){
    out.resize(level);
    out[0].resize(1);
    out[0][0]=root;

    for(int level_idx=1;level_idx<level;level_idx++){
        out[level_idx].resize(out[level_idx-1].size()*2);
        for(int prev_idx=0;prev_idx<out[level_idx-1].size();prev_idx++){
            hash_provider.Hash(out[level_idx][prev_idx*2],out[level_idx-1][prev_idx],0);
            hash_provider.Hash(out[level_idx][prev_idx*2+1],out[level_idx-1][prev_idx],1);
        }
    }
}

