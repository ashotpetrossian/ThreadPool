#include <thread>
#include <future>
#include <chrono>
#include <vector>
#include <iostream>
#include <unordered_set>


void doWork(std::unordered_set<std::thread::id>& threadIds)
{
    threadIds.insert(std::this_thread::get_id());
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void threadBased()
{
    std::vector<std::thread> threads;
    auto numThreads = std::thread::hardware_concurrency();
    std::unordered_set<std::thread::id> threadIds;
    
    auto start = std::chrono::high_resolution_clock::now();

    for (decltype(numThreads) i{}; i < numThreads; ++i) {
        threads.emplace_back(std::thread{doWork, std::ref(threadIds)});
    }
    for (auto& thread : threads) thread.join();

    auto end = std::chrono::high_resolution_clock::now();

    // Compute duration in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Thread Based:: Time taken: " << duration.count() << " ms\n";
    std::cout << "Thread Based:: Num of hardware threads: " << numThreads << ", size of set: " << threadIds.size() << std::endl;
}

void taskBased()
{
    std::vector<std::future<void>> futures;
    auto numThreads = std::thread::hardware_concurrency();
    std::unordered_set<std::thread::id> threadIds;
    
    auto start = std::chrono::high_resolution_clock::now();

    for (decltype(numThreads) i{}; i < numThreads; ++i) {
        futures.emplace_back(std::async(std::launch::async, doWork, std::ref(threadIds)));
    }
    for (auto& future : futures) future.get();

    auto end = std::chrono::high_resolution_clock::now();

    // Compute duration in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Task Based:: Time taken: " << duration.count() << " ms\n";
    std::cout << "Task Based:: Num of hardware threads: " << numThreads << ", size of set: " << threadIds.size() << std::endl;    
}


// the reasin we need std::red for std::async or std::thread is not the expected param type
template <typename T, typename... Args>
void foo(T&& param, Args&&... args)
{   
    (param.push_back(std::forward<Args>(args)), ...);
}

int main()
{
    std::vector<int> vec{5};
    foo(vec, 12);
    for (int i : vec) std::cout << i << " ";
    std::cout << std::endl;
    // threadBased();
    // taskBased();
}