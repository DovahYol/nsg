//
// Created by 付聪 on 2017/6/21.
//

#include <efanna2e/index_nsg.h>
#include <efanna2e/util.h>
#include <chrono>
#include <string>

void load_data(char* filename, float*& data, unsigned& num,
	unsigned& dim) {  // load data with sift10K pattern
	std::ifstream in(filename, std::ios::binary);
	if (!in.is_open()) {
		std::cout << "open file error" << std::endl;
		exit(-1);
	}
	in.read((char*)&dim, 4);
	// std::cout<<"data dimension: "<<dim<<std::endl;
	in.seekg(0, std::ios::end);
	std::ios::pos_type ss = in.tellg();
	size_t fsize = (size_t)ss;
	num = (unsigned)(fsize / (dim + 1) / 4);
	data = new float[(size_t)num * (size_t)dim];

	in.seekg(0, std::ios::beg);
	for (size_t i = 0; i < num; i++) {
		in.seekg(4, std::ios::cur);
		in.read((char*)(data + i * dim), dim * 4);
	}
	in.close();
}

void save_result(const char* filename,
	std::vector<std::vector<unsigned> >& results) {
	std::ofstream out(filename, std::ios::binary | std::ios::out);

	for (unsigned i = 0; i < results.size(); i++) {
		unsigned GK = (unsigned)results[i].size();
		out.write((char*)&GK, sizeof(unsigned));
		out.write((char*)results[i].data(), GK * sizeof(unsigned));
	}
	out.close();
}
int main(int argc, char** argv) {
	if (argc != 7 && argc != 8) {
		std::cout << argv[0]
			<< " data_file query_file nsg_path search_L search_K result_path"
			<< std::endl;

		std::cout << argv[0]
			<< " data_file query_file nsg_path search_L search_K result_path truth_path"
			<< std::endl;
		exit(-1);
	}
	float* data_load = NULL;
	unsigned points_num, dim;
	load_data(argv[1], data_load, points_num, dim);
	float* query_load = NULL;
	unsigned query_num, query_dim;
	load_data(argv[2], query_load, query_num, query_dim);
	assert(dim == query_dim);

	unsigned L = (unsigned)atoi(argv[4]);
	unsigned K = (unsigned)atoi(argv[5]);

	if (L < K) {
		std::cout << "search_L cannot be smaller than search_K!" << std::endl;
		exit(-1);
	}

	// data_load = efanna2e::data_align(data_load, points_num, dim);//one must
	// align the data before build query_load = efanna2e::data_align(query_load,
	// query_num, query_dim);
	efanna2e::IndexNSG index(dim, points_num, efanna2e::FAST_L2, nullptr);
	index.Load(argv[3]);
	index.OptimizeGraph(data_load);

	efanna2e::Parameters paras;
	paras.Set<unsigned>("L_search", L);
	paras.Set<unsigned>("P_search", L);

	std::vector<std::vector<unsigned> > res(query_num);
	for (unsigned i = 0; i < query_num; i++) res[i].resize(K);

	auto s = std::chrono::high_resolution_clock::now();
	for (unsigned i = 0; i < query_num; i++) {
		index.SearchWithOptGraph(query_load + i * dim, K, paras, res[i].data());
	}
	auto e = std::chrono::high_resolution_clock::now();
	double diff = std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() * 1.0;
	std::cout << "search time(ms): " << diff << std::endl;
	std::cout << "avarage latency(ms): " << diff / query_num << std::endl;
	std::cout << "qps: " << query_num * 1000 / diff << std::endl;

	save_result(argv[6], res);

	if (argc == 8) {
		std::vector<std::vector<int>> truths;
		std::ifstream truthFile(argv[7], std::ios::binary);
		while (true) {
			int dim;
			if (truthFile) {
				truthFile.read(reinterpret_cast<char*>(&dim), 4);
			}
			else {
				break;
			}
			std::vector<int> myvec(dim);
			truthFile.read(reinterpret_cast<char*>(myvec.data()), 4 * dim);
			truths.push_back(std::move(myvec));
		}
		truthFile.close();

		float recall = 0;
		for (int i = 0; i < res.size(); i++)
		{
			for (int id : res[i])
			{
				for (int j = 0; j < K; j++)
				{
					if (truths[i][j] == id)
					{
						recall++;
						break;
					}
				}
			}
		}

		recall = recall / static_cast<float>(res.size() * K);
		
		std::cout << "recall@" << K << ": " << recall << std::endl;
	}

	return 0;
}
