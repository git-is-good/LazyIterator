#ifndef _LAZYITERATOR_HH_
#define _LAZYITERATOR_HH_

#include <vector>
#include <iterator>
#include <type_traits>
#include <exception>
#include <utility>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <limits>

#define throw_stop_iteration()              \
    throw StopIteration(__func__);


#define must_ok()                           \
    do {                                    \
        if ( !ok() ) {                      \
            throw_stop_iteration();         \
        }                                   \
    } while (0)

class StopIteration
    : public std::exception
{
public:
    explicit StopIteration(const char* funcName)
        : funcName_(funcName)
    {}

    virtual const char *what() const noexcept {
        return funcName_;
    }
private:
    const char *funcName_ = nullptr;
};

template<class Derived>
class LazyIteratorBase;

template<class Iterator>
class LazyIteratorRaw
    : public LazyIteratorBase<LazyIteratorRaw<Iterator>>
{
    using self_type = LazyIteratorRaw;

public:
    using value_type = typename std::iterator_traits<Iterator>::value_type;

    LazyIteratorRaw(Iterator beg, Iterator end)
        : beg(beg)
        , end(end)
    {}

    LazyIteratorRaw() = default;

    self_type &operator++() {
        must_ok();
        ++beg;
        return *this;
    }

    self_type operator++(int) {
        must_ok();
        self_type res = *this;
        ++beg;
        return res;
    }

    value_type operator*() {
        must_ok();
        return *beg;
    }

    bool ok() {
        return beg != end;
    }

protected:
    Iterator beg;
    Iterator end;
};

template<class T>
class LazyIteratorWithVectorContent
    : public LazyIteratorRaw<typename std::vector<T>::iterator>
{
    using self_type = LazyIteratorWithVectorContent<T>;
public:
    LazyIteratorWithVectorContent(std::vector<T> &&vec)
        : vec(std::move(vec))
    {
        init();
    }

    LazyIteratorWithVectorContent(std::vector<T> const &vec)
        : vec(vec)
    {
        init();
    }

    self_type &sort() {
        std::sort(vec.begin(), vec.end());
        return *this;
    }

    template<class Compare>
    self_type &sort(Compare compare) {
        std::sort(vec.begin(), vec.end(), compare);
        return *this;
    }

    auto reverse() {
        using VectorIterator = typename std::vector<T>::iterator;
        using ReverseVectorIterator = std::reverse_iterator<VectorIterator>;

        return LazyIteratorRaw<ReverseVectorIterator>(
                ReverseVectorIterator(this->end),
                ReverseVectorIterator(this->beg)
                );
    }
private:
    void init() {
        this->beg = vec.begin();
        this->end = vec.end();
    }
    std::vector<T> vec;
};

template<class Generator>
class LazyIteratorWithGenerator
    : public LazyIteratorBase<LazyIteratorWithGenerator<Generator>>
{
    using self_type = LazyIteratorWithGenerator;
public:
    using value_type = std::result_of_t<Generator()>;

    LazyIteratorWithGenerator(Generator gen, ssize_t count)
        : gen_(gen)
        , count_(count)
    {
        if ( count_ != 0 ) {
            cached_ = gen_();
        }
    }

    self_type &operator++() {
        must_ok();
        next();
        return *this;
    }

    self_type operator++(int) {
        must_ok();
        self_type res = *this;
        next();
        return res;

    }

    value_type operator*() {
        must_ok();
        return cached_;
    }

    /* decrement count_ to 0, 0 indicates termination,
     * count_ == -1 means infinitely many
     */
    bool ok() {
        return count_ == -1 || count_ > 0;
    }
private:
    void next() {
        if ( count_ != -1 ) {
            if ( --count_ != 0 ) {
                cached_ = gen_();
            }
        } else {
            cached_ = gen_();
        }
    }

    Generator       gen_;
    value_type      cached_;
    ssize_t         count_;
};

