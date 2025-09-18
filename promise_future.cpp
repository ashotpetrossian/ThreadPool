/*

🔹 Definition
    - std::promise → an object that produces a value (or exception) meant to be retrieved later.
    - std::future → an object that consumes that value, i.e., allows you to wait for it and retrieve it once it’s ready.

They always come in pairs:
    - One thread sets a result into a promise.
    - Another thread gets that result via a future.


🔹 Motivation
In multithreaded code, threads often need to communicate results.
    - With raw threads, if one thread computes something, how does another thread safely get that result?
    - You could use shared variables + mutexes, but that’s error-prone.

std::promise / std::future give a safe channel for one-time communication between threads.
    - Think of it as a thread-safe package delivery system:
    - The producer thread: puts the package in the box (promise.set_value).
    - The consumer thread: waits for the package and opens it (future.get).

🔹 Purpose
- Synchronization → the consumer can wait until the producer provides the result.
- Exception propagation → if the producer throws, the exception is stored in the promise and re-thrown in the future.
- Cleaner code → avoids manual synchronization with condition variables or atomics for one-time results.

🔹 Example Analogy
Think of it as an asynchronous package delivery service:
* You (the main thread) order a pizza (start a long-running task).
* The pizza shop (the new thread) starts making the pizza. The shop hands you a tracker ID for your order, which is your std::future. You can't eat the tracker, but it's your way of knowing when the pizza is ready.
* The pizza shop promises to deliver the pizza (this is the std::promise).
* You wait for the delivery (calling future.get()).
* The delivery person arrives with the pizza (promise.set_value()). You take the pizza, and future.get() returns the value. The tracker ID is now "used up" and can't be used again.
- If the pizza shop runs out of ingredients (promise.set_exception()), you are notified of the failure, and future.get() will re-throw the same exception, allowing you to handle the error gracefully.
- This system effectively decouples the producer thread from the consumer thread. The consumer doesn't need to know how or when the pizza is being made—it only needs the tracker ID to eventually get the result.

*/

#include <thread>
#include <future>
#include <vector>
#include <iostream>

void pizzaShop(std::promise<std::string>&& pizzaPromise)
{
    std::cout << "Pizza Shop:: Starting to prepare the pizza...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::string pizza = "Pepperoni pizza";

    try {
        std::cout << "Pizza Shop:: The order is ready\n";
        // throw std::runtime_error("Pizza maker error");
        pizzaPromise.set_value(pizza);

        // pizzaPromise.set_value("MArgarita");

    }
    catch (const std::exception& e) {
        std::cerr << "Pizza Shop:: Failed to make the pizza, reason: " << e.what() << std::endl;
        pizzaPromise.set_exception(std::current_exception());
    }

    // std::cout << "Pizza Shop:: continues his work\n";
    // std::this_thread::sleep_for(std::chrono::seconds(5));
}

void order1()
{
    std::cout << "Creating a pizza order\n";
    std::promise<std::string> pizzaPromise;

    std::cout << "Getting the order ID\n";
    std::future<std::string> pizza = pizzaPromise.get_future();
    
    std::thread t{pizzaShop, std::move(pizzaPromise)};
    
    std::cout << "Waiting for the pizza to be delivered\n";
    try {
        std::string peperoni = pizza.get();
        std::cout << "Pizza is ready: " << peperoni << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to get the pizza: " << e.what() << std::endl;
    }

    std::cout << "Joining the thread\n";
    t.join();
}

void order2()
{
    std::promise<std::string> p;
    // std::cout << "Promise size: " << sizeof(p) << std::endl;
    std::future<std::string> f{p.get_future()};
    std::thread t{pizzaShop, std::move(p)};

    std::cout << "Consumer:: waiting for the pizza to be ready\n";
    f.wait(); // blocks current thread
    std::cout << "Great, the pizza is ready, I'll get it later\n";

    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::string pizza = f.get();
    std::cout << "Consumer got the pizza: " << pizza << std::endl;

    t.join();
}

