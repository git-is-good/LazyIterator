/*
 * Stream Concept:
 * Stream {
 *      const char *next(std::size_t howmany);
 *      void put(std::size_t howmany);
 * };
 */

#include <cstdlib>
#include <cstring>
#include <string>
#include <stack>
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

class ParserChar
{
public:
    explicit ParserChar(char ch)
        : ch_(ch)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        const char *p = stream.next(1);
        if ( !p ) return false;

        if ( *p != ch_ ) {
            stream.put(1);
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        stream.put(1);
    }
private:
    char ch_;
};

class ParserString
{
public:
    explicit ParserString(std::string const &s)
        : s_(s)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        const char *p = stream.next(s_.size());
        if ( !p ) return false;

        if ( std::memcmp(s_.c_str(), p, s_.size()) ) {
            stream.put(s_.size());
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        stream.put(s_.size());
    }
private:
    std::string s_;
};

class ParserEnd
{
public:
    template<class Stream>
    bool parse(Stream &stream) {
        const char *p = stream.next(1);
        if ( p ) {
            stream.put(1);
            return false;
        } else {
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) { }
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
//alternative(ParserA const &a, ParserB const &b)
operator|(ParserA const &a, ParserB const &b)
{
    return ParserAlternative<ParserA, ParserB>(a, b);
}

template<class ParserA, class ParserB>
auto
//chain(ParserA const &a, ParserB const &b)
operator>>(ParserA const &a, ParserB const &b)
{
    return ParserChain<ParserA, ParserB>(a, b);
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
            (ParserString("hello") | ParserString("good")) >>= s1
          );

    assert(
            (ParserString(" world") | ParserString("good")) >>= s1
          );

    assert(
            (ParserString("hello") >> ParserChar(' ') >> ParserString("world")) >>= s2
          );
    assert(!(
            (ParserString("hello") >> ParserChar(' ') >> ParserString("world ")) >>= s3
          ));

    StreamAdapter s4("hello hello hello hello hello hello hello hello hello");
    assert(
            (manyIndeed(ParserString("hello ")) >> ParserString("hello") >> ParserEnd()) >>= s4
          );
}

void test2() {

}

int main() {
    test();
}
