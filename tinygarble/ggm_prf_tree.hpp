#include "sequential_2pc_sh.h"
typedef std::vector<std::vector<block>> GGM_Hash_Tree_t;
void compute_ggm_prf(block root, GGM_Hash_Tree_t& out, int level,const SequentialC2PC_SH& hash_provider){
    out.resize(level);
    out[0].resize(1);
    out[0][0]=root;

    for(int level_idx=1;level_idx<level;level_idx++){
        out[level_idx].resize(out[level_idx-1].size()*2);
        for(int prev_idx=0;prev_idx<out[level_idx-1].size();prev_idx++){
            hash_provider.Hash(out[level_idx][prev_idx*2],out[level_idx-1][prev_idx],0);
            hash_provider.Hash(out[level_idx][prev_idx*2],out[level_idx-1][prev_idx],1);
        }
    }
}

