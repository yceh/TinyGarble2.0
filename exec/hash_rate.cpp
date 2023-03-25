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

static long getcputime(){
	struct rusage usage;
	auto ret=getrusage(RUSAGE_SELF, &usage);
	//fprintf(stderr, "%d\n",ret);
	if(ret!=0){
		raise(SIGTRAP);
	}
	return usage.ru_utime.tv_sec*1E6+usage.ru_utime.tv_usec;
}
int main(int argc, char** argv) {

	SequentialC2PC_SH hash_provider(nullptr,ALICE);
	block init=hash_provider.Delta;
	block next;
	auto start_time=getcputime();
	for (long i=0; i<100000000; i++) {
		hash_provider.Hash(next,init,1);
		hash_provider.Hash(init,next,1);
	}
	printf("%d\n", init);
	printf("time %zu\n",getcputime()-start_time);
	return 0;
}
