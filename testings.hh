#ifndef _TESTINGS_HH_
#define _TESTINGS_HH_

#include <chrono>
#include <string>
#include <ctime>

struct TimeInterval {
    std::chrono::time_point<std::chrono::high_resolution_clock>  start;
    std::string message;
    int nops;
    explicit TimeInterval(std::string const &message = "unknown", int nops = 0 )
        : message(message)
        , nops(nops)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~TimeInterval() {
        auto period = std::chrono::high_resolution_clock::now() - start;
        if ( nops != 0 ) {
            printf("<%s> duration: %-9.3lfms, %-7.3lf ns/op\n", message.c_str(), 
                    std::chrono::duration<double, std::milli>(period).count(),
                    (double)std::chrono::duration_cast<std::chrono::nanoseconds>(period).count() / nops
                  );
        } else {
            printf("<%s> duration: %lfms\n", message.c_str(), 
                    std::chrono::duration<double, std::milli>(period).count());
        }
    }
};

#endif /* _TESTINGS_HH_ */
