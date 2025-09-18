/*
Asynchronous Launch:
    "Launching a thread is like giving a task to a new worker. You tell them to start and you immediately go back to what you were doing.
    You're not blocked; you're working asynchronously with them."

Need for Synchronization:
    "Now, imagine that worker needs to use the same tool as you (the shared data). 
    You both can't use it at the same time without risking breaking it.
    So, you establish a rule: 'Whoever needs the tool first, grabs the lock on it, uses it, and then releases it.' This act of waiting for the tool introduces a moment of synchronization."

    Imagine now that the tool you and the worker are using is very simple — say, just a counter that tracks how many tasks have been completed.
Normally, to make sure neither of you messes up the counter while updating it, you’d need a lock every time you touch it.
But locks come with overhead and can slow things down if all you need is a simple update.
    This is where std::atomic comes in. You can think of it as a ‘magic tool’ that automatically ensures safe usage without you having to explicitly grab a lock. 
When either of you updates the counter, std::atomic guarantees that the operation happens atomically — it’s indivisible, safe, and consistent, even if both of you try to update it at the exact same time.
*/

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

std::atomic<int> counter{};

void incremental()
{
    for (int i{}; i < 100; ++i) ++counter;
}

void atomicCounterTester()
{
    std::vector<std::thread> threads;
    for (int i{}; i < 10; ++i) {
        threads.emplace_back(incremental);

        std::cout << "For: " << i << ", counter: " << counter << std::endl;
    }

    for (auto& t : threads) t.join();

    std::cout << "Res: " << counter << std::endl;
}

std::atomic<bool> ready{false};

void waitForReady()
{
    while (!ready.load()) {
        std::this_thread::yield(); // Hint to the OS to run other threads
    }

    std::cout << "Ready\n";
}

void readyChecker()
{
    std::thread t{waitForReady};
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ready.store(true);
    // ready = true;

    t.join();
}

// or the same using conditional variable
bool readyCV{false};
std::mutex mx;
std::condition_variable cv;

void waitForReadyCV()
{
    std::unique_lock<std::mutex> lock(mx);
    // std::lock_guard<std::mutex> lock(mx);
    std::cout << "Waiting for readyCV...\n";
    cv.wait(lock, [] { return readyCV; }); // waits efficiently
    std::cout << "Ready!\n";
}

void cvChecker()
{
    std::cout << "Creating the worker thread\n";
    std::thread t{waitForReadyCV};
    std::this_thread::sleep_for(std::chrono::seconds(3));

    {
        std::lock_guard<std::mutex> lock{mx};
        std::cout << "Setting the readyCV to true\n";
        readyCV = true;
    }

    std::cout << "Sending the notification\n";
    cv.notify_one();
    std::cout << "Joining the thread\n";
    t.join();
}

int main()
{
    atomicCounterTester();
    // readyChecker();
    // cvChecker();
}

// lock_guard vs unique lock