#include <string>
#include <vector>

#include "ctream.hpp"

struct Person
{
    std::string firstName{"John"};
    std::string lastName{"Doe"};
    long age{56};
};

int main()
{    
    const size_t sz = 1e4;
    std::vector<Person> vals{sz, Person{}};

    ctream::toCtream(vals)
            .map<std::string>([] (const auto& p) {
                return p.firstName + " " + p.lastName;
            })
            .toVector();
}