void order3()
{
    std::promise<std::string> p;
    std::future<std::string> f{p.get_future()};
    std::thread t{pizzaShop, std::move(p)};

    std::future_status status = f.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::ready) {
        std::cout << "Consumer:: Pizza arrived on time\n";
    }
    else {
        std::cout << "Consumer:: Time out! The pizza is still not here\n";
    }

    std::cout << "Customer:: Waiting for it now without a timeout...\n";
    std::string pizza = f.get(); // This will now block for the remaining 2 seconds
    std::cout << "Customer:: " << pizza << " is finally here.\n";

    t.join();
}

void order4()
{
    std::promise<std::string> p;
    std::future<std::string> f{p.get_future()};
    std::thread t{pizzaShop, std::move(p)};

    auto deadline = std::chrono::high_resolution_clock::now() + std::chrono::seconds(2);
    std::cout << "Consumer:: Waiting for delivery until the deadline\n";

    std::future_status status = f.wait_until(deadline);

    if (status == std::future_status::ready) {
        std::cout << "Pizza arrived on time\n";
    }
    else {
        std::cout << "Pizza is late\n";
    }

    std::string pizza{f.get()};
    std::cout << "Pizza arrived: " << pizza << std::endl;
    // f.get();

    t.join();
}

int main()
{
    order1();
}

/*
When a std::promise and its corresponding std::future are created, they form a communication channel.
The caller (who holds the future) and the callee (who holds the promise) need a place to store the result.
But where is this result actually kept?

It can't be in either the std::promise or the std::future object itself.
    The std::promise is local to the callee and will be destroyed when the callee's function finishes. This could happen before the caller is ready to retrieve the result.
    The std::future is not a suitable container either.
    It can be moved to create a std::shared_future or even be destroyed while the result is still needed by other objects.
    Also, not all result types are copyable.

The Solution: The Shared State
    Since neither the caller nor the callee's local objects are suitable, the result is stored in an independent, third location: the shared state.
    The shared state is a thread-safe data structure that resides outside of both the std::promise and the std::future.

It's like a central, protected mailbox. The std::promise is the producer that writes the result to this mailbox, and the std::future is the consumer that reads from it.

This shared state exists on the heap and its lifetime is managed automatically.
It lives as long as at least one std::promise or std::future is still linked to it.
When the last of these objects is destroyed, the shared state is automatically deallocated.

Why the Shared State is Crucial
    Decoupling: It completely separates the std::promise from the std::future, allowing the callee to finish its work and die without affecting the caller's ability to retrieve the result later.
    Lifetime Management: It solves the lifetime problem for the result. The result stays in the shared state until the last object that needs it (the last future or shared_future) is destroyed.
    Resource and Exception Handling: It's the place where not just the result, but also any exceptions or status flags are stored. This is how exceptions thrown in the callee can be propagated to the caller.

The existence of the shared state is the fundamental concept that makes std::promise/std::future so powerful and reliable for asynchronous communication in C++.
*/

void producerTask(std::promise<int>&& p)
{
    std::cout << "Producer: Starting long computation\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    int res{42};

    std::cout << "Producer: Computation complete, setting the value: " << res << std::endl;
    p.set_value(42);
}

void consumerTask(std::shared_future<int> sharedFutureResult, int id)
{
    std::cout << "Consumer: " << id << " -> waiting for the result\n";
    int res{sharedFutureResult.get()};
    std::cout << "Consumer: " << id << " -> received res: " << res << std::endl;
}

void foo()
{
    std::promise<int> p;
    std::future<int> f{p.get_future()};
    std::shared_future<int> sf{f.share()};

    std::thread producerThread{producerTask, std::move(p)};

    std::vector<std::thread> threads;
    for (int i{}; i < 4; ++i) {
        threads.emplace_back(consumerTask, sf, i);
    }

    producerThread.join();
    for (auto& t : threads) t.join();

    std::cout << "All threads finished\n";
}

// int main()
// {
//     foo();
// }