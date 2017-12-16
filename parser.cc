/*
 * Stream Concept:
 * Stream {
 *      const char *next(std::size_t howmany);
 *      void put(std::size_t howmany);
 * };
 */

#include "util.hh"

#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <stack>
#include <memory>
#include <functional>
#include <type_traits>
#include <cassert>

class StreamAdapter
{
public:
    explicit StreamAdapter(std::string const &s)
        : s_(s)
    {}

    const char *next(std::size_t howmany) {
        if ( cur_pos + howmany <= s_.size() ) {
            auto prev_pos = cur_pos;
            cur_pos += howmany;
            return &s_[prev_pos];
        } else {
            return nullptr;
        }
    }

    void put(std::size_t howmany) {
        cur_pos -= howmany;
        assert(cur_pos >= 0);
    }
private:
    std::string     s_;
    std::size_t     cur_pos = 0;
};

/* VStream means a Stream with virtual next() and put() */
class AbstractVStream
{
public:
    virtual char const *next(std::size_t howmany) = 0;
    virtual void put(std::size_t howmany) = 0;
    virtual ~AbstractVStream() = default;
};

/* adapt a stream without virtual function to 
 * an inplementation of AbstractVStream 
 */
template<class Stream>
class VStreamAdapter
    : public AbstractVStream
{
public:
    explicit VStreamAdapter(Stream &s)
        : s_(s)
    {}

    char const *next(std::size_t howmany) override {
        return s_.next(howmany);
    }

    void put(std::size_t howmany) override {
        s_.put(howmany);
    }
private:
    Stream &s_;
};

class AbstractDeferredParserOperations
{
public:
    virtual bool parse(AbstractVStream &stream) = 0;
    virtual void unparse(AbstractVStream &stream) = 0;
    virtual ~AbstractDeferredParserOperations() = default;
};

template<class Parser>
class DeferredParserOperationsAdapter
    : public AbstractDeferredParserOperations
{
public:
    explicit DeferredParserOperationsAdapter(Parser p)
        : p_(p)
    {}

    bool parse(AbstractVStream &stream) override {
        return p_.parse(stream);
    }

    void unparse(AbstractVStream &stream) override {
        p_.unparse(stream);
    }
private:
    Parser      p_;
};

class DeferredParser
    : public NonCopyable
    , public DebugPolicyOnDemand<DeferredParser>
{
public:
    template<class Stream>
    bool parse(Stream &stream) {
        debug_printf("DeferredParser:parsing...:remain:%s\n", stream.next(0));

        VStreamAdapter<Stream> sa(stream);
        return ops->parse(sa);
    }

    template<class Stream>
    void unparse(Stream &stream) {
        debug_printf("DeferredParser:unparsing...:remain:%s\n", stream.next(0));

        VStreamAdapter<Stream> sa(stream);
        ops->unparse(sa);
    }

    template<class Parser>
    DeferredParser &operator=(Parser &&p) {
        ops = std::make_unique<
            DeferredParserOperationsAdapter<
                typename std::remove_reference<Parser>::type
            >>(p);
        return *this;
    }

private:
    std::unique_ptr<AbstractDeferredParserOperations> ops;
};

template<class Parser>
struct parser_traits
{
    using real_type = typename std::remove_cv<
        typename std::remove_reference<Parser>::type
        >::type;

    using pass_type = typename std::conditional<
        std::is_base_of<DeferredParser, real_type>::value,
        real_type&, real_type>::type;

//    using pass_type = typename std::conditional<
//            std::is_base_of<DeferredParser, Parser>::value,
//            Parser&, Parser
//            >::type;
};

class ParserEpsilon
{
public:
    template<class Stream>
    bool parse(Stream &stream) { return true; }

    template<class Stream>
    void unparse(Stream &stream) {}
};

class SkipPolicySpace
{
public:
    template<class Stream>
    void skip(Stream &stream) {
        char const *p = stream.next(1);
        if ( !p || !std::isspace(*p) ) {
            if ( p ) stream.put(1);
            counts_.push(0);
            return;
        }

        std::size_t count = 1;
        while ( (p = stream.next(1)) != nullptr && std::isspace(*p) ) 
            ++count;
        if ( p ) {
            stream.put(1);
        }
        counts_.push(count);
    }

    template<class Stream>
    void unskip(Stream &stream) {
        assert(!counts_.empty());
        auto count = counts_.top();
        counts_.pop();
        if ( count ) stream.put(count);
    }
private:
    std::stack<std::size_t>     counts_;
};

class SkipPolicyNone
{
public:
    template<class Stream>
    void skip(Stream &stream) {}
    
    template<class Stream>
    void unskip(Stream &stream) {}
};

template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserChar
{
public:
    explicit ParserChar(char ch)
        : ch_(ch)
        , skipper_(SkipPolicy{})
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        skipper_.skip(stream);
        const char *p = stream.next(1);
        if ( !p ) {
            skipper_.unskip(stream);
            return false;
        }

        if ( *p != ch_ ) {
            stream.put(1);
            skipper_.unskip(stream);
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        stream.put(1);
        skipper_.unskip(stream);
    }
private:
    char        ch_;
    SkipPolicy  skipper_;
};

template<
//    class SkipPolicy = SkipPolicyNone
    class SkipPolicy = SkipPolicySpace
    >
class ParserString
{
public:
    explicit ParserString(std::string const &s)
        : s_(s)
        , skipper_(SkipPolicy{})
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        skipper_.skip(stream);
        const char *p = stream.next(s_.size());
        if ( !p ) {
            skipper_.unskip(stream);
            return false;
        }

