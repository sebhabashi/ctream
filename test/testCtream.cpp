#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <string>
#include <vector>

#include "ctream.hpp"

TEST_CASE("Base.toList.Small") {
    struct Person {
        std::string name;
        int age;
        double weight;
        int height;
    };
    std::vector<Person> persons {
        {.name = "John Doe",   .age = 51, .weight = 68.5, .height = 175 },
        {.name = "Peter Pan",  .age = 10, .weight = 50.2, .height = 155 },
        {.name = "Cinderella", .age = 18, .weight = 51.3, .height = 162 },
    };

    auto ages = ctream::toCtream(persons)
            .extract<int>([] (const Person& p) -> const int& { return p.age; })
            .toList();

    CHECK( ages == std::list<int>{51, 10, 18} );
}

TEST_CASE("Base.toList.Large") {
    const long n = 10000;

    // Vector 1, 2, 3, ...n
    std::vector<long> ints;
    for (long i = 1; i <= n; ++i)
        ints.emplace_back(i);

    // Compute the sum of squares (1 + 4 + 9 + ...n^2)
    auto sumOfSquares = ctream::toCtream(ints)
        .map<long>([] (int i) { return i * i; })
        .sum();

    // Math says it should be equal to this
    auto expected = (n * long(n + 1) * long(2 * n + 1)) / 6;

    CHECK( sumOfSquares == expected );
}
