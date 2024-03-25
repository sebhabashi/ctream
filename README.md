# Ctream: Container streams for C++
## What is Ctream ?

Ctream is a single-header library that provides fast parallelized pipelines on STL containers (vector, list etc.) and emulates the [Java Stream API](https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html) syntax.

Ctream's main advantage is its simplicity, its speed, and its clear syntax (especially for developpers that are used to Java). Because it is a single header, it can be integrated into your project with great simplicity, either installed in your system includes, or added directly to your project.

Ctream enables you to write code like this:
```cpp
// Use Ctream to get the maximum of the input integers
auto max = ctream::toCtream(argv, argc)
        .map<std::string>()
        .filter(validInteger)
        .map<long>([] (const std::string& str) { return std::stol(str); })
        .max();
```
In this example we:
+ Create a Ctream pipeline from `int argc` et `char** argv`
+ Cast each `char*` element to `std::string` using the `std::string` constructor
+ Filter only the elements for which `bool validInteger(const std::string&)` returns `true`
+ Parse each element as an integer using `std::stol`
+ Compute the maximum of the elements

All that effortlessly!

## How to get it ?
Tagged releases will be available soon in the Releases section. In the meantime, you can simply download the file `include/ctream.hpp` and add it into your project.

## How to use it ?
### Compatibility
Ctream is compatible with C++11 and higher.

### Caution
Ctream objects represent manipulation pipelines, not containers.
Using Ctream objects after their data source is destroyed will result in undefined behavior.

### Basic usage and examples
#### Examples
Get the sum of the even numbers
```cpp
auto sum = ctream::toCtream<int>(...)
        .filter([] (int i) { return i % 2 == 0; })
        .sum();
```
Get the age of the oldest person
```cpp
auto max = ctream::toCtream<Person>(...)
        .extract<int>([] (const Person& p) { return p.age; })
        .max();
```
Convert a list into a vector
```cpp
auto list = std::list<int>(...);
auto vect = ctream::toCtream(list).toVector();
```
Square all values in a vector
```cpp
auto values = std::vector<double>(...);
auto squares = ctream::toCtream(values)
        .map<double>([] (double v) { return v * v; });
        .toVector();
```

#### Creating a stream from a container
Ctream's can be constructed from different container types:
```cpp
std::vector<int> vec(...);
auto vectorStream = ctream::toCtream(vec);

std::list<int> list(...);
auto listStream = ctream::toCtream(list);

int raw[ARRAY_SIZE] = {...};
auto rawStream = ctream::toCtream(raw, ARRAY_SIZE);
```

#### Filtering
To keep only certain elements of the stream, use `filter`.
```cpp
// Filter only the even/odd numbers of a stream
auto numbers = ctream::toCtream<int>(...);
auto evenNumbers = numbers.filter([] (int i) {
    return i % 2 == 0;
});
auto oddNumbers = numbers.filter([] (int i) {
    return i % 2 == 1;
});

// Filter only the strings that are not empty
auto strings = ctream::toCtream<std::string>(...);
auto notEmptyStrings = strings.filter([] (const std::string& s) {
    return !s.empty();
});
```

#### Extracting data
To transform the data using only const references to data within the elements, use `extract`.
```cpp
struct Person
{
    std::string firstName;
    std::string lastName;
    int age;
    std::string address;
};
auto persons = ctream::toCtream<Person>(...);

// Extract the age of every person
auto ages = persons.extract<int>([] (const Person& p) {
    return p.age;
});

// Extract the first name of every person
auto firstNames = persons.extract<std::string>([] (const Person& p) {
    return p.firstName;
});
```

#### Transforming (mapping) the elements
When interesting data is not directly present in the elements and has to be constructed, `extract` cannot be used. In this case, we use `map`.
```cpp
struct Person
{
    std::string firstName;
    std::string lastName;
    int age;
    std::string address;
};
auto persons = ctream::toCtream<Person>(...);

// Get a stream containing the full name of every person
auto names = persons.map<std::string>([] (const Person& p) {
    return p.firstName + " " + p.lastName;
});
```

