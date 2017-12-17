#include "ParserLibrary.hh"
#include "ParserCombinators.hh"
#include "ParserBasics.hh"
#include "Stream.hh"

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <stack>
#include <tuple>
#include <memory>
#include <functional>
#include <type_traits>
#include <cassert>

void test() {
    StreamAdapter s1("hello world");
    StreamAdapter s2("hello world");
    StreamAdapter s3("hello world");

    assert(
            (ParserString<>("hello") | ParserString<>("good")) >>= s1
          );

    assert(
            (ParserString<>("world") | ParserString<>("good")) >>= s1
          );

    assert(
            (ParserString<SkipPolicyNone>("hello")
             >> ParserChar<SkipPolicyNone>(' ')
             >> ParserString<SkipPolicyNone>("world")
            ) >>= s2
          );

    assert(!(
            (ParserString<SkipPolicyNone>("hello")
             >> ParserChar<SkipPolicyNone>(' ')
             >> ParserString<SkipPolicyNone>("world ")
            ) >>= s3
            )
          );

    StreamAdapter s4("hello hello hello hello hello hello");
    assert(
            (manyIndeed(ParserString<>("hello")) >> ParserEnd<>()) >>= s4
          );
}

void test2() {
//    StreamAdapter s1("{{{}}}");
    StreamAdapter s1("  { int=   abc {    double=   xyz{   int=abc{  }   }   }   }");
    DeferredParser dp;

//    dp = (ParserString("{") >> ParserString("}"))
//       | (ParserString("{") >> dp >> ParserString("}"))
//       ;

//    dp = ParserString("{") >> (
//                    ParserString("}") | (dp >> ParserString("}")) 
//                    )
//        ;

    
    dp = (ParserString<>("{") >> ParserString<>("}")) 
       | (ParserString<>("{") >> ParserString<>("int=") >> ParserString<>("abc") >> dp >> ParserString<>("}"))
       | (ParserString<>("{") >> ParserString<>("double=") >> ParserString<>("xyz") >> dp >> ParserString<>("}"))
       ;

    assert(dp >>= s1);
//    dp = ParserString("{") >> dp >> ParserString("}");
//    dp = operator>><ParserString, DeferredParser&>(ParserString("{"), dp);

}

struct AddNode
    : public YieldResult
{
    void show(std::ostream &os) {
        os << "[+:";
        left->show(os);
        os << ",";
        right->show(os); 
        os << "]";
    }
    YieldResultPtr left;
    YieldResultPtr right;
};

struct MultiNode
    : public YieldResult
{
    void show(std::ostream &os) {
        os << "[*:";
        left->show(os);
        os << ",";
        right->show(os);
        os << "]";
    }

    YieldResultPtr left;
    YieldResultPtr right;
};

struct NegateNode
    : public YieldResult
{
    void show(std::ostream &os) {
        os << "[negate:";
        left->show(os);
        os << "]";
    }

    YieldResultPtr left;
};

void test3() {
//    StreamAdapter s1("(123 + - ( 765 * 342 + 789)) * 34 + 42 * 76");
    StreamAdapter s1("(123 + - ( 765 * 342 + \"hello\" )) * 34 + 42 * 76");
    //StreamAdapter s1("123 + (123 * 456 + 765)");
//    StreamAdapter s1("456 + 123 + 765");

    DeferredParser expr;
//    expr
//        .set_debug_name("expr")
//        .debug()
//        ;

    auto unit = (ParserInt<>())
              | (ParserLiteral<>())
              | ((ParserString<>("(") >> expr >> ParserString<>(")"))
                 [(
                     [] ( auto &&tp ) -> YieldResultPtr {
                        return std::move(std::get<1>(tp));
                     }
                 )]
                )
              ;

    auto bigunit = ((ParserString<>("-") >> unit)
                    [(
                        [] (auto &&tp) -> YieldResultPtr {
                            NegateNode nd;
                            nd.left = std::move(std::get<1>(tp));
                            return std::move(
                                    std::make_unique<NegateNode>(std::move(nd))
                                    );
                        }
                    )]
                   )
                 | (unit)
                 ; 

    DeferredParser factor;

//    factor
//        .set_debug_name("factor")
//        .debug()
//        ;

    factor = ((bigunit >> ParserString<>("*") >> factor)
              [(
                  [] ( auto &&tp ) -> YieldResultPtr {
                    MultiNode nd;
                    nd.left = std::move(std::get<0>(tp));
                    nd.right = std::move(std::get<2>(tp));
                    return std::move(
                            std::make_unique<MultiNode>(std::move(nd))
                            );
                  }
              )]
             )
//           | (bigunit >> ParserString<>("/") >> factor)
           | (bigunit)
           ;

    expr = ((factor >> ParserString<>("+") >> expr)
            [( 
                [] ( auto &&tp ) -> YieldResultPtr {
                    AddNode nd;
                    nd.left = std::move(std::get<0>(tp));
                    nd.right = std::move(std::get<2>(tp));
                    return std::move(
                            std::make_unique<AddNode>(std::move(nd))
                            );
                }
             )]
           )
//         | (factor >> ParserString<>("-") >> expr)
         | (factor)
         ;

    assert( expr >>= s1 );
    auto res = expr.getResult();
    res.ptr_->show(std::cout);
    std::cout << "\n";

//    assert( (expr >> ParserEnd<>()) >>= s1 );
}

/* 
 * {
 *  "Coffee" : {
 *      "Java" : 12,
 *      "Indo" : "high",
 *      },
 *  
 *  "Orange" : {
 *      "Hot" : "bad",
 *      "Cold" : 18,
 *  },
 * }
 */

void test4() {
//    StreamAdapter s1("{ \"Coffee\" : 12, \"Chacolate\" : \"beau\", }");
    StreamAdapter s1("{\"Coffee\" : { \"Java\" : 12, \"Indo\" : \"high\", }, \"Orange\" : { \"Hot\" : \"bad\", \"Cold\" : 18, }, }");

    DeferredParser block;

    auto unit = ParserInt<>()
              | ParserLiteral<>()
              | block
              ;

    auto item = ParserLiteral<>() >> ParserString<>(":") >> unit >> ParserString<>(",");

    block = ParserString<>("{") >> many(item) >> ParserString<>("}");

    auto format = block >> ParserEnd<>();

    assert(format >>= s1);
}

int main() {
    test();
    test2();
//    for ( int i = 0; i < 10000000; i++ ) test3();
    test3();
    test4();
}
