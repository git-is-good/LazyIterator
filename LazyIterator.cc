#include "LazyIterator.hh"

#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <type_traits>
#include <numeric>
#include <exception>
#include <utility>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <limits>

void test4() {
    std::vector<int> vec(1000000);

    std::generate(vec.begin(), vec.end(), [] () { return std::rand() % 100; });

    auto iter = makeLazyIterator(vec.begin(), vec.end())
                .done()
                .sort()
                .groupBy<TWithCount<int>>(tWithCountJoiner<int>)
                ;

    auto iter2 = iter;

    std::cout << "---- Print foreach:\n";
    iter2
                .foreach([] (auto const &e) { std::cout << e << "\n"; })
                ;

    std::cout << "---- Print statistics:\n";

    auto sum = iter
                .dup()
                .map([] (auto const &e) { return e.count; })
                .reduce([] (int a, int b) { return a + b; }, 0)
                ;

    std::cout << "Sum: " << sum << ", Average: " << 1.0 * sum / vec.size() << "\n";

    auto minimal = iter
                    .dup()
                    .map([] (auto const &e) { return e.count; })
                    .reduce([] (int a, int b) { return a < b ? a : b; }, std::numeric_limits<int>::max())
                    ;
    std::cout << "Minimal: " << minimal << "\n";

    auto maximal = iter
                    .dup()
                    .map([] (auto const &e) { return e.count; })
                    .reduce([] (int a, int b) { return a > b ? a : b; }, std::numeric_limits<int>::min())
                    ;
    std::cout << "Maximal: " << maximal << "\n";

    std::cout << "\n";
}

void test3() {

    std::vector<std::string> vec{
        "this", "this", "that", "that", "that"
    };

    auto iter = makeLazyIterator(vec.begin(), vec.end())
//                .groupBy<TWithCount<std::string>>(tWithCountJoiner<std::string>)
                .groupSame()
                ;

    iter
                .foreach([] (auto const &e) { std::cout << e; })
                ;

    std::cout << "\n";


}

void test2() {
    constexpr std::size_t test_sz = 100LU;

    std::vector<int> vec(test_sz);

    std::generate(vec.begin(), vec.end(), [] () { return std::rand() % 10000; });

    auto iter = makeLazyIterator(vec.begin(), vec.end())
                .filter([] (auto e) { return e % 2 == 0; })
                .map([] (auto e) { return e * e % 10000; })
                .done()
                .sort(std::greater<int>())
                ;

    auto iter_dup1 = iter;
    auto iter_dup2 = iter;

    iter_dup1
        .foreach([] (auto e) { std::cout << e << "\n"; })
        ;

    auto res = iter
        .reduce(std::plus<int>(), 0)
        ;
    std::cout << "Result: " << res << "\n";

    iter_dup2
        .foreach([] (auto e) { std::cout << "Should be the same: " << e << "\n"; })
        ;

}

void test1() {
    std::vector<int> vec(10);
    std::generate(vec.begin(), vec.end(), [] () { return std::rand() % 100; });

    std::vector<std::string> res;

    auto iter = LazyIteratorRaw(vec.begin(), vec.end())
                .filter([] (auto e) { return e % 2 == 0; })
                .map([] (auto e) { return "hello: " + std::to_string(e); })
                .map([] (auto e) { return std::stoi(e.substr(7)); })
                .done()
                .sort()
                ;

//    iter.store(std::back_inserter(res));

//    for ( auto const &s : res ) {
//        std::cout << s << "\n";
//    }

    std::cout << iter.reduce(std::plus<int>(), 0) << "\n";

    while ( iter.ok() ) {
        std::cout << *iter++ << "\n";
    }

}

int main() {
//    test2();
//    test3();
    test4();
}
