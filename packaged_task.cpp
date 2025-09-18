/*
std::packaged_task: The Explicit Task Manager
    std::packaged_task is a class that wraps a callable object (a function, lambda, or function object) and automatically links it to a std::promise and a std::future.
    It's the mechanism that std::async uses internally.
    The key difference is that you are responsible for executing the task, not the C++ runtime.
    
    Think of it as a self-contained unit of work.
    You can create a task, hand it off to a specific thread, and then retrieve its result later via the future.
    This gives you explicit control over when and where the task runs, making it a perfect building block for more complex systems like a --> thread pool <--.

How It Works
    Wrap a Callable: You create a std::packaged_task object, passing a function or lambda to its constructor. It takes ownership of this callable.
    Get a future: You get a std::future from the packaged task using its .get_future() method. This is the handle you'll use to retrieve the result.
    Execute the Task: You can execute the task by invoking the std::packaged_task object as if it were a function (using the () operator).
        This call will run the wrapped callable and automatically set the result or any exceptions in the associated promise.
    Retrieve the Result: The thread that holds the future can then call .get() to retrieve the result.


*/


#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <string>

int longComputation(int x)
{
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Computation done in thread: " << std::this_thread::get_id() << std::endl;
    return x * 2;
}

int main()
{
    // 1. Create a packaged_task object
    std::packaged_task<int(int)> task(longComputation);

    // 2. Get the future before executing the task
    task(10);
    std::future<int> result_future{task.get_future()};

    // 3. Create a thread and give it the packaged_task
    // Note: packaged_task is movable, but not copyable
    // std::thread worker_thread(std::move(task), 10); // Pass the task and its argument
    // 4. In the main thread, we wait and then get the result
    std::cout << "Main thread waiting for the result...\n";
    int result = result_future.get();

    std::cout << "Main thread received result: " << result << std::endl;

    // 5. Join the worker thread
    // worker_thread.join();
}