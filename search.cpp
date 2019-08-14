#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cmdline.h"
#include "hmsearch.hpp"

class timer {
  public:
    using hrc = std::chrono::high_resolution_clock;

    timer() = default;

    template <class Duration>
    double get() const {
        return std::chrono::duration_cast<Duration>(hrc::now() - tp_).count();
    }

  private:
    hrc::time_point tp_ = hrc::now();
};

std::vector<uint8_t> load_keys(const std::string& fn, uint32_t length, uint32_t alphabet_size) {
    std::ios::sync_with_stdio(false);

    std::ifstream ifs(fn);
    if (!ifs) {
        std::cerr << "open error: " << fn << std::endl;
        exit(1);
    }

    std::vector<uint8_t> buf(64);
    std::vector<uint8_t> keys;
    keys.reserve(1U << 16);

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

std::vector<std::string> string_split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::string item;
    for (char ch : s) {
        if (ch == delim) {
            if (!item.empty()) elems.push_back(item);
            item.clear();
        } else {
            item += ch;
        }
    }
    if (!item.empty()) elems.push_back(item);
    return elems;
}

std::tuple<uint32_t, uint32_t, uint32_t> parse_range(const std::string& range_str) {
    auto elems = string_split(range_str, ':');
    if (elems.size() == 1) {
        uint32_t max = std::stoi(elems[0]);
        return {0, max, 1};
    }
    if (elems.size() == 2) {
        uint32_t min = std::stoi(elems[0]);
        uint32_t max = std::stoi(elems[1]);
        return {min, max, 1};
    }
    if (elems.size() == 3) {
        uint32_t min = std::stoi(elems[0]);
        uint32_t max = std::stoi(elems[1]);
        uint32_t stp = std::stoi(elems[2]);
        return {min, max, stp};
    }

    std::cerr << "error: invalid format of range string " << range_str << std::endl;
    exit(1);
}

template <class T>
uint32_t compute_hamming_distance(const T* x, const T* y, uint32_t length, uint32_t range = UINT32_MAX) {
    uint32_t dist = 0;
    for (uint32_t k = 0; k < length; ++k) {
        if (x[k] != y[k]) {
            ++dist;
            if (dist > range) {
                break;
            }
        }
    }
    return dist;
}

void compute_diff(const std::vector<uint32_t>& x, const std::vector<uint32_t>& y, const char* msg) {
    std::vector<uint32_t> results;
    std::set_difference(x.begin(), x.end(), y.begin(), y.end(), std::back_inserter(results));

    std::cout << msg << ": ";
    for (auto r : results) {
        std::cout << r << " ";
    }
    std::cout << std::endl;
}