template<class Iterator, class StopPred>
class LazyIteratorWithStop
    : public LazyIteratorBase<LazyIteratorWithStop<Iterator, StopPred>>
{
    using self_type = LazyIteratorWithStop;
public:
    using value_type = typename Iterator::value_type;
    static_assert(std::is_convertible_v<
            std::result_of_t<StopPred(value_type)>, bool>,
            "StopPred must return bool-convertible");

    LazyIteratorWithStop(Iterator iter, StopPred pred)
        : internal_iter_(iter)
        , stop_pred_(pred)
    {}

    self_type &operator++() {
        must_not_stop();
        ++internal_iter_;
        return *this;
    }

    self_type operator++(int) {
        must_not_stop();
        self_type res = *this;
        ++internal_iter_;
        return res;
    }

    value_type operator*() {
        value_type v = *internal_iter_;
        if ( stop_pred_(v) ) {
            throw_stop_iteration();
        }
        return v;
    }

    bool ok() {
        return internal_iter_.ok() && !stop_pred_(*internal_iter_);
    }
private:
    void must_not_stop() {
        if ( stop_pred_(*internal_iter_) ) {
            throw_stop_iteration();
        }
    }

    Iterator        internal_iter_;
    StopPred        stop_pred_;
};

template<class Iterator>
class LazyIteratorWithTake
    : public LazyIteratorBase<LazyIteratorWithTake<Iterator>>
{
    using self_type = LazyIteratorWithTake;
public:
    using value_type = typename Iterator::value_type;

    LazyIteratorWithTake(Iterator iter, size_t howmany)
        : internal_iter_(iter)
        , remain_(howmany)
    {}

    self_type &operator++() {
        must_not_stop();
        --remain_;
        ++internal_iter_;
        return *this;
    }

    self_type operator++(int) {
        must_not_stop();
        self_type res = *this;
        --remain_;
        ++internal_iter_;
        return res;
    }

    value_type operator*() {
        must_not_stop();
        return *internal_iter_;
    }

    bool ok() {
        return remain_ > 0 && internal_iter_.ok();
    }
private:
    void must_not_stop() {
        if ( !remain_ ) {
            throw_stop_iteration();
        }
    }
    Iterator        internal_iter_;
    std::size_t     remain_;
};

template<class Iterator, class MapFunc>
class LazyIteratorWithMap
    : public LazyIteratorBase<LazyIteratorWithMap<Iterator, MapFunc>>
{
    using self_type = LazyIteratorWithMap;

public:
    using value_type = std::result_of_t<MapFunc(typename Iterator::value_type)>;

    LazyIteratorWithMap(Iterator iter, MapFunc func)
        : internal_iter_(iter)
        , map_func_(func)
    {}

    self_type &operator++() {
        ++internal_iter_;
        return *this;
    }

    self_type operator++(int) {
        self_type res = *this;
        ++internal_iter_;
        return res;
    }

    value_type operator*() {
        return map_func_(*internal_iter_);
    }

    bool ok() {
        return internal_iter_.ok();
    }
private:
    Iterator        internal_iter_;
    MapFunc         map_func_;
};

template<class Iterator, class FilterFunc>
class LazyIteratorWithFilter
    : public LazyIteratorBase<LazyIteratorWithFilter<Iterator, FilterFunc>>
{
    using self_type = LazyIteratorWithFilter;

public:
    using value_type = typename Iterator::value_type;
    static_assert(std::is_convertible_v<
            std::result_of_t<FilterFunc(value_type)>, bool>,
            "filter must return bool-convertible");

    LazyIteratorWithFilter(Iterator iter, FilterFunc func)
        : internal_iter_(iter)
        , filter_func_(func)
    {
        advance();
    }

    self_type &operator++() {
        ++internal_iter_;
        advance();
        return *this;
    }

    self_type operator++(int) {
        self_type res = *this;
        ++internal_iter_;
        advance();
        return res;
    }

    value_type operator*() {
        return *internal_iter_;
    }

    bool ok() {
        return internal_iter_.ok();
    }

private:
    Iterator        internal_iter_;
    FilterFunc      filter_func_;

    void advance() {
        while ( internal_iter_.ok() && !filter_func_(*internal_iter_) ) {
            ++internal_iter_;
        }
    }
};

template<class Iterator1, class Iterator2, class Zipper>
class LazyIteratorWithZip
    : public LazyIteratorBase<LazyIteratorWithZip<Iterator1, Iterator2, Zipper>>
{
    using self_type = LazyIteratorWithZip;
public:
    using value_type = std::result_of_t<
        Zipper(typename Iterator1::value_type, typename Iterator2::value_type)
        >;

    LazyIteratorWithZip(Iterator1 iter1, Iterator2 iter2, Zipper zipper)
        : internal_iter1_(iter1)
        , internal_iter2_(iter2)
        , zipper_(zipper)
    {}

    self_type &operator++() {
        ++internal_iter1_;
        ++internal_iter2_;
        return *this;
    }

    self_type operator++(int) {
        self_type res = *this;
        ++internal_iter1_;
        ++internal_iter2_;
        return res;
    }

    value_type operator*() {
        return zipper_(*internal_iter1_, *internal_iter2_);
    }

    bool ok() {
        return internal_iter1_.ok() && internal_iter2_.ok();
    }
private:
    Iterator1       internal_iter1_;
    Iterator2       internal_iter2_;
    Zipper          zipper_;
};

/* Joiner: bool (&After, value_type)
 *
 * After concept:
 *  1) After()
 */
template<class Iterator, class Joiner, class AfterType>
class LazyIteratorWithJoin
    : public LazyIteratorBase<LazyIteratorWithJoin<Iterator, Joiner, AfterType>>
{
    using self_type = LazyIteratorWithJoin<Iterator, Joiner, AfterType>;
public:
    using value_type = AfterType;

    LazyIteratorWithJoin(Iterator iter, Joiner joiner)
        : internal_iter_(iter)
        , joiner_(joiner)
    {
        advance();
    }

    self_type &operator++() {
        must_ok();
        cached_ = false;
        advance();
        return *this;
    }

    self_type operator++(int) {
        must_ok();
        auto res = *this;
        cached_ = false;
        advance();
        return res;
    }

    AfterType operator*() {
        must_ok();
        return after_;
    }

    bool ok() {
        return cached_;
    }
private:
    Iterator        internal_iter_;
    Joiner          joiner_;

    AfterType       after_;
    bool            cached_ = false;

    // after advance, internal_iter_ always points to the next position after cached_
    void advance() {
        if ( !internal_iter_.ok() ) return;

        cached_ = true;
        after_ = AfterType();
        while ( internal_iter_.ok() && joiner_(after_, *internal_iter_) ) {
            ++internal_iter_;
        }
    }
};

template<class T>
struct TWithCount {
    T               t;
    std::size_t     count = 0;
    friend std::ostream &operator<<(std::ostream &os, TWithCount const &tg) {
        os << "[" << tg.t << ":" << tg.count << "]";
        return os;
    }
};

template<class T>
bool
tWithCountJoiner(TWithCount<T> &tw, T const &t)
{
    if ( tw.count == 0 || tw.t == t ) {
        if ( tw.count == 0 ) {
            tw.t = t;
        }
        ++tw.count;
        return true;
    } else {
        return false;
    }
}

template<class Derived>
class LazyIteratorBase {
public:
    template<class Pred>
    auto filter(Pred pred)
    {
        return LazyIteratorWithFilter<Derived, Pred>(
                *static_cast<Derived*>(this), pred
                );
    }

    template<class Func>
    auto map(Func f)
    {
        return LazyIteratorWithMap<Derived, Func>(
                *static_cast<Derived*>(this), f
                );
    }

    template<class AfterType, class Joiner>
    auto groupBy(Joiner joiner) {
        return LazyIteratorWithJoin<Derived, Joiner, AfterType>(
                *static_cast<Derived*>(this), joiner
                );
    }

    auto groupSame() {
        return groupBy<TWithCount<typename Derived::value_type>>(
                tWithCountJoiner<typename Derived::value_type>
                );

    }

    template<class Pred>
    auto stopWhen(Pred pred) {
        return LazyIteratorWithStop<Derived, Pred>(
                *static_cast<Derived*>(this), pred
                );
    }

    auto take(std::size_t howmany) {
        return LazyIteratorWithTake<Derived>(
                *static_cast<Derived*>(this), howmany
                );
    }

    template<class StoreIterator>
    void store(StoreIterator store_iter) {
        while ( static_cast<Derived*>(this)->ok() ) {
            *store_iter++ = std::move(**static_cast<Derived*>(this));
            static_cast<Derived*>(this)->operator++();
        }
    }

    /*
     * Binary: (InitValueType, value_type) -> InitValueType
     */
    template<class Binary, class InitValueType>
    auto reduce(Binary binary, InitValueType init_value) {
        static_assert(std::is_convertible_v<
                std::result_of_t<Binary(InitValueType, typename Derived::value_type)>,
                InitValueType>,
                "Binary must be InitValueType -> value_type -> InitValueType");

        auto res = init_value;
        while ( static_cast<Derived*>(this)->ok() ) {
            res = binary(res, static_cast<Derived*>(this)->operator*());
            static_cast<Derived*>(this)->operator++();
        }
        return res;
    }

    /*
     * Pred: value_type -> bool
     */
    template<class Pred>
    Derived &skipUntil(Pred pred) {
        while ( static_cast<Derived*>(this)->ok() && !pred(static_cast<Derived*>(this)->operator*()) ) {
            static_cast<Derived*>(this)->operator++();
        }
        return *static_cast<Derived*>(this);
    }

    std::size_t count() {
        std::size_t cnt = 0;
        while ( static_cast<Derived*>(this)->ok() ) {
            ++cnt;
            static_cast<Derived*>(this)->operator++();
        }
        return cnt;
    }

    auto sum() {
        return reduce([] (auto const &a, auto const &b) { return a + b; },
                typename Derived::value_type{});
    }

    auto numeric_min() {
        return reduce([] (auto const &a, auto const &b) { return a < b ? a : b; },
                std::numeric_limits<typename Derived::value_type>::max());
    }

    auto numeric_max() {
        return reduce([] (auto const &a, auto const &b) { return a > b ? a : b; },
                std::numeric_limits<typename Derived::value_type>::min());
    }

    template<class Pred>
    void foreach(Pred pred) {
        while ( static_cast<Derived*>(this)->ok() ) {
            pred(**static_cast<Derived*>(this));
            static_cast<Derived*>(this)->operator++();
        }
    }

    /* for LazyIteratorWithVectorContent:
     *
     * self_type &sort();
     *
     * template<class Compare>
     * self_type &sort(Compare compare);
     *
     * LazyIteratorRaw<ReverseVectorIterator>
     * reverse();
     */

    auto done() {
        std::vector<typename Derived::value_type> vec;
        store(std::back_inserter(vec));
        return LazyIteratorWithVectorContent<typename Derived::value_type>(std::move(vec));
    }

    auto dup() {
        return *static_cast<Derived*>(this);
    }
};

template<class Iterator>
auto
makeLazyIterator(Iterator beg, Iterator end)
{
    return LazyIteratorRaw<Iterator>(beg, end);
}

template<class Generator>
auto
makeLazyIteratorFromGenerator(Generator gen, ssize_t max_count = -1)
{
    return LazyIteratorWithGenerator<Generator>(gen, max_count);
}

template<class Iterator1, class Iterator2, class Zipper>
auto
makeLazyIteratorFromZipWith(Iterator1 iter1, Iterator2 iter2, Zipper zipper)
{
    return LazyIteratorWithZip<Iterator1, Iterator2, Zipper>(
            iter1, iter2, zipper
            );
}

template<class Iterator1, class Iterator2>
auto
makeLazyIteratorFromZip(Iterator1 iter1, Iterator2 iter2)
{
    return makeLazyIteratorFromZipWith(iter1, iter2,
            [] (auto const &a, auto const &b) { return std::make_pair(a, b); }
            );
}

#endif /* _LAZYITERATOR_HH_ */
