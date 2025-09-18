/*
Introduction to std::async  
    std::async is a function template that runs a callable object asynchronously and returns a std::future that will hold the result.
    It's the highest-level and most convenient way to launch an asynchronous task in C++.
    Think of it as a simplified, one-line version of the std::promise, std::future, and std::thread setup we discussed earlier.

Why Use std::async?
    Simplicity: It hides the complex boilerplate. Instead of manually creating a std::promise, getting its std::future, and launching a std::thread, you simply call std::async with the function and its arguments.
    Automatic Thread Management: The library decides whether to launch a new thread, reuse an existing one, or even run the task on the calling thread. This is managed by the std::launch policy.
    Automatic Synchronization: It handles the synchronization for you. The returned std::future will block on a call to .get() until the task is complete, and it automatically propagates any exceptions.

std::async: The Superior Choice
    When you want to run a function asynchronously, you have two primary options:
        Thread-Based: Manually creating a std::thread to run your function.
        Task-Based: Using the std::async function.
    The task-based approach using std::async is almost always the better choice. It offloads a lot of difficult work from you, the programmer, to the C++ Standard Library.


--> The Problems with std::thread
    Programming directly with std::thread creates several difficult-to-solve problems that std::async handles automatically.
        1. Getting Results and Handling Exceptions
            With std::thread, there's no easy way to get a return value or an exception back from your thread's function.
            If that function throws an exception, your entire program will crash! std::async solves this by returning a std::future,
            which provides a safe, simple way to retrieve the return value and a robust mechanism for propagating exceptions back to the calling thread.
        2. Thread Exhaustion
            The number of software threads a system can support is limited.
            If you try to create too many std::thread objects, your program will throw a std::system_error exception and fail.
            How do you handle this? You could try to wait, but that might lead to a deadlock.  std::async almost never throws this exception.
        3. Oversubscription and Performance
            Oversubscription happens when you have more active threads than your CPU has physical cores (hardware threads).
            This causes the operating system to constantly "context-switch" between threads, which is very inefficient. Context switching:
                Wastes CPU cycles on management instead of computation.
                "Cools" the CPU caches, forcing the system to re-load data from slower memory.
            Avoiding oversubscription is extremely difficult because the optimal number of threads changes dynamically.

--> Why std::async Is the Solution 
    Using std::async shifts the responsibility for all these problems to the C++ Standard Library implementer, who is much better equipped to handle them.
    Smart Thread Management: std::async doesn't guarantee a new thread will be created.
        With the default launch policy, it can choose to run the task on the thread that calls future.get() if the system is oversubscribed.
        This helps prevent thread exhaustion and oversubscription.
    Load Balancing: The runtime scheduler managing std::async tasks has a system-wide view of all running processes, giving it better information to make smart decisions about when and where to run your tasks.
    Automatic Benefits: std::async allows you to automatically benefit from advanced threading technologies like thread pools and work-stealing algorithms if they're implemented in your C++ runtime.
        You don't have to write that complex code yourself.

! NOTE: std::async isn't a thread pool; it's a convenient function that allows the underlying implementation to use a thread pool for you, without you having to manage that complexity yourself.


There are two primary policies:
    1. std::launch::async (The "Now!" Policy)
        What it does: This policy guarantees that your function will be executed on a new thread immediately. It's what you probably have in mind when you think of "asynchronous" execution.
        When to use it: Use this when you absolutely need a task to run concurrently, for example, to unblock a GUI thread or to perform truly parallel work.
    2. std::launch::deferred (The "Later" Policy)
        What it does: This policy is for lazy execution. The function is not run on a new thread. Instead, its execution is deferred until you call .get() or .wait() on the returned future.
            At that point, the function runs synchronously on the same thread that made the call, blocking it until the task is complete.
            If you never call .get() or .wait(), the function will never run.
        When to use it: This is useful for tasks that are expensive to run and whose results you might not always need.    

The Default Policy: The "It's Complicated" Option
    Perhaps surprisingly, the default launch policy—the one std::async uses if you don't specify one—is a combination of both async and deferred.
    The C++ runtime is allowed to choose whichever policy it wants.
This flexibility is what makes std::async so powerful for solving the problems of thread exhaustion and oversubscription.
The library can decide --> "I'll run this now on a new thread because there's plenty of room," or "The system is busy, I'll defer this task and run it later on the main thread if the result is requested."

However, this flexibility comes at a cost for the programmer: uncertainty. You can no longer guarantee the following:
    Concurrency: You don't know if your task will run concurrently.
    Thread Identity: You can't predict which thread will execute your function. This is especially problematic if your function uses thread_local variables.
    Execution Guarantee: The task might never run at all if no one ever calls .get() or .wait() on the future.
*/


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

void async_policy_1()
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

void async_policy_2()
{
    std::vector<std::future<void>> futures;
    auto numThreads = std::thread::hardware_concurrency();
    std::unordered_set<std::thread::id> threadIds;
    
    auto start = std::chrono::high_resolution_clock::now();

    for (decltype(numThreads) i{}; i < 10; ++i) {
        futures.emplace_back(std::async(std::launch::deferred, doWork, std::ref(threadIds)));
    }
    for (auto& future : futures) future.get();

    auto end = std::chrono::high_resolution_clock::now();

    // Compute duration in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Task Based:: Time taken: " << duration.count() << " ms\n";
    std::cout << "Task Based:: Num of hardware threads: " << numThreads << ", size of set: " << threadIds.size() << std::endl;
    
    auto id1 = *threadIds.begin();
    std::cout << "this thread: " << std::this_thread::get_id() << ", set-> " << id1 << std::endl;
}

void async_policy_3()
{
    std::vector<std::future<void>> futures;
    auto numThreads = std::thread::hardware_concurrency();
    std::unordered_set<std::thread::id> threadIds;
    
    auto start = std::chrono::high_resolution_clock::now();

    for (decltype(numThreads) i{}; i < 10000; ++i) {
        futures.emplace_back(std::async(doWork, std::ref(threadIds)));
    }
    for (auto& future : futures) future.get();

    auto end = std::chrono::high_resolution_clock::now();

    // Compute duration in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Task Based:: Time taken: " << duration.count() << " ms\n";
    std::cout << "Task Based:: Num of hardware threads: " << numThreads << ", size of set: " << threadIds.size() << std::endl;    
}

int main()
{
    // async_policy_1();
    // async_policy_2();
    async_policy_3();
}