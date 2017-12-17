#ifndef _PARSERCOMBINATORS_HH_
#define _PARSERCOMBINATORS_HH_

#include "util.hh"
#include "Stream.hh"
#include "ParserBasics.hh"

#include <tuple>
#include <stack>
#include <memory>
#include <utility>

template<class Tuple>
struct TupleYieldResult
    : public YieldResult
{
    Tuple tp_;
    void show(std::ostream &os) override {
        os << "{Tuple of size: " << std::tuple_size<Tuple>::value << "\n";
        tuple_foreach(tp_,
                [&os] (YieldResultPtr const &e) -> void {
                    e->show(os);
                });
        os << "\n}";
    }
};

class ParserEpsilon
    : public ParserBase<ParserEpsilon>
{
public:
    using ResultTupleType = std::tuple<>;

    template<class Stream>
    bool parse(Stream &stream) { return true; }

    template<class Stream>
    void unparse(Stream &stream) {}

    YieldResultPtr getResult() { return nullptr; }

    std::tuple<> getTuple() {
        return std::make_tuple<>();
    }
};

template<class ParserA, class ParserB>
class ParserAlternative
    : public ParserBase<ParserAlternative<ParserA, ParserB>>
{
    enum {
        Which_A = 1,
        Which_B,
    };
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;
    ParserAlternative(ParserA const &a, ParserB const &b)
        : parserA(a)
        , parserB(b)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        assert( result_.ptr_ == nullptr );
        if ( parserA.parse(stream) ) {
            whichs.push(Which_A);
            result_ = parserA.getResult();
            return true;
        }

        if ( parserB.parse(stream) ) {
            whichs.push(Which_B);
            result_ = parserB.getResult();
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

    YieldResultPtr getResult() {
        return std::move(result_);
    }

    auto getTuple() {
        return std::move(
                std::make_tuple<YieldResultPtr>(getResult())
                );
    }
private:
    ParserA             parserA;
    ParserB             parserB;
    std::stack<int>     whichs;

    YieldResultPtr      result_;
};

template<class ParserA, class ParserB>
class ParserChain
    : public ParserBase<ParserChain<ParserA, ParserB>>
{
public:
    using ResultTupleType = decltype(std::tuple_cat(
            typename parser_traits<ParserA>::real_type::ResultTupleType{},
            typename parser_traits<ParserB>::real_type::ResultTupleType{}
            ));
    ParserChain(ParserA const &a, ParserB const &b)
        : parserA(a)
        , parserB(b)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        if ( !parserA.parse(stream) ) {
            return false;
        }

        /* This parser must fetch the result immediately,
         * getTuple() or getResult() is called whenever a parser
         * succeeds, this is our semantics.
         * In fact, because of DeferredParser, a parser might call
         * itself during its call to its template parameters,
         * if getResult() is not called immediately, the result
         * might be overwritten.
         */
        auto tpa = parserA.getTuple();
        if ( !parserB.parse(stream) ) {
            parserA.unparse(stream);
            return false;
        }

        tp_ = std::tuple_cat(std::move(tpa), parserB.getTuple());

        return true;
    }

    template<class Stream>
    void unparse(Stream &stream) {
        parserB.unparse(stream);
        parserA.unparse(stream);

        tp_ = ResultTupleType{};
    }

    YieldResultPtr getResult() {
        TupleYieldResult<ResultTupleType> res;
        res.tp_ = std::move(tp_);
        return std::make_unique<TupleYieldResult<ResultTupleType>>(
                std::move(res)
                );
    }

    auto getTuple() {
        return std::move(tp_);
    }
private:
    ParserA         parserA;
    ParserB         parserB;
    ResultTupleType tp_;
};

//TODO: add result_ generation
template<class Parser>
class ParserMany
    : public ParserBase<ParserMany<Parser>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

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

    YieldResultPtr getResult() {
        return std::move(result_);
    }

    auto getTuple() {
        return std::move(
                std::make_tuple<YieldResultPtr>(std::move(result_))
                );
    }
private:
    Parser          p_;
    std::size_t     count_ = 0;

    YieldResultPtr  result_;
};

template<class ParserA, class ParserB>
auto
operator|(ParserA &&a, ParserB &&b)
{
    return ParserAlternative<
        typename parser_traits<ParserA>::pass_type,
        typename parser_traits<ParserB>::pass_type
        >(std::forward<ParserA>(a), std::forward<ParserB>(b));
}

template<class ParserA, class ParserB>
auto
operator>>(ParserA &&a, ParserB &&b)
{
    return ParserChain<
        typename parser_traits<ParserA>::pass_type,
        typename parser_traits<ParserB>::pass_type
        >(std::forward<ParserA>(a), std::forward<ParserB>(b));
}

template<class Parser, class Stream>
auto
operator>>=(Parser &&p, Stream &stream)
{
    return p.parse(stream);
}

template<class Parser>
auto
maybe(Parser &&p)
{
    return std::forward<Parser>(p) >> ParserEpsilon();
}

template<class Parser>
auto
many(Parser &&p)
{
    return ParserMany<
        typename parser_traits<Parser>::pass_type
        >(std::forward<Parser>(p));
}

template<class Parser>
auto
manyIndeed(Parser &&p)
{
    return std::forward<Parser>(p) >> many(std::forward<Parser>(p));
}

#endif /* _PARSERCOMBINATORS_HH_ */

