#include <chrono>
#include <fstream>
#include <iostream>

#include "cmdline.h"
#include "hmsearch.hpp"

std::vector<uint8_t> load_keys(const std::string& fn, uint32_t length, uint32_t alphabet_size) {
    std::ios::sync_with_stdio(false);

    std::ifstream ifs(fn);
    if (!ifs) {
        std::cerr << "open error: " << fn << std::endl;
        exit(1);
    }

    std::vector<uint8_t> buf(64);
    std::vector<uint8_t> keys;

    while (true) {
        uint32_t dim = 0;
        ifs.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        if (ifs.eof()) {
            break;
        }
        if (dim < length) {
            std::cerr << "error: dim < length" << std::endl;
            exit(1);
        }
        if (buf.size() < dim) {
            buf.resize(dim);
        }
        ifs.read(reinterpret_cast<char*>(buf.data()), dim);
        std::copy(buf.data(), buf.data() + length, std::back_inserter(keys));
    }
    keys.shrink_to_fit();

    for (size_t i = 0; i < keys.size(); ++i) {
        keys[i] = static_cast<uint8_t>(keys[i] % alphabet_size);
    }

    return keys;
}

int main(int argc, char* argv[]) {
    cmdline::parser p;
    p.add<std::string>("key_fn", 'k', "input file name of keys (bvecs format)", true);
    p.add<std::string>("query_fn", 'q', "input file name of queries (bvecs format)", true);
    p.add<uint32_t>("length", 'l', "length", false, 64);
    p.add<uint32_t>("alphabet_size", 'a', "alphabet size", false, 256);
    p.add<uint32_t>("hamming_range", 'r', "hamming range", false, 10);
    p.parse_check(argc, argv);

    auto key_fn = p.get<std::string>("key_fn");
    auto query_fn = p.get<std::string>("query_fn");
    auto length = p.get<uint32_t>("length");
    auto alphabet_size = p.get<uint32_t>("alphabet_size");
    auto hamming_range = p.get<uint32_t>("hamming_range");

    std::vector<uint8_t> keys_buf;
    std::vector<const uint8_t*> keys;

    std::vector<uint8_t> queries_buf;
    std::vector<const uint8_t*> queries;

    std::cout << "Loading keys..." << std::endl;
    {
        keys_buf = load_keys(key_fn, length, alphabet_size);
        keys.reserve(keys_buf.size() / length);
        for (size_t i = 0; i < keys_buf.size(); i += length) {
            keys.push_back(keys_buf.data() + i);
        }
        std::cout << "--> " << keys.size() << " keys" << std::endl;
    }

    std::cout << "Loading queries..." << std::endl;
    {
        queries_buf = load_keys(query_fn, length, alphabet_size);
        queries.reserve(queries_buf.size() / length);
        for (size_t i = 0; i < queries_buf.size(); i += length) {
            queries.push_back(queries_buf.data() + i);
        }
        std::cout << "--> " << queries.size() << " queries" << std::endl;
    }

    std::cout << "Constructing index..." << std::endl;
    hmsearch::hm_index index;
    index.build(keys, length, alphabet_size, hmsearch::get_proper_buckets(hamming_range));

    std::cout << "Searching queries..." << std::endl;

    std::vector<uint32_t> solutions;
    std::vector<uint32_t> true_solutions;

    solutions.reserve(1U << 10);
    true_solutions.reserve(1U << 10);

    for (uint32_t j = 0; j < queries.size(); ++j) {
        solutions.clear();
        true_solutions.clear();

        index.search(queries[j], hamming_range, [&](uint32_t id) { solutions.push_back(id); });

        for (uint32_t i = 0; i < keys.size(); ++i) {
            uint32_t hamming_dist = 0;
            for (uint32_t k = 0; k < length; ++k) {
                if (keys[i][k] != queries[j][k]) {
                    ++hamming_dist;
                    if (hamming_dist > hamming_range) {
                        break;
                    }
                }
            }
            if (hamming_dist <= hamming_range) {
                true_solutions.push_back(i);
            }
        }

        if (solutions.size() != true_solutions.size()) {
            std::cerr << "verification error: solutions.size() != true_solutions.size() -> "  //
                      << solutions.size() << " != " << true_solutions.size() << std::endl;
            std::cerr << "  at " << j << "-th query: " << queries[j] << std::endl;
            return 1;
        }

        std::sort(solutions.begin(), solutions.end());
        for (uint32_t i = 0; i < solutions.size(); ++i) {
            if (solutions[i] != true_solutions[i]) {
                std::cerr << "verification error: solutions[i] != true_solutions[i] for i = " << i << std::endl;
                std::cerr << "  at " << j << "-th query: " << queries[j] << std::endl;
                return 1;
            }
        }

        std::cout << j << ":\t" << solutions.size() << " solutions" << std::endl;
    }

    std::cout << "--> No problem!!" << std::endl;

    return 0;
}