#include <emp-tool/emp-tool.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <vector>
#include "emp-tool/utils/block.h"
#include "tinygarble/program_interface_sh.h"
#include "tinygarble/TinyGarble_config.h"

using namespace std;
namespace po = boost::program_options;

void millionaire(TinyGarblePI_SH* TGPI){	

	uint64_t bit_width = 8;
    int64_t a = 0, b = 0;
	
	cout << "Input wealth: ";
    if (TGPI->party == ALICE){
        cin >> a;
        cout << "ALICE's wealth: $" << a << endl;
    }
    else{
        cin >> b;
        cout << "BOB's wealth: $" << b << endl;
    }
    
    auto a_x = TGPI->TG_int_init(ALICE, bit_width+1, a);
    auto b_x = TGPI->TG_int_init(BOB, bit_width+1, b);

    TGPI->gen_input_labels();

    TGPI->retrieve_input_labels(a_x, ALICE, bit_width);
    TGPI->retrieve_input_labels(b_x, BOB, bit_width);

    block* res_x;
    TGPI->outer_product(res_x, a_x, b_x, bit_width,bit_width);
	int64_t res = TGPI->reveal(res_x, bit_width*bit_width, false);
	printf("%lx\n",res);
	int out_bw=2*bit_width;
	block* acc=new block[out_bw];
	block* temp=new block[out_bw];
	for (int idx=0; idx<bit_width; idx++) {
		TGPI->unsign_extend(temp, res_x+idx*bit_width, out_bw,bit_width);
		if(idx!=0){
			res = TGPI->reveal(temp, out_bw, false);
    		printf("temp_pre_sft:%ld\n",res);
			TGPI->left_shift(temp, idx, out_bw);
			res = TGPI->reveal(temp, out_bw, false);
    		printf("temp:%ld\n",res);
			TGPI->add(acc,acc,temp,bit_width+idx+1,bit_width+idx+1);
		}else {
			memcpy(acc, temp, sizeof(block)*(out_bw));
		}
		res = TGPI->reveal(acc, out_bw, false);
    	printf("acc:%ld\n",res);
	}


    res = TGPI->reveal(acc, out_bw, false);
    printf("%ld\n",res);
	TGPI->clear_TG_int(a_x);
	TGPI->clear_TG_int(b_x);
	TGPI->clear_TG_int(res_x);
	
	delete TGPI;
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
	io->flush();
	millionaire(TGPI);		
	
	delete io;
	return 0;
}
