/*

"Have you ever wondered why your video game doesn't freeze when it's loading a new level?"
"Or how a web browser can download multiple images at once while letting you scroll the page?"


| **Process**                                                                                                     | **Thread**                                                                         |
| --------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| An independent unit of execution with its **own memory space** (virtual address space).                         | A unit of execution **within a process**, sharing the process’s memory space.      |
| Processes are **heavier** – creating or switching between them costs more.                                      | Threads are **lighter** – faster to create and switch, since they share resources. |
| Communication between processes requires **inter-process communication (IPC)** (pipes, sockets, shared memory). | Threads communicate via **shared memory** (since they live in the same process).   |
| Failure in one process usually **does not affect** another process.                                             | Failure in one thread (e.g., segmentation fault) can **crash the whole process**.  |
| Typically managed by the **OS kernel scheduler**.                                                               | Managed by the OS but tied to **software threads** within a process.               |


Process vs Thread analogy:


A process is like a house. It has its own address, a separate yard, and private rooms.
If one house burns down, the neighboring houses are safe.
Communication between houses requires specific methods, like sending a letter or talking on the phone (IPC).

A thread is like a person inside the house. All people share the same house (memory space), the same kitchen, living room, and plumbing. 
They can talk to each other directly and easily by shouting from room to room (shared memory).
However, if one person starts a fire, the whole house is in danger.


Thread definitions (based on Meyers snippet)
- Hardware threads: actual execution units on CPU cores. (e.g., 8-core CPU with 2 hardware threads/core = 16 hardware threads total).
- Software threads (OS threads): managed by the OS, scheduled onto hardware threads. You can have more software threads than hardware threads (some will be waiting/blocked).
- std::thread (C++ abstraction): a C++ object that acts as a handle to an OS thread. It can represent:
    * An active running function.
    * A “null” state (default constructed, moved-from, joined, or detached).

With more clarity:
- CPU Core: The physical engine that executes instructions.
- Hardware Thread (Logical Processor): The virtualized execution pipeline within a single core. Modern CPUs can often run more than one hardware thread per core (e.g., Intel*s Hyper-Threading).
- OS Thread (Software Thread): The abstract unit of execution that the OS scheduler sees. It's the OS's job to map these software threads onto the available hardware threads.
- std::thread: Your C++ handle to that OS thread, letting you manage it from your code.

1. Synchronization

Definition: Coordination between threads to safely access shared data.
Why we need it: Without synchronization, multiple threads accessing the same memory can cause data races, undefined behavior, or corrupted state.
Mechanisms in C++: std::mutex, std::lock_guard, std::unique_lock, std::condition_variable, std::atomic.
When to use:
    Multiple threads need to read/write the same shared data.
    You want predictable, safe execution in critical sections.
    Example: incrementing a shared counter, updating a shared resource.
Key trade-offs:
    Synchronized code may block threads, reducing concurrency.
    Incorrect locking can lead to deadlocks or priority inversion.

Synchronization primitives
| **Primitive**               | **Purpose / Why we have it**                                                                         |
| --------------------------- | ---------------------------------------------------------------------------------------------------- |
| `std::mutex`                | Protect shared data; prevent simultaneous access (mutual exclusion).                                 |
| `std::lock_guard`           | RAII wrapper for mutex; automatically locks/unlocks; avoids forgetting unlock and reduces deadlocks. |
| `std::unique_lock`          | Flexible lock: can lock/unlock multiple times, used with `condition_variable`.                       |
| `std::condition_variable`   | Wait for a condition in a safe way; notify one/all threads when condition changes.                   |
| `std::shared_mutex` (C++17) | Multiple readers or one writer; read-write locking pattern.                                          |
| `std::atomic<T>`            | Atomic operations without mutex; safe for simple shared state (flags, counters, pointers).           |
| `std::atomic_flag`          | Lightweight atomic boolean; simple synchronization.                                                  |


2. Asynchronous execution

Definition: Running code that may complete in the future without blocking the current thread.
Why we need it: Allows threads to continue executing while waiting for long-running operations, improving throughput and responsiveness.
Mechanisms in C++: std::async, std::future, std::promise, std::packaged_task, thread pools.
When to use:
    Long-running CPU-bound or I/O-bound tasks that shouldn’t block the main thread.
    Tasks that produce results needed later (futures/promises).
    Example: loading data from a file while the main thread updates a GUI.
Key trade-offs:
    Adds complexity in managing lifetimes and dependencies.
    May consume additional threads or resources if overused.

Asynchronous / value-passing primitives
| **Primitive**        | **Purpose / Why we have it**                                                                          |
| -------------------- | ----------------------------------------------------------------------------------------------------- |
| `std::promise`       | Send a value (or exception) from one thread to another.                                               |
| `std::future`        | Receive the value; blocks until ready.                                                                |
| `std::shared_future` | Multiple consumers can read the same future.                                                          |
| `std::packaged_task` | Wrap a callable to be executed later, automatically sets associated future.                           |
| `std::async`         | Run a function asynchronously; returns a future immediately; high-level alternative to `std::thread`. |


| **Aspect**         | **Synchronous / Synchronized**           | **Asynchronous**                                                 |
| ------------------ | ---------------------------------------- | ---------------------------------------------------------------- |
| **Safety**         | Guarantees safe access to shared state   | Must design carefully for thread safety; no automatic protection |
| **Responsiveness** | Blocking may freeze threads              | Non-blocking; threads can continue working                       |
| **Complexity**     | Simple to reason about in small scopes   | Higher complexity: futures, promises, callbacks                  |
| **Use-case**       | Protect shared memory, critical sections | Long-running tasks, I/O, pipelines, task scheduling              |


std::thread creation is inherently asynchronous
    - When you create a std::thread, the function you pass starts executing independently of the thread that created it.
    - If your thread does not access shared data, there is no synchronization, and each thread runs completely asynchronously.
    - Example: the first example we did with heavy_computation and sleep_for — each thread started immediately and ran concurrently, without blocking the main thread (except when join() is called later).

Key points:
    - Asynchronous by default: Threads start immediately (or very soon) and execute concurrently.
    - No implicit synchronization: Without mutexes, atomics, or futures, there is no protection for shared data.
    - Synchronization introduces “partial synchronization”: When you add mutexes, lock_guard, or condition variables, threads may block while waiting for resources — this is a combination of async execution + synchronization.
    - Analogy: Launching a thread is like sending a worker off to do a task while you continue working yourself. If the worker only works on independent tasks, you don’t need to wait. If they touch shared resources, you must coordinate (mutex/atomic/future).

So, in short: creating std::thread is an example of asynchronous programming, but shared data introduces the need for synchronization, which can partially block or serialize threads depending on how it’s done.

*/

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

std::mutex mx;
int sharedCounter{};

void heavyComputation(int id)
{
    // mx.lock();
    // std::cout << "Thread: " << id << " staring computation...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // ++sharedCounter;

    // std::cout << "Thread: " << id << " finished computation.\n";

    // std::cout << "Thread: " << id << ", counter: " << sharedCounter << std::endl;
    // mx.unlock();

    {
        std::lock_guard<std::mutex> lock{mx};
        ++sharedCounter;

        std::cout << "Thread: " << id << ", counter: " << sharedCounter << std::endl;
    }
}

void simpleThreadExample()
{
    const int numThreads{4};
    std::vector<std::thread> threads;

    for (int i{}; i < numThreads; ++i) {
        threads.emplace_back(heavyComputation, i);
    }

    for (auto& thread : threads) thread.join();
    std::cout << "All computations done.\n";
}

int main()
{
    simpleThreadExample();
}