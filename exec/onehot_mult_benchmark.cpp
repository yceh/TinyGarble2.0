#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <vector>
#include "emp-tool/utils/block.h"
#include "exec/unit_test.h"
#include "tinygarble/program_interface_sh.h"
#include "tinygarble/TinyGarble_config.h"
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;
namespace po = boost::program_options;

struct stats{
	size_t nhashs;
	size_t ncomm;
	std::vector<size_t> time;	
	std::vector<size_t> utime;	
};
static long getcputime(){
	struct rusage usage;
	auto ret=getrusage(RUSAGE_SELF, &usage);
	//fprintf(stderr, "%d\n",ret);
	if(ret!=0){
		raise(SIGTRAP);
	}
	return usage.ru_utime.tv_sec*1E6+usage.ru_utime.tv_usec;
}
void millionaire(int bit_width,TinyGarblePI_SH* TGPI, stats& baseline,stats& onehot){

    int64_t a = 0, b = 0;
	
    if (TGPI->party == ALICE){
		a=rand()&((1<<bit_width)-1);
    }
    else{
		b=rand()&((1<<bit_width)-1);
    }
    
    auto a_x = TGPI->TG_int_init(ALICE, bit_width, a);
    auto b_x = TGPI->TG_int_init(BOB, bit_width, b);
	
	auto res_ref=TGPI->TG_int(2*bit_width);
	
    TGPI->gen_input_labels();

    TGPI->retrieve_input_labels(a_x, ALICE, bit_width);
    TGPI->retrieve_input_labels(b_x, BOB, bit_width);
	
	long cputime_start=getcputime();
	auto baseline_start=std::chrono::high_resolution_clock::now();
	TGPI->mult(res_ref,a_x,b_x,bit_width,bit_width);
	auto baseline_end=std::chrono::high_resolution_clock::now();
	baseline.utime.emplace_back(getcputime()-cputime_start);
	auto n_microsecond=std::chrono::duration_cast<std::chrono::microseconds>(baseline_end-baseline_start).count();
	baseline.ncomm=TGPI->io->send_count;
	baseline.nhashs= TGPI->twopc->prp.nhashes;
	baseline.time.emplace_back(n_microsecond);
	TGPI->twopc->prp.nhashes=0;
	TGPI->io->send_count=0;
	TGPI->io->recv_count=0;
	auto res_ref_rev=TGPI->reveal(res_ref,2*bit_width,false);
    //printf("ref:%d\n",res_ref_rev);
    
	cputime_start=getcputime();
	auto onehot_start=std::chrono::high_resolution_clock::now();
	block* res_x;
    TGPI->outer_product(res_x, a_x, b_x, bit_width,bit_width);
	int out_bw=2*bit_width;
	block* acc=new block[out_bw];
	block* temp=new block[out_bw];
	for (int idx=0; idx<bit_width; idx++) {
		TGPI->unsign_extend(temp, res_x+idx*bit_width, out_bw,bit_width);
		if(idx!=0){
			TGPI->left_shift(temp, idx, out_bw);
			TGPI->add(acc,acc,temp,bit_width+idx+1,bit_width+idx+1);
		}else {
			memcpy(acc, temp, sizeof(block)*(out_bw));
		}
    	//printf("acc:%ld\n",res);
	}

	auto onehot_end=std::chrono::high_resolution_clock::now();

    //printf("%ld\n",res);
	auto onehot_microsecond=std::chrono::duration_cast<std::chrono::microseconds>(onehot_end-onehot_start).count();
	onehot.utime.push_back(getcputime()-cputime_start);
	onehot.ncomm=TGPI->io->send_count;
	onehot.nhashs= TGPI->twopc->prp.nhashes;
	onehot.time.emplace_back(onehot_microsecond);
	TGPI->io->send_count=0;
	TGPI->io->recv_count=0;
	TGPI->twopc->prp.nhashes=0;
    auto res = TGPI->reveal(acc, out_bw, false);
	
	TGPI->clear_TG_int(a_x);
	TGPI->clear_TG_int(b_x);
	TGPI->clear_TG_int(res_x);
	
}

int main(int argc, char** argv) {
	int party = 1, port = 1234;
	string netlist_address;
	string server_ip;
	
	po::options_description desc{"Yao's Millionair's Problem \nAllowed options"};
	desc.add_options()  //
	("help,h", "produce help message")  //
	("party,k", po::value<int>(&party)->default_value(1), "party id: 1 for garbler, 2 for evaluator")  //
	("port,p", po::value<int>(&port)->default_value(8000), "socket port")  //
	("server_ip,s", po::value<string>(&server_ip)->default_value("127.0.0.1"), "server's IP.");
	
	po::variables_map vm;
	try {
		po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
		po::store(parsed, vm);
		if (vm.count("help")) {
			cout << desc << endl;
			return 0;
		}
		po::notify(vm);
	}catch (po::error& e) {
		cout << "ERROR: " << e.what() << endl << endl;
		cout << desc << endl;
		return -1;
	}
		
	NetIO* io = new NetIO(party==ALICE ? nullptr:server_ip.c_str(), port, true);
	io->set_nodelay();
	
	TinyGarblePI_SH* TGPI; 
	
	cout << "testing program interface in semi-honest setting" << endl;
	TGPI = new TinyGarblePI_SH(io, party);
	//unit_test(TGPI);
	io->flush();
	fprintf(stderr, "bit_width\tbaseline_time\tbaseline_comm_bytes\tonehot_time\tonehot_comm_bytes\tonehot_time\t\n");
	for (int bitsize:{2,4,6,7,8,9,10,12,16}){
		stats baseline,onehot;
		for (int iter=0; iter<21; iter++) {
			millionaire(bitsize, TGPI, baseline, onehot);
		}
		std::sort(baseline.time.begin(),baseline.time.end());
		std::sort(baseline.utime.begin(),baseline.utime.end());
		std::sort(onehot.time.begin(),onehot.time.end());
		std::sort(onehot.utime.begin(),onehot.utime.end());
		double base_line_sum=0;
		double one_hot_sum=0;
		double one_hot_usum=0;
		double base_line_usum=0;
		for (int idx=5; idx<15; idx++) {
			base_line_sum+=baseline.time[idx];
			base_line_usum+=baseline.utime[idx];
			one_hot_sum+=onehot.time[idx];
			one_hot_usum+=onehot.utime[idx];
		}

		fprintf(stderr, "%d\t%f\t%f\t%f\t%f\t%zu\t%zu\t%zu\t%zu\n", 
			bitsize,base_line_sum/10,one_hot_sum/10,base_line_usum/10,one_hot_usum/10,baseline.ncomm,onehot.ncomm,baseline.nhashs
			,onehot.nhashs);
	}
	
	delete io;
	return 0;
}
