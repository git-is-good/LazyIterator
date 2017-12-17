#ifndef _STREAM_HH_
#define _STREAM_HH_

#include <string>
#include <cstdlib>
#include <cassert>
#include <stack>

/*
 * Stream Concept:
 * Stream {
 *      const char *next(std::size_t howmany);
 *      void put(std::size_t howmany);
 * };
 */

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

#endif /* _STREAM_HH_ */
