#ifndef _PARSERLIBRARY_HH_
#define _PARSERLIBRARY_HH_

#include "util.hh"
#include "ParserBasics.hh"
#include "ParserCombinators.hh"
#include "Stream.hh"

#include <tuple>
#include <cstring>
#include <cctype>
#include <string>
#include <utility>
#include <memory>

struct IntYieldResult
    : public YieldResult
{
    explicit IntYieldResult(long num)
        : num_(num)
    {}

    void show(std::ostream &os) override {
        os << "[Int: " << num_ << "]";
    }
    long num_;
};

struct StringYieldResult
    : public YieldResult
{
    explicit StringYieldResult(std::string const &s)
        : s_(s)
    {}

    void show(std::ostream &os) override {
        os << "[String: " << s_ << "]";
    }

    std::string s_;
};

template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserChar
    : public ParserBase<ParserChar<SkipPolicy>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

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

    YieldResultPtr getResult() {
        return std::move(result_);
    }

    auto getTuple() {
        return std::move(
                std::make_tuple<YieldResultPtr>(std::move(result_))
                );
    }
private:
    char        ch_;
    SkipPolicy  skipper_;

    YieldResultPtr  result_;
};

template<
    int IntBase = 10,
    class SkipPolicy = SkipPolicySpace
    >
class ParserInt
    : public ParserBase<ParserInt<IntBase, SkipPolicy>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

    template<class Stream>
    bool parse(Stream &stream) {
        skipper_.skip(stream);
        const char *p = stream.next(1);

        if ( !p || !std::isdigit(*p) ) {
            if ( p ) stream.put(1);
            skipper_.unskip(stream);
            return false;
        }

        int count = 1;
        num_ = *p - '0';
        
        while ( (p = stream.next(1)) != nullptr ) {
            if ( std::isdigit(*p) ) {
                ++count;
                num_ = 10 * num_ + (*p - '0');
            } else {
                stream.put(1);
                break;
            }
        }
        counts_.push(count);
        return true;
    }

    template<class Stream>
    void unparse(Stream &stream) {
        assert( !counts_.empty() );
        auto count = counts_.top();
        counts_.pop();
        stream.put(count);
        skipper_.unskip(stream);
    }

    YieldResultPtr getResult() {
        return std::make_unique<IntYieldResult>(num_);
    }

    ResultTupleType getTuple() {
        return std::move(
                std::make_tuple(getResult())
                );
    }
private:
    SkipPolicy              skipper_;
    long                    num_ = 0;
    std::stack<int>         counts_;
};

//TODO: add escape chars
template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserLiteral
    : public ParserBase<ParserLiteral<SkipPolicy>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

    template<class Stream>
    bool parse(Stream &stream) {
        skipper_.skip(stream);
        const char *p = stream.next(1);
        if ( !p || *p != '\"' ) {
            if ( p ) stream.put(1);
            skipper_.unskip(stream);
            return false;
        }

        const char *start = p;
        int count = 1;
        while ( (p = stream.next(1)) != nullptr && *p != '\"' ) {
            ++count;
        }

        if ( !p ) {
            // "... unexpected EOF
            stream.put(count);
            skipper_.unskip(stream);
            return false;
        } else {
            ss_ = std::string(start + 1, p);
            counts_.push(count + 1);
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        assert(!counts_.empty());
        stream.put(counts_.top());
        counts_.pop();
        skipper_.unskip(stream);
    }

    YieldResultPtr getResult() {
        return std::move(std::make_unique<StringYieldResult>(ss_));
    }

    ResultTupleType getTuple() {
        return std::move(std::make_tuple(getResult()));
    }
private:
    SkipPolicy              skipper_;
    std::string             ss_;
    std::stack<int>         counts_;
};

template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserString
    : public ParserBase<ParserString<SkipPolicy>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

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
            result_ = std::make_unique<StringYieldResult>(s_);
            return true;
        }
    }

    template<class Stream>
    void unparse(Stream &stream) {
        stream.put(s_.size());
        skipper_.unskip(stream);
    }

    YieldResultPtr getResult() {
        return std::move(result_);
    }

    auto getTuple() {
        return std::move(
                std::make_tuple<YieldResultPtr>(std::move(result_))
                );
    }
private:
    std::string         s_;
    SkipPolicy          skipper_;
    YieldResultPtr      result_;
};

template<
    class SkipPolicy = SkipPolicySpace
    >
class ParserEnd
    : public ParserBase<ParserEnd<SkipPolicy>>
{
public:
    using ResultTupleType = std::tuple<>;
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

    YieldResultPtr getResult() { return nullptr; }

    std::tuple<> getTuple() {
        return std::make_tuple<>();
    }
private:
    SkipPolicy  skipper_;
};


#endif /* _PARSERLIBRARY_HH_ */
