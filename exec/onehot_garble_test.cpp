#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
#include "tinygarble/onehot_circuit.hpp"
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <vector>
#include "tinygarble/program_interface.h"
#include "tinygarble/program_interface_sh.h"
#include "tinygarble/TinyGarble_config.h"

#include <inttypes.h>

void print128_num(char* prefix, block var) 
{
    int64_t v64val[2];
    memcpy(v64val, &var, sizeof(v64val));
    printf("%s: %.16llx %.16llx\n",prefix, v64val[1], v64val[0]);
}

int main(int argc, char** argv) {

	SequentialC2PC_SH hash_provider(nullptr,ALICE);
	std::vector<size_t> out_label_idx(8);
	for (int i=0; i<8; i++) {
		out_label_idx[i]=i+3;
	}
	One_Hot_Garble dut{
		std::vector<size_t>{0,1,2},
		std::vector<size_t>{0,1,2},
		std::vector<size_t>{3},
		out_label_idx
	};	

	block* eval_blocks=new block[11];
	block* garble_blocks=new block[11];
	block* garbler_to_eval=new block[7];

	dut.garble(garble_blocks, garbler_to_eval, hash_provider);
	eval_blocks[0]=xorBlocks(garble_blocks[0],hash_provider.Delta);
	eval_blocks[1]=garble_blocks[1];
	eval_blocks[2]=xorBlocks(garble_blocks[2],hash_provider.Delta);
	dut.eval(eval_blocks, std::vector<bool>{true,false,true},garbler_to_eval, hash_provider);
	for (int i=0; i<11; i++) {
		printf ("idx %d: \n"
			"\t Garbler:\n"
			,i);
		print128_num("\t\t0:", garble_blocks[i]);
		print128_num("\t\t1:", xorBlocks(hash_provider.Delta, garble_blocks[i]));
		print128_num("\teval:",  eval_blocks[i]);
		puts("");
	}
	return 0;
}