#### Converting back to usable data
The data of the stream can be exported back to STL containers using `toList` or `toVector`.
```cpp
auto stream = ctream::toCtream<int>(...);
std::list<int> list = stream.toList();
std::vector<int> vect = stream.toVector();
```
It can also be reduced with common operations:
```cpp
auto stream = ctream::toCtream<int>(...);
auto sum = stream.sum(); // Compute the sum
auto prod = stream.product(); // Compute the product
auto min = stream.min(); // Get the minimum
auto max = stream.max(); // Get the maximum

auto strings = ctream::toCtream<std::string>(...);
auto concat = strings.concat(); // Concatenates all strings
```
For other operations, it is necessary to use Collectors.

#### Using Collectors
Collectors are the way data is exported from a stream. In fact, all previous exporters are just shorthands for Collectors:
```cpp
auto stream = ctream::toCtream<int>(...);
std::list<int> list = stream.collect(collectors::ToList<int>{});
std::vector<int> vect = stream.collect(collectors::ToVector<int>{});
auto sum = stream.collect(collectors::Sum<int>{});
auto prod = stream.collect(collectors::Product<int>{});
auto min = stream.collect(collectors::Min<int>{});
auto max = stream.collect(collectors::Max<int>{});

auto strings = ctream::toCtream<std::string>(...);
auto concat = strings.collect(collectors::Concat<std::string>{});
```
To create specific collectors, the templated interface `Collector<T, A, R>` must be implemented with:
+ T being the input type
+ A being the accumulator type (see below, often the same as R)
+ R being the result type

Collectors are objects that contain information on how to export the
data from the streams into usable output data. Their aim is to have a
simple syntax, and to be parallelized easily.

The logic behind collectors is to `accumulate()` the elements of type T of
the stream in to partial results of type A. These partial results are then
combined into a complete result of type A with function `combine()`. This
complete result is finally converted into the final output result of type R
by function `finish()`.

Using a different type for A and R is not always necessary, but it can
sometimes be very useful. For instance, to concatenate strings, a solution
to avoid copying strings endlessly (the one used in this library) is to use
stringstream objects as accumulators, and then finish by extracting the
string from the stringstream.

For instance, the `Sum` collector, where A and R are identical, is defined as follows: 
```cpp
// Sum up all the elements of the stream
template<typename T>
class Sum : public Collector<T, T, T>
{
public:
    T supply() const override { return 0; }
    void accumulate(T& a, const T& b) const override { a += b; };
    void combine(T& a, T& b) const override { a += b; };
    T finish(T& a) const override { return a; }
};
```
Alternatively, the `Concat` collector uses different types for A (stringstream) and R (string):
```cpp
// Concatenate the elements of the stream into a string
template<typename T>
class Concat : public Collector<T, std::stringstream, std::string>
{
public:
    std::stringstream supply() const override
    {
        return std::stringstream{};
    }
    void accumulate(std::stringstream& a, const T& b) const override
    {
        a << b;
    }
    void combine(std::stringstream& a, std::stringstream& b) const override
    {
        a << b.str();
    }
    std::string finish(std::stringstream& a) const override
    {
        return a.str();
    }
};
```
Collectors can be "implemented" inline using the `Custom` collector:
```cpp
auto intSumCollector = collector::Custom<int, int, int>(
        [] () { return 0; }, // supplier
        [] (int& a, int b) { a+= b }, // accumulator
        [] (int& a, int& b) { a+= b }, // combiner
        [] (int a) { return a; }); // finisher

auto sum = ctream::toCtream<int>(...).collect(intSumCollector);
```

## Examples
Examples are available in directory `examples`, and more should come.

## API documentation
Complete documentation can be found in the `docs` directory, and can be regenerated using Doxygen.
Interesting entry points can be:
+ The [CStream class documentation](https://htmlpreview.github.io/?https://github.com/sebhabashi/ctream/blob/main/docs/html/group__ctream.html) (`ctream::Ctream<T>`)
+ The [Collector API](https://htmlpreview.github.io/?https://github.com/sebhabashi/ctream/blob/main/docs/html/group__collectors.html) (`ctream::collectors`)
