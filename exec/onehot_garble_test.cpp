#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
#include "tinygarble/onehot_circuit.hpp"
#include <algorithm>
#include <cstdio>
#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <vector>
#include "tinygarble/program_interface.h"
#include "tinygarble/program_interface_sh.h"
#include "tinygarble/TinyGarble_config.h"

#include <inttypes.h>

std::vector<bool> to_bool_vec(int val, int size){
	std::vector<bool> out(size);
	for(int idx=0;idx<size;idx++){
		out[idx]=val&1;
		val>>=1;
	}
	std::reverse(out.begin(), out.end());
	return out;
}
bool run(block* eval_blocks,block* garble_blocks,block* garbler_to_eval,SequentialC2PC_SH& hash_provider,One_Hot_Garble& dut, int test_a_val, int test_b_val){
	auto clear_text_a=to_bool_vec(test_a_val, dut.input_a.size());
	auto clear_text_b=to_bool_vec(test_b_val, dut.input_b.size());
	dut.garble(garble_blocks, garbler_to_eval, hash_provider);
	for(int bit_idx=0;bit_idx<dut.input_a.size();bit_idx++){
		if(clear_text_a[bit_idx]){
			eval_blocks[dut.input_a[bit_idx]]=xorBlocks(garble_blocks[dut.input_a[bit_idx]],hash_provider.Delta);
		}else {
			eval_blocks[dut.input_a[bit_idx]]=garble_blocks[dut.input_a[bit_idx]];
		}
	}
	for(int bit_idx=0;bit_idx<dut.input_b.size();bit_idx++){
		if(clear_text_b[bit_idx]){
			eval_blocks[dut.input_b[bit_idx]]=xorBlocks(garble_blocks[dut.input_b[bit_idx]],hash_provider.Delta);
		}else {
			eval_blocks[dut.input_b[bit_idx]]=garble_blocks[dut.input_b[bit_idx]];
		}
	}
	puts("Garbler to eval");
	for (int i=1; i<dut.level_text.size(); i++) {
		printf("\t level %d: \n", i);
		print128_num("\t\t", garbler_to_eval[dut.level_text[i]]);
		print128_num("\t\t", garbler_to_eval[dut.level_text[i]+1]);
	}
	dut.eval(eval_blocks, clear_text_a,garbler_to_eval, hash_provider);
	bool mismatch=false;
	for (int leaves_idx=0; leaves_idx<1<<dut.input_a.size(); leaves_idx++) {
		for(int output_idx=0; output_idx<dut.input_b.size();output_idx++){
		auto garbler_0=garble_blocks[dut.output[output_idx][leaves_idx]];
		auto garbler_1=xorBlocks(hash_provider.Delta, garbler_0);
		printf ("output idx %d leaves idx %d: \n"
			"\t Garbler:\n"
			,output_idx, leaves_idx);
		auto eval_out=eval_blocks[dut.output[output_idx][leaves_idx]];
		print128_num("\t\t0:", garbler_0);
		print128_num("\t\t1:", garbler_1);
		print128_num("\teval:",  eval_out);
		puts("");
		if(leaves_idx==test_a_val&&clear_text_b[output_idx]){
			if(!cmpBlock(&garbler_1,&eval_out, 1)){
				printf("MISMATCH: idx %d, expect 1\n",leaves_idx-3);
				mismatch=true;
			}
		}else {
			if(!cmpBlock(&garbler_0,&eval_out, 1)){
				printf("MISMATCH: idx %d, expect 0\n",leaves_idx-3);
				mismatch=true;
			}
		}
		}
	}
	return mismatch;
}
int main(int argc, char** argv) {

	SequentialC2PC_SH hash_provider(nullptr,ALICE);
	One_Hot_Garble dut;

	int wire_label_idx=0, garbler_to_eval_idx=0;	
	dut.allocate_idx(3, 2, wire_label_idx, garbler_to_eval_idx);
	block* eval_blocks=new block[wire_label_idx];
	block* garble_blocks=new block[wire_label_idx];
	block* garbler_to_eval=new block[garbler_to_eval_idx];

	for (int test_a=0; test_a<8; test_a++) {
		bool mismatch=false;
		for (int test_b=0; test_b<4; test_b++) {
			printf("\n\n----------------------------testing a: %d b: %d--------------------------\n",test_a,test_b);
			if(run(eval_blocks, garble_blocks, garbler_to_eval, hash_provider, dut, test_a, test_b)){
				mismatch=true;
				break;
			}
		}
		if(mismatch){
			break;
		}
	}
	return 0;
}