        if ( std::memcmp(s_.c_str(), p, s_.size()) ) {
            stream.put(s_.size());
            skipper_.unskip(stream);
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        stream.put(s_.size());
        skipper_.unskip(stream);
    }
private:
    std::string s_;
    SkipPolicy  skipper_;
};

template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserEnd
{
public:
    ParserEnd()
        : skipper_(SkipPolicy{})
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        skipper_.skip(stream);
        const char *p = stream.next(1);
        if ( p ) {
            stream.put(1);
            skipper_.unskip(stream);
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        skipper_.unskip(stream);
    }

private:
    SkipPolicy  skipper_;
};

template<class ParserA, class ParserB>
class ParserAlternative
{
    enum {
        Which_A = 1,
        Which_B,
    };
public:
    ParserAlternative(ParserA const &a, ParserB const &b)
        : parserA(a)
        , parserB(b)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        if ( parserA.parse(stream) ) {
            whichs.push(Which_A);
            return true;
        }

        if ( parserB.parse(stream) ) {
            whichs.push(Which_B);
            return true;
        }

        return false;
    }

    template<class Stream>
    void unparse(Stream &stream) {
        assert( !whichs.empty() );

        int which = whichs.top();
        whichs.pop();

        if ( which == Which_A ) {
            parserA.unparse(stream);
        } else if ( which == Which_B ) {
            parserB.unparse(stream);
        } else {
            assert(0);
        }
    }

private:
    ParserA             parserA;
    ParserB             parserB;
    std::stack<int>     whichs;
};

template<class ParserA, class ParserB>
class ParserChain
{
public:
    ParserChain(ParserA const &a, ParserB const &b)
        : parserA(a)
        , parserB(b)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        if ( !parserA.parse(stream) ) {
            return false;
        }

        if ( !parserB.parse(stream) ) {
            parserA.unparse(stream);
            return false;
        }

        return true;
    }

    template<class Stream>
    void unparse(Stream &stream) {
        parserB.unparse(stream);
        parserA.unparse(stream);
    }
private:
    ParserA         parserA;
    ParserB         parserB;
};

template<class Parser>
class ParserMaybe
{
public:
    ParserMaybe(Parser const &p)
        : p_(p)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        reallis.push(p_.parse(stream));
        return true;
    }

    template<class Stream>
    bool unparse(Stream &stream) {
        assert( !reallis.empty() );

        if ( reallis.top() ) {
            p_.unparse(stream);
        }
        reallis.pop();
    }

private:
    std::stack<bool>    reallis;
    Parser              p_;
};

template<class Parser>
class ParserMany
{
public:
    ParserMany(Parser const &p)
        : p_(p)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        while ( p_.parse(stream) )
            ++count_;
        return true;
    }
    
    template<class Stream>
    void unparse(Stream &stream) {
        while ( count_-- ) {
            p_.unparse(stream);
        }
    }
private:
    Parser          p_;
    std::size_t     count_ = 0;

};

template<class ParserA, class ParserB>
auto
operator|(ParserA const &a, ParserB const &b)
{
    return ParserAlternative<
        typename parser_traits<ParserA>::pass_type,
        typename parser_traits<ParserB>::pass_type
        >(const_cast<ParserA&>(a), const_cast<ParserB&>(b));
}

template<class ParserA, class ParserB>
auto
operator>>(ParserA const &a, ParserB const &b)
{
    return ParserChain<
        typename parser_traits<ParserA>::pass_type,
        typename parser_traits<ParserB>::pass_type
        >(const_cast<ParserA&>(a), const_cast<ParserB&>(b));
}

template<class Parser, class Stream>
auto
operator>>=(Parser &&p, Stream &stream)
{
    return p.parse(stream);
}

template<class Parser>
auto
maybe(Parser const &p)
{
    return ParserMaybe<Parser>(p);
}

template<class Parser>
auto
many(Parser const &p)
{
    return ParserMany<Parser>(p);
}

template<class Parser>
auto
manyIndeed(Parser const &p)
{
    return p >> many(p);
}

void test() {
    StreamAdapter s1("hello world");
    StreamAdapter s2("hello world");
    StreamAdapter s3("hello world");

    assert(
            (ParserString<>("hello") | ParserString<>("good")) >>= s1
          );

    assert(
            (ParserString<>(" world") | ParserString<>("good")) >>= s1
          );

    assert(
            (ParserString<>("hello") >> ParserChar<>(' ') >> ParserString<>("world")) >>= s2
          );
    assert(!(
            (ParserString<>("hello") >> ParserChar<>(' ') >> ParserString<>("world ")) >>= s3
          ));

    StreamAdapter s4("hello hello hello hello hello hello hello hello hello");
    assert(
            (manyIndeed(ParserString<>("hello ")) >> ParserString<>("hello") >> ParserEnd<>()) >>= s4
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

void test3() {
    StreamAdapter s1("(abc + - ( abc * abc )) * abc + abc / abc");
    //StreamAdapter s1("(abc + abc)");

    DeferredParser expr;
//    expr
//        .set_debug_name("expr")
//        .debug()
//        ;

    auto unit = (ParserString<>("abc"))
              | (ParserChar<>('(') >> expr >> ParserChar<>(')'))
              ;

    auto bigunit = (ParserString<>("-") >> unit)
                 | unit
                 ; 

    DeferredParser factor;

//    factor
//        .set_debug_name("factor")
//        .debug()
//        ;

    factor = (bigunit >> ParserString<>("*") >> factor)
           | (bigunit >> ParserString<>("/") >> factor)
           | bigunit
           ;

    expr = (factor >> ParserString<>("+") >> expr)
         | (factor >> ParserString<>("-") >> expr)
         | factor
         ;

    assert( (expr >> ParserEnd<>()) >>= s1 );
}

int main() {
//    test();
//    test2();
    test3();
}
