#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <string>
#include <vector>

#include "ctream.hpp"

TEST_CASE("Collectors.Sum") {
    long n = 1e6;
    std::vector<long> ints;
    for (long i = 1; i <= n; ++i)
        ints.emplace_back(i);

    long expectedSum = 0;
    for (const auto v : ints)
        expectedSum += v;

    long sum;
    sum = ctream::toCtream(ints)
            .collect(ctream::collectors::Sum<long>{});
    CHECK( sum == expectedSum );

    sum = ctream::toCtream(ints).sum();
    CHECK( sum == expectedSum );
}

TEST_CASE("Collectors.Product") {
    // Vector like 3, 5, 1, 1, 1, (lots of 1s), ...1, 7
    long n = 1e6;
    std::vector<long> ints{};
    ints.emplace_back(3);
    ints.emplace_back(5);
    for (long i = 1; i <= n; ++i)
        ints.emplace_back(1);
    ints.emplace_back(7);

    long expectedProduct = 3 * 5 * 7;

    long product;
    product = ctream::toCtream(ints)
            .collect(ctream::collectors::Product<long>{});
    CHECK( product == expectedProduct );

    product = ctream::toCtream(ints).product();
    CHECK( product == expectedProduct );
}

TEST_CASE("Collectors.Min") {
    // Vector containing a lot of 1s, 2s, 3s... and a -6 in the middle
    long n = 1e6;
    std::vector<long> ints{};
    for (long i = 1; i <= n/2; ++i)
        ints.emplace_back(1 + (i % 3));
    ints.emplace_back(-6);
    for (long i = 1; i <= n/2; ++i)
        ints.emplace_back(1 + (i % 3));

    long expectedMin = -6;

    long min;
    min = ctream::toCtream(ints)
            .collect(ctream::collectors::Min<long>{});
    CHECK( min == expectedMin );
}

TEST_CASE("Collectors.Max") {
    // Vector containing a lot of 1s, 2s, 3s... and a 123456 in the middle
    long n = 1e6;
    std::vector<long> ints{};
    for (long i = 1; i <= n/2; ++i)
        ints.emplace_back(1 + (i % 3));
    ints.emplace_back(123456);
    for (long i = 1; i <= n/2; ++i)
        ints.emplace_back(1 + (i % 3));

    long expectedMax = 123456;

    long max;
    max = ctream::toCtream(ints)
            .collect(ctream::collectors::Max<long>{});
    CHECK( max == expectedMax );
}
