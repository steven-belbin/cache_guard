// cache_guard.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    class entry
    {
        public:
        entry()=default;
        ~entry()=default;
        entry(const entry&)=default;
        int x;
    };

    using cache=std::vector<entry>;

    using cache_ptr=std::shared_ptr<cache>;

    std::mutex global_cache_mutex;
    cache_ptr global_cache;
    std::chrono::high_resolution_clock::time_point global_cache_last_access;

    std::atomic<bool> global_is_stopped={false};

    const auto maximum_time_worker=200ms;
    const auto maximum_idle_time=100ms;
    const auto maximum_unused_time=30ms;
    const auto clear_cache_interval=60ms;

    _int64 randomize_value(const _int64 upper_limit)
    {
        _int64 x=upper_limit + 1;

        while (x > upper_limit)
            x=1 + std::rand() / ((RAND_MAX + 1u) / upper_limit);  // Note: 1+rand()%6 is biased

        return x;
    }

    template<typename T>
    T randomize_time(const T upper_limit)
    {
        return T(randomize_value(upper_limit.count()));
    }

    void worker()
    {
        std::lock_guard<std::mutex> guard(global_cache_mutex);


        //
        // Using the scoped concept and swapping against the global cache.
        //
        // When the code executes to completion, then the global cache is assigned (reassigned)
        // the contents of the cache.
        //
        // If an exception, then global cache has already been reset and therefore
        // the code will not be retaining a invalid cache filled with data raised
        // during an exception.
        //
        cache_ptr scoped_cache;
        scoped_cache.swap(global_cache);

        if (scoped_cache)
        {
            std::wcout << "Grabbing the cache from the global." << std::endl;
        }
        else
        {
            scoped_cache.reset(new cache(1000));
            std::wcout << "Created a new cache." << std::endl;
        }

        const auto wait_time=randomize_time(maximum_idle_time);

        std::wcout << L"Waiting in worker for " << wait_time.count() << " milliseconds." << std::endl;

        std::this_thread::sleep_for(wait_time);

        if (randomize_value(1000) % 17 == 0)
        {
            throw std::runtime_error("A randomized exception has occurred.");
        }


        std::wcout << "Putting the cache back into the global." << std::endl;

        global_cache.swap(scoped_cache);
        global_cache_last_access=std::chrono::high_resolution_clock::now();
    }

    void worker_loop()
    {
        while (!global_is_stopped)
        {
            const auto wait_time=randomize_time(maximum_time_worker);

            std::wcout << L"Waiting in worker loop for " << wait_time.count() << " milliseconds." << std::endl;

            std::this_thread::sleep_for(wait_time);

            try
            {
                worker();
            }
            catch (const std::runtime_error& ex)
            {
                std::wcout << "Caught a " <<  ex.what() << std::endl;
            }
        }
    }

    void clear_cache_worker()
    {
        while (!global_is_stopped)
        {
            std::this_thread::sleep_for(clear_cache_interval);

            {
                std::unique_lock<std::mutex> guard(global_cache_mutex, std::try_to_lock);

                if (guard.owns_lock() && global_cache)
                {
                    const auto elapsed_time=std::chrono::high_resolution_clock::now() - global_cache_last_access;

                    if (elapsed_time > maximum_unused_time)
                    {
                        global_cache.reset();

                        std::wcout << L"Resetting the cache, since the elapsed time is "
                            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count() << " milliseconds."
                            << std::endl;
                    }
                }
            }

        }
    }
}

int main()
{
    auto worker_loop_thread = std::thread(worker_loop);
    auto wait_thread = std::thread(clear_cache_worker);

    std::this_thread::sleep_for(30s);

    global_is_stopped = true;

    worker_loop_thread.join();
    wait_thread.join();

    return 0;
}

