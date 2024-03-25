#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <string>
#include <vector>

#include "ctream.hpp"

TEST_CASE("Benchmarks") {

    struct Person
    {
        std::string firstName{"John"};
        std::string lastName{"Doe"};
        long age{56};
    };
    
    for (size_t sz = 1; sz < 1e5; sz *= 10)
    {
        std::vector<Person> vals{sz, Person{}};

        BENCHMARK("Streamed full name " + std::to_string(sz)) {
            ctream::toCtream<Person>(vals)
                    .map<std::string>([] (const auto& p) {
                        return p.firstName + " " + p.lastName;
                    })
                    .toVector();
        };
        BENCHMARK("For-loop full name " + std::to_string(sz)) {
            std::vector<std::string> names;
            names.reserve(sz);
            for (const auto& v : vals)
                names.emplace_back(v.firstName + " " + v.lastName);
        };
    }
}