template <class It>
inline void print_ints(std::ostream& os, It beg, It end, const char* title = nullptr) {
    if (title) {
        os << title << ": ";
    }
    for (auto it = beg; it != end; ++it) {
        std::cout << static_cast<int32_t>(*it) << " ";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    cmdline::parser p;
    p.add<std::string>("key_fn", 'k', "input file name of keys (bvecs format)", true);
    p.add<std::string>("query_fn", 'q', "input file name of queries (bvecs format)", true);
    p.add<uint32_t>("length", 'l', "length", false, 64);
    p.add<uint32_t>("alphabet_size", 'a', "alphabet size", false, 256);
    p.add<std::string>("hamming_ranges", 'r', "hamming ranges (min:max:step)", false, "0:10:2");
    p.add<bool>("enable_test", 't', "enable test", false, false);
    p.parse_check(argc, argv);

    auto key_fn = p.get<std::string>("key_fn");
    auto query_fn = p.get<std::string>("query_fn");
    auto length = p.get<uint32_t>("length");
    auto alphabet_size = p.get<uint32_t>("alphabet_size");
    auto hamming_ranges = p.get<std::string>("hamming_ranges");
    auto enable_test = p.get<bool>("enable_test");

    std::vector<uint8_t> keys_buf;
    std::vector<const uint8_t*> keys;

    std::vector<uint8_t> queries_buf;
    std::vector<const uint8_t*> queries;

    std::cout << "Loading keys from " << key_fn << std::endl;
    {
        keys_buf = load_keys(key_fn, length, alphabet_size);
        keys.reserve(keys_buf.size() / length);
        for (size_t i = 0; i < keys_buf.size(); i += length) {
            keys.push_back(keys_buf.data() + i);
        }
        std::cout << "--> " << keys.size() << " keys" << std::endl;
    }

    std::cout << "Loading queries from " << query_fn << std::endl;
    {
        queries_buf = load_keys(query_fn, length, alphabet_size);
        queries.reserve(queries_buf.size() / length);
        for (size_t i = 0; i < queries_buf.size(); i += length) {
            queries.push_back(queries_buf.data() + i);
        }
        std::cout << "--> " << queries.size() << " queries" << std::endl;
    }

    uint32_t min_range, max_range, range_step;
    std::tie(min_range, max_range, range_step) = parse_range(hamming_ranges);

    std::unique_ptr<hmsearch::hm_index> index;

    for (uint32_t hamming_range = min_range; hamming_range <= max_range; hamming_range += range_step) {
        const uint32_t proper_buckets = hmsearch::hm_index::get_proper_buckets(hamming_range);

        std::cout << std::endl;
        std::cout << "[analyzing] " << hamming_range << " range; " << proper_buckets << " buckets" << std::endl;

        if (!index || index->get_buckets() != proper_buckets) {
            std::cout << "Constructing index..." << std::endl;
            {
                timer t;
                index = std::make_unique<hmsearch::hm_index>();
                index->build(keys, length, alphabet_size, hmsearch::hm_index::get_proper_buckets(hamming_range));
                std::cout << "--> construction time: " << t.get<std::chrono::seconds>() << " sec" << std::endl;

                uint64_t memory_usage = sdsl::size_in_bytes(*index.get());
                std::cout << "--> memory usage: " << memory_usage << " bytes; "  //
                          << memory_usage / (1024.0 * 1024.0) << " MiB" << std::endl;
            }
        }

        if (enable_test) {
            std::vector<uint32_t> solutions;
            std::vector<uint32_t> true_solutions;

            solutions.reserve(1U << 10);
            true_solutions.reserve(1U << 10);

            for (uint32_t j = 0; j < queries.size(); ++j) {
                solutions.clear();
                true_solutions.clear();

                index->search(queries[j], hamming_range, [&](uint32_t id) { solutions.push_back(id); });

                for (uint32_t i = 0; i < keys.size(); ++i) {
                    uint32_t hamming_dist = compute_hamming_distance(keys[i], queries[j], length, hamming_range);
                    if (hamming_dist <= hamming_range) {
                        true_solutions.push_back(i);
                    }
                }

                std::sort(solutions.begin(), solutions.end());

                if (solutions.size() != true_solutions.size()) {
                    std::cerr << "verification error: solutions.size() != true_solutions.size() -> "  //
                              << solutions.size() << " != " << true_solutions.size() << std::endl;
                    std::cerr << "  at " << j << "-th query: " << queries[j] << std::endl;
                    compute_diff(solutions, true_solutions, "solutions - true_solutions");
                    compute_diff(true_solutions, solutions, "true_solutions - solutions");
                    print_ints(std::cerr, queries[j], queries[j] + length);
                    print_ints(std::cerr, keys[9477], keys[9477] + length);
                    return 1;
                }

                for (uint32_t i = 0; i < solutions.size(); ++i) {
                    if (solutions[i] != true_solutions[i]) {
                        std::cerr << "verification error: solutions[i] != true_solutions[i] for i = " << i << std::endl;
                        std::cerr << "  at " << j << "-th query: " << queries[j] << std::endl;
                        return 1;
                    }
                }

                // std::cout << j << ":\t" << solutions.size() << " solutions" << std::endl;
            }

            std::cout << "--> No problem!!" << std::endl;
        }

        std::cout << "Searching queries..." << std::endl;
        {
            std::vector<uint32_t> solutions;
            solutions.reserve(1U << 10);

            uint64_t sum_candidates = 0;

            timer t;
            for (uint32_t j = 0; j < queries.size(); ++j) {
                sum_candidates += index->search(queries[j], hamming_range,  //
                                                [&](uint32_t id) { solutions.push_back(id); });
            }
            double elapsed_ms = t.get<std::chrono::milliseconds>() / queries.size();
            double num_solutions = double(solutions.size()) / queries.size();
            double num_candidates = double(sum_candidates) / queries.size();

            std::cout << "--> " << elapsed_ms << " ms_per_query" << std::endl;
            std::cout << "--> " << num_solutions << " solutions_per_query" << std::endl;
            std::cout << "--> " << num_candidates << " candidates_per_query" << std::endl;
        }
    }

    return 0;
}