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

struct StopIteration : public std::exception {
    virtual const char *what() const noexcept {
        return "StopIteration";
    }
};

template<class Derived>
struct LazyIteratorBase;

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

private:
    void must_ok() {
        if ( !ok() ) {
            throw StopIteration();
        }
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

private:
    void init() {
        this->beg = vec.begin();
        this->end = vec.end();
    }
    std::vector<T> vec;
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

    void must_ok() {
        if ( !ok() ) {
            throw StopIteration();
        }
    }

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
struct LazyIteratorBase {
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

#endif /* _LAZYITERATOR_HH_ */

