#ifndef _PARSERBASICS_HH_
#define _PARSERBASICS_HH_

#include "util.hh"
#include "Stream.hh"

#include <memory>
#include <type_traits>
#include <cassert>
#include <iostream>

struct YieldResult
{
    virtual ~YieldResult() = default;
    virtual void show(std::ostream &os) = 0;
};

struct YieldResultPtr {
    YieldResultPtr() = default;
    YieldResultPtr(const YieldResultPtr &other) {
        assert(other.ptr_ == nullptr && ptr_ == nullptr);
    }
    YieldResultPtr(YieldResultPtr &&) = default;
    YieldResultPtr &operator=(const YieldResultPtr &other) {
        assert(other.ptr_ == nullptr && ptr_ == nullptr);
        return *this;
    }
    YieldResultPtr &operator=(YieldResultPtr &&) = default;
    ~YieldResultPtr() = default;

    template<class U>
    YieldResultPtr(U &&u)
        : ptr_(std::forward<U>(u))
    {}

    auto &operator->() { return ptr_; }
    auto const &operator->() const { return ptr_; }

    auto &operator*() { return *ptr_; }
    auto const &operator*() const { return *ptr_; }

    std::unique_ptr<YieldResult> ptr_;
};

template<class Derived>
class ParserBase
{
public:
    template<class Func>
    auto operator[](Func &&func);
private:
    Derived &derived() {
        return *static_cast<Derived*>(this);
    }
};

template<class Parser, class Func>
class SemanticParser
    : public ParserBase<SemanticParser<Parser, Func>>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

    SemanticParser(Parser const &p, Func func)
        : p_(p)
        , func_(func)
    {}

    template<class Stream>
    bool parse(Stream &stream) {
        return p_.parse(stream);
    }

    template<class Stream>
    void unparse(Stream &stream) {
        p_.unparse(stream);
    }

    YieldResultPtr getResult() {
        return std::move(func_(p_.getTuple()));
    }

    std::tuple<YieldResultPtr> getTuple() {
        return std::move(
                std::make_tuple(getResult())
                );
    }
private:
    Parser  p_;
    Func    func_;
};

class AbstractDeferredParserOperations
{
public:
    virtual bool parse(AbstractVStream &stream) = 0;
    virtual void unparse(AbstractVStream &stream) = 0;
    virtual YieldResultPtr getResult() = 0;
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

    YieldResultPtr getResult() override {
        return std::move(p_.getResult());
    }

private:
    Parser      p_;
};

class DeferredParser
    : public NonCopyable
    , public ParserBase<DeferredParser>
    , public DebugPolicyOnDemand<DeferredParser>
{
public:
    using ResultTupleType = std::tuple<YieldResultPtr>;

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
        assert(ops == nullptr);
        ops = std::make_unique<
            DeferredParserOperationsAdapter<
                typename std::remove_reference<Parser>::type
            >>(p);
        return *this;
    }

    YieldResultPtr getResult() {
        return std::move(ops->getResult());
    }

    std::tuple<YieldResultPtr> getTuple() {
        return std::move(
                std::make_tuple(ops->getResult())
                );
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
};

template<class Derived>
template<class Func>
auto
ParserBase<Derived>::operator[](Func &&func)
{
    return SemanticParser<
        typename parser_traits<Derived>::pass_type, 
        typename std::remove_reference<Func>::type
        >(derived(), std::forward<Func>(func));
}

#endif /* _PARSERBASICS_HH_ */

