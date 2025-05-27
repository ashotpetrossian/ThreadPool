#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>

#include <random> // for testing

/**
                                                        About the constructor
🔹 1. What is the constructor of the ThreadPool doing?
    The constructor is initializing a number of worker threads equal to the number of available hardware threads on the system.
    Each thread enters an infinite loop where it waits for tasks to be assigned from a shared task queue.
    When tasks become available, each worker picks up a task, executes it, and then goes back to waiting for more work.
    This setup allows tasks to be executed concurrently and efficiently across CPU cores.
🔹 2. Why and for what is the condition_variable waiting?
    The condition variable is used to block worker threads when there's no work to do. It keeps them sleeping until either:
    A new task is added to the queue, or
    The thread pool is being shut down.
    This avoids wasteful CPU usage that would result from continuous polling. The waiting thread wakes up only when one of those two conditions is met.

🔹 3. Why is the front of the taskQueue moved?
    Tasks in the queue are represented as callable objects.
    These can be expensive to copy, especially if they capture a lot of data.
    Using move semantics allows the thread to take ownership of the task without duplicating it.
    This is both efficient and necessary to avoid extra memory usage and copying overhead.

🔹 4. Why do we unlock the mutex before calling the task?
    Once a task is removed from the queue, there's no need to keep holding the mutex. 
    Releasing the lock immediately allows other threads to access the shared queue and continue working.
    If the mutex were held during task execution - which might take time - it would block other threads from submitting or retrieving tasks,
    leading to reduced concurrency and possible deadlocks. Unlocking before execution ensures high parallel efficiency and avoids contention.

                                                        About the "submit" function
🔹 1. What does the submit function take as arguments?
    The submit function accepts:
        A callable (like a function, lambda, or functor), and
        A variable number of arguments to pass to that callable.
    This allows users to submit any type of task, with any number of parameters, to the thread pool.
    The arguments are stored using perfect forwarding to preserve their value category (e.g., lvalues/rvalues).
🔹 2. Why does it create a func callable object?
    A callable object is created by binding the function and its arguments together into a single unit.
    This is necessary because the task may not take parameters directly when it's executed by a worker thread.
    By binding the arguments up front, we create a ready-to-run task that requires no additional input when invoked.
    This abstraction simplifies later task execution.
🔹 3. Why do we use a taskPtr with shared_ptr?
    We wrap the callable object in a shared_ptr to a packaged_task. There are two key reasons:
        1:  Type erasure: packaged_task is templated with a specific return type and function signature.
            But inside the thread pool, we store tasks as plain std::function<void()>.
            Using a shared pointer lets us manage the lifetime of the strongly-typed task while submitting it into a type-erased container.
        2:  Safe and flexible ownership: The lambda that wraps and executes the task captures the shared_ptr.
            This guarantees the task will stay alive until it's executed, even if the original submit call returns immediately.

    In short, this technique hides the actual type while preserving proper lifetime and functionality.
🔹 4. Why use std::packaged_task and why do we need a future?
    std::packaged_task wraps a function and connects it to a future, allowing us to retrieve the result of the computation later.
    The caller of submit receives this future so they can get() the result whenever they want - potentially after the task is already complete.
    It enables asynchronous execution with result tracking, which is essential in many real-world applications (e.g., getting return values, error handling, or synchronization).
    Without a packaged_task, we couldn't easily bridge the gap between the user submitting work and getting results back later.

🔹 5. Why do we use notify_one and why is it called inside the lock_guard scope?
    We use notify_one because we're adding just a single task to the queue.
    Only one worker thread needs to be woken up to handle that task.
    Waking up more threads with notify_all would be wasteful and could lead to unnecessary contention.
    notify_one() is called while still holding the lock (inside the scope of lock_guard) to ensure that:
        The task is fully enqueued before any worker thread is woken.
        No worker thread wakes up, checks the queue, and sees it empty - which could happen if we released the lock before notifying.
        This pattern avoids subtle race conditions and is a standard best practice in producer-consumer designs using std::condition_variable.

                                                        About the Destructor
* The destructor ensures a graceful shutdown of the thread pool. Here's what happens:
    1: Signals all threads to stop by setting the stop flag to true.
    2: Notifies all worker threads waiting on the condition variable, so they can check the stop flag and exit their loops.
    3: Joins all threads, meaning the destructor waits for every worker thread to finish before the thread pool is destroyed.
This guarantees that no thread is left running and all resources are safely cleaned up.
*/

