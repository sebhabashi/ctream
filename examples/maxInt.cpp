#include <ctream.hpp>

#include <iostream>
#include <vector>

bool validInteger(const std::string& line)
{
    char* p;
    strtol(line.c_str(), &p, 10);
    return *p == 0;
}

int main(int argc, char** argv)
{
    std::cout << "The biggest integer you typed is ";

    auto max = ctream::toCtream(argv, argc)
            .map<std::string>()
            .filter(validInteger)
            .map<long>([] (const std::string& str) { return std::stol(str); })
            .max();

    std::cout << max << "!" << std::endl;
}
