#ifndef SEQUENTIAL_2PC_WRAP_SH_H__
#define SEQUENTIAL_2PC_WRAP_SH_H__

#include <cstring>
#include <emp-tool/emp-tool.h>
#include "emp-tool/utils/block.h"
#include "emp-tool/utils/constants.h"
#include "tinygarble/sequential_2pc_sh.h"
#include "tinygarble/TinyGarble_config.h"
#include "onehot_circuit.hpp"

using namespace std;

void sequential_2pc_exec_sh(InputOutput* InOut, SequentialC2PC_SH* twopc, block* labels_B, block* labels_A, block* labels_S, block*& labels_R, int party, NetIO* io, CircuitFile* cf, int cycles = 1, int repeat = 1, int output_mode = 0, bool report = false, uint64_t dc[4] = (uint64_t*)default_array, double dt[4] = default_array) {
	Timer T;
		
	int cyc_rep = cycles*repeat;
	
	T.start();
	twopc->init(party, cf, cycles, cyc_rep, output_mode);
	io->flush();	
	T.get(dc[1], dt[1]);
	if (report) cout << "init:\t" << dc[1] << "\tcc\t" << dt[1] << "\tms" << endl;

	int output_bit_width = cf->n3;		
	InOut->init("", 0, output_bit_width, "", 0, cycles);	
	bool *out = new bool[output_bit_width];
	memset(out, false, output_bit_width);
	
	T.start();
	for(int cid = 0; cid < cyc_rep; ++cid) {
		twopc->copy_input_labels(labels_B, labels_A, labels_S);
		twopc->garble();
		io->flush();
		twopc->evaluate(out);
		if((output_mode == 0) || ((output_mode == 1)&&(((cid+1)%cycles) == 0))) InOut->fill_output(out);
	}
	T.get(dc[3], dt[3]);
	if (report) cout << "g/e:\t" << dc[3] << "\tcc\t" << dt[3] << "\tms" << endl;
	
	twopc->retrieve_shares(labels_R);
	
	twopc->clear();
	
	delete[] out;
	
	return;
}

void sequential_2pc_exec_sh(SequentialC2PC_SH* twopc, block* labels_B, block* labels_A, block* labels_S, block*& labels_R, int party, NetIO* io, General_One_Hot_Outer_Prod& cf) {	
	//twopc->init(party, cf, cycles, cyc_rep, output_mode);
	io->flush();
	
	block* labels=new block[cf.total_label_cnt];
	block* to_transmit=new block[cf.total_comm_cnt];

	for (size_t a_idx=0; a_idx<cf.input_a.size(); a_idx++) {
		labels[cf.input_a[a_idx]]=labels_A[a_idx];
	}

	for (size_t b_idx=0; b_idx<cf.input_b.size(); b_idx++) {
		labels[cf.input_b[b_idx]]=labels_B[b_idx];
	}

	if(party==ALICE){
		cf.garble(labels, to_transmit, *twopc);
		io->send_block(to_transmit, cf.total_comm_cnt);
	}else {
		io->recv_block(to_transmit, cf.total_comm_cnt);
	}

	io->flush();

	if(party==BOB){
		cf.eval(labels, to_transmit, *twopc);
	}

	/*for(int cid = 0; cid < cyc_rep; ++cid) {
		//twopc->copy_input_labels(labels_B, labels_A, labels_S);
		//twopc->garble();
		//io->flush();
		//twopc->evaluate(out);
	}*/
	
	memcpy(labels_R, labels+cf.output[0][0], sizeof(block)*cf.output.size()*cf.output[0].size());
	//twopc->retrieve_shares(labels_R);
	
	delete [] labels;
	delete [] to_transmit;
	//twopc->clear();
	
	//delete[] out;	
	return;
}


void sequential_2pc_exec_sh(SequentialC2PC_SH* twopc, block* labels_B, block* labels_A, block* labels_S, block*& labels_R, int party, NetIO* io, CircuitFile* cf, int cycles = 1, int repeat = 1, int output_mode = 2) {	
	if ((output_mode == 0) || (output_mode == 1)){
		cout << "for output_mode = 0 or 1, an InputOutput object is needed" << endl;
		exit(-1);
	}
	
	int cyc_rep = cycles*repeat;
	
	twopc->init(party, cf, cycles, cyc_rep, output_mode);
	io->flush();
	
	bool *out = nullptr;	
	for(int cid = 0; cid < cyc_rep; ++cid) {
		twopc->copy_input_labels(labels_B, labels_A, labels_S);
		twopc->garble();
		io->flush();
		twopc->evaluate(out);
	}
	
	twopc->retrieve_shares(labels_R);
	
	twopc->clear();
	
	delete[] out;	
	return;
}

#endif //SEQUENTIAL_2PC_WRAP_SH_H__
