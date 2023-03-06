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

bool run(block* eval_blocks,block* garble_blocks,block* garbler_to_eval,SequentialC2PC_SH& hash_provider,One_Hot_Garble& dut, int test_val){
	std::vector<bool> clear_text_a(4);
	int temp_test_val=test_val;
	for(int bit_idx=0;bit_idx<4;bit_idx++){
		clear_text_a[3-bit_idx]=temp_test_val&1;
		temp_test_val>>=1;
	}
	dut.garble(garble_blocks, garbler_to_eval, hash_provider);
	for(int bit_idx=0;bit_idx<4;bit_idx++){
		if(clear_text_a[bit_idx]){
			eval_blocks[bit_idx]=xorBlocks(garble_blocks[bit_idx],hash_provider.Delta);
		}else {
			eval_blocks[bit_idx]=garble_blocks[bit_idx];
		}
	}
	puts("Garbler to eval");
	for (int i=0; i<7; i++) {
		print128_num("\t", garbler_to_eval[i]);
	}
	dut.eval(eval_blocks, clear_text_a,garbler_to_eval, hash_provider);
	bool mismatch=false;
	for (int i=0; i<20; i++) {
		auto garbler_0=garble_blocks[i];
		auto garbler_1=xorBlocks(hash_provider.Delta, garble_blocks[i]);
		printf ("idx %d: \n"
			"\t Garbler:\n"
			,i);
		print128_num("\t\t0:", garbler_0);
		print128_num("\t\t1:", garbler_1);
		print128_num("\teval:",  eval_blocks[i]);
		puts("");
		if(i>=4){
			if((i-4)==test_val){
				if(!cmpBlock(&garbler_1,& eval_blocks[i], 1)){
					printf("MISMATCH: idx %d, expect 1\n",i-3);
					mismatch=true;
				}
			}else {
				if(!cmpBlock(&garbler_0,& eval_blocks[i], 1)){
					printf("MISMATCH: idx %d, expect 0\n",i-3);
					mismatch=true;
				}
			}
		}
	}
	return mismatch;
}
int main(int argc, char** argv) {

	SequentialC2PC_SH hash_provider(nullptr,ALICE);
	std::vector<size_t> out_label_idx(16);
	for (int i=0; i<16; i++) {
		out_label_idx[i]=i+4;
	}
	One_Hot_Garble dut{
		std::vector<size_t>{0,1,2,3},
		std::vector<size_t>{0,0,2,4},
		std::vector<size_t>{6},
		out_label_idx
	};	

	block* eval_blocks=new block[20];
	block* garble_blocks=new block[20];
	block* garbler_to_eval=new block[7];

	for (int test_a=0; test_a<16; test_a++) {
		printf("\n\n----------------------------testing %d--------------------------\n",test_a);
		if(run(eval_blocks, garble_blocks, garbler_to_eval, hash_provider, dut, test_a)){
			break;
		}
	}
	return 0;
}