class ThreadPool
{
public:
    // Constructor initializes the thread pool
    ThreadPool() : nThreads{std::thread::hardware_concurrency()}
    {
        std::cout << "Hardware available threads: " << nThreads << std::endl;

        // Create and start worker threads
        for (size_t i{}; i < nThreads; ++i) {
            workerThreads.emplace_back([this] {
                while (true) {
                    std::unique_lock<std::mutex> lock{mx};
                    
                    // Wait until either new tasks are available or stop flag is set
                    cv.wait(lock, [this] {
                        return stop || !taskQueue.empty();
                    });

                    // If stopping and no tasks left, exit the thread
                    if (stop && taskQueue.empty()) return;

                    // Retrieve the next task
                    auto task{std::move(taskQueue.front())};
                    taskQueue.pop();

                    std::cout << "Task is being executed by thread: " << std::this_thread::get_id() << std::endl;

                    lock.unlock();  // Unlock before running the task
                    task();         // Execute the task
                }
            });
        }
    }

    // Destructor gracefully shuts down the thread pool
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock{mx};
            stop = true;  // Set the stop flag
        }

        cv.notify_all();  // Wake up all threads

        // Wait for all worker threads to finish
        for (auto& thread : workerThreads) {
            thread.join();
        }
    }

    // Submit a task to the thread pool
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
    {
        // Bind function and arguments into a callable object
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // Wrap the callable into a packaged_task for future support
        auto taskPtr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(std::move(func));
        std::future<decltype(f(args...))> future = taskPtr->get_future();

        // Create a wrapper function that can be stored in the task queue
        auto wrapper = [taskPtr] { (*taskPtr)(); };

        {
            std::lock_guard<std::mutex> lock{mx};
            taskQueue.emplace(wrapper);  // Enqueue the task
            cv.notify_one();             // Notify one waiting thread
        }

        return future;  // Return the future to the caller
    }

private:
    std::queue<std::function<void()>> taskQueue;  // Queue of tasks
    std::vector<std::thread> workerThreads;       // Worker threads

    std::condition_variable cv;  // Condition variable for signaling
    std::mutex mx;               // Mutex for shared data protection

    size_t nThreads{};  // Number of threads
    bool stop{false};   // Flag to stop the pool
};

int test1(const std::string& s, int i)
{
    int res{};
    for (char c : s) res += int(c) * i;
    return res;
}

std::vector<int> test2(int n, int m)
{
    if (n > m) std::swap(n, m);
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<int> res;
    for (int i{}; i < 10; ++i) {
        // Create a uniform integer distribution between 1 and 6 (inclusive)
        std::uniform_int_distribution<> distribution(n, m);
        res.push_back(distribution(gen));
    }

    return res;
}

int main()
{
    ThreadPool tp;
    // std::vector<std::future<int>> futures;

    auto start = std::chrono::steady_clock::now();

    // Submit 100 tasks to the thread pool
    for (int i{}; i < 100; ++i) {
        // futures.emplace_back(tp.submit([i] {
        //     std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulate work
        //     return i;
        // }));

        auto future1 = tp.submit(test1, "hello world", 9);
        std::cout << "The result of the test1: " << future1.get() << std::endl;
        auto future2 = tp.submit(test2, i + 1000, i + 5000);
        auto vec = future2.get();
        
        std::cout << "The result of the test2: " << std::endl;
        for (int i : vec) std::cout << i << " ";
        std::cout << std::endl; 
    }
    

    // Wait for all futures to complete and print results
    // for (auto& future : futures) {
    //     std::cout << "Task: " << future.get() << " completed" << std::endl;
    // }

    auto end = std::chrono::steady_clock::now();

    // Calculate and print total execution time
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Total time: " << elapsed_ms / 1000.0 << " seconds\n";
}
