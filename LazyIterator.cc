#include "LazyIterator.hh"
#include "testings.hh"

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

struct StupidGen {
    int now = 0;

    int operator()() {
        return now++;
    }
};

template<class Int>
struct StupidConjecture {
    Int start;

    explicit StupidConjecture(Int s)
        : start(s)
    {}

    Int operator() () {
        Int res = start;
        if ( start % 2 == 0 ) {
            start /= 2;
        } else {
            start = 3 * start + 1;
        }
        return res;
    }
};

auto printer = [] (const auto &e) { std::cout << e << "\n"; };

void test7() {
    std::vector<int> a = {1, 5, 8, 23, 6, 17, 11, 23, 12, 2};
    std::vector<std::string> b = {
        "hello", "world", "goodbye", "moon", "milky-way",
        "congratulation", "signification", "nominal",
    };

    makeLazyIteratorFromZip(
            makeLazyIterator(a.begin(), a.end()),
            makeLazyIterator(b.begin(), b.end())
            )
        .map(
                [] (auto const &e) {
                    return std::make_pair(e.second, e.second.size());
                })
        .filter(
                [] (auto const &e) {
                    return e.second > 4;
                })
        .stopWhen(
                [] (auto const &e) {
                    return e.first[0] == 's';
                })
        .take(14)
        .done()
        .sort(
                [] (auto const &a, auto const &b) {
                    return a.second > b.second;
                })
        .reverse()
        .foreach(
                [] (auto const &e) {
                    std::cout << "[" << e.first << "," << e.second << "]\n";
                })
        ;

    auto iter77 = makeLazyIterator(b.begin(), b.end())
                    .done()
                    ;

    auto iter88 = iter77.reverse();
    try {
        *iter77;
    } catch (StopIteration const &) {
        std::cout << "should catch StopIteration\n";
    }

    while ( iter88.ok() ) {
        std::cout << *iter88 << "\n";
        ++iter88;
    }
}

void test6() {
    long start = 223036523;

    {
        TimeInterval _("Conjecture Lazy");

        makeLazyIteratorFromGenerator(StupidConjecture<long>(start))
            .stopWhen([] (auto e) { return e == 1; })
            .foreach(printer)
            ;
    }

    std::vector<int> vec{
        1,2,3,4,
    };

    makeLazyIterator(vec.begin(), vec.end())
        .stopWhen([] (auto e) { return e > 100; })
        .take(2)
        .foreach(printer)
        ;
}

void test5() {
    auto iter = makeLazyIteratorFromGenerator(StupidGen(), 7)
        ;

    iter
        .dup()
        .foreach( printer )
        ;

    std::cout << "Count: " << (
            iter
                .dup()
                .count()
            )
        << "\n";

    auto iter2 = makeLazyIteratorFromGenerator(StupidGen())
                .stopWhen([] (int e) { return e > 50; })
                .skipUntil([] (int e) { return e > 25; })
                .map([] (int e) { return e * e; })
                .filter([] (int e) { return e % 3 == 1; })
                ;
    iter2
                .foreach(printer);

    std::cout << "------- Start Conjecture:\n";
    makeLazyIteratorFromGenerator(StupidConjecture<long>(10343))
                    .stopWhen([] (auto e) { return e == 1; })
                    .foreach(printer)
                    ;
}

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
                .skipUntil([] (auto const &e) { return e.t > 50; })
                .foreach([] (auto const &e) { std::cout << e << "\n"; })
                ;

    std::cout << "---- Print statistics:\n";

    auto sum = iter
                .dup()
                .map([] (auto const &e) { return e.count; })
                .sum()
//                .reduce([] (int a, int b) { return a + b; }, 0)
                ;

    std::cout << "Sum: " << sum << ", Average: " << 1.0 * sum / vec.size() << "\n";

    auto minimal = iter
                    .dup()
                    .map([] (auto const &e) { return e.count; })
                    .numeric_min()
//                    .reduce([] (int a, int b) { return a < b ? a : b; }, std::numeric_limits<int>::max())
                    ;
    std::cout << "Minimal: " << minimal << "\n";

    auto maximal = iter
                    .dup()
                    .map([] (auto const &e) { return e.count; })
                    .numeric_max()
//                    .reduce([] (int a, int b) { return a > b ? a : b; }, std::numeric_limits<int>::min())
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
    test2();
    test3();
    test4();
    test5();
    test7();
}
