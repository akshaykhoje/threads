# SYSTEM CODING ROUND

In a system coding round, the interviewer is looking for two things: **working code** and, just as importantly, your ability to **communicate** your **design decisions**.

Before proceeding, it is assumed that you already decent knowledge and challeneges of multi-threading, OS and concurrency.

## Problem Statement

Implement a Thread-Safe Queue

## Breaking down the interview (5 phases)

### 1. Clarify Requirements

*Determine the constraints (blocking vs. non-blocking, fixed capacity vs. unlimited).*

- Before we write a single line of logic, we need to agree on what this queue looks like to the outside world.

### 2. Define the Interface

*Sketch the class structure and method signatures.*

Discuss with the interviewer questions like the following:
- If we are designing a standard ThreadSafeQueue class, what are the two primary operations we must support to allow threads to add and remove tasks? Also, should our pop (remove) operation return immediately if the queue is empty, or should it wait?

- The interview suggests push() and pop() operations. Besides, it should be a blocking queue, meaning the if the queue is empty, the thread must pause and wait until the data becomes available.

### 3. Choose Synchronization Primitives

*Decide which threading tools (mutexes, condition_variable etc.) fit the job.*

To implement this behaviour in C++, we need to solve two distinct problems:

1. **Mutual Exclusion:** We need to ensure that if two threads try to push or pop at the exact same time, they don't corrupt the queue's internal memory.
2. **Signaling:** We need a mechanism for the consumer to say, "I'm going to sleep until there is data", and for the producer to say, "Hey! I just added data, wake up!". Note, this is not the OS signals like SIGSEGV.

#### Decision

We need both `std::mutex` for the lock(to ensure only one thread can access the internal queue memory at a time) and the `std::condition_variable` for signaling mechanism(allows consumer to sleep saving CPU resource, while the queue is empty and allows producer to wake them up when data arrives).


### 4. Implementation

Write the actual logic (RAW)

1. Create a class BoundedBlockingQueue and define it's private members and attributes
2. In the class, the `push(x)` and `pop()` methods must be public.
3. Say the `max_capacity` of the queue is `n`.
4. We also need an atomic flag `is_shutdown` to trigger a safe shutdown across all threads.
5. The `push(x)` must acquire lock before adding a value to the queue. Check if queue has some space to add new data. If yes, add it and set the `std::condition_variable1` for consumers, that data is available for consumption. Use `condition_variable1.notify_one()` for this.  The lock is release by RAII (Resource Acquisition Is Initialization) at the end of the method.
6. the `pop()` operation must acquire a lock before removing data. Once the data is removed, the `std::condition_variable2` for producers must be set so that producers know that data can be added for consumption. Use `condition_variable2.notify_one()` for this. The lock is release at the end of the method by RAII.
7. While pushing, check if the queue has space, if no, the thread must wait until the data is consumed by a consumer.
8. The sleeping consumers must be notified using the `std::condition_variable` as soon as the queue is non-empty
9. Then, we design a workerthread each for consumerTask and producerTask and create one thread each in the main function. Then, if joinable, join them and return.

### 5. Explain the Logic

*This logic proves that the Bounded Blockign Queue is correctly enforcing flow control between the threads.*

#### Interview Explanation

1. **The Ramp Up:** "Initially, the producer is still faster than the consumer (chrono times)", filling up the queue from size 0 to 5 (the `max_capacity`)"
2. **The Ceiling:** "Once the Size hits 5, we stop seeing consecutive pushes. The producer is now blocked.
3. **The Lockstep:** "From this point on, every Push is immediately preceded by a Pop. The Producer cannot add item #8 until the Consumer removes item #3. This shows the condition variable is successfully putting the Producer to sleep when the buffer is full and waking it up only when space is made."
4. **Graceful Shutdown:** "Finally, once the Producer finishes its 30 items, it triggers the shutdown. The Consumer drains the remaining items (checking is_shutdown but seeing the queue isn't empty yet), and eventually exits cleanly."

#### Scenario Analysis : Tampering with the variables

**Scenario A: The Current Setup (Fast Producer, Slow Consumer)**

1. **Settings:** Producer sleeps 25ms, Consumer sleeps 1000ms.
2. **Result:** The queue stays FULL (Size ~5).
3. **Bottleneck:** The Consumer.
4. **Why:** The Producer generates work much faster than it can be processed. The system's overall speed is limited by the Consumer. The Producer spends most of its life waiting on the condition variable.

**Scenario B: The Reverse (Slow Producer, Fast Consumer)**

1. **Settings:** Change Producer sleep to 1000ms, Consumer sleep to 25ms.
2. **Result:** The queue stays EMPTY (Size 0 or 1).
3. **Bottleneck:** The Producer.
4. **Why:** As soon as an item arrives, the starving Consumer grabs it. The Consumer spends most of its life waiting for data.
5. **Interview Insight:** This is typical for "event-driven" systems where workers are sitting idle waiting for user requests.

**Scenario C: Mismatched Task Counts**

*This is where the shutdown() logic is critical.*

**Case 1:** Producer has 30 items, Consumer wants 1000 (Your Code).

1. Outcome: Works perfectly.
2. Producer finishes 30, calls shutdown().
3. Consumer wakes up, sees shutdown is true, but buffer.size() > 0. It continues looping until the buffer is empty.
4. Once empty, pop returns false, and the Consumer exits.

**Case 2:** Producer has 100 items, Consumer wants only 10.

1. Outcome: Without your specific shutdown logic, this would Deadlock.
2. Consumer finishes 10 items, exits its loop, and the thread dies.
3. Producer fills the queue to 5.
4. Producer waits for space... but the Consumer is gone! The Producer sleeps forever. 💀
5. Fix: The Consumer also needs to call shutdown() when it leaves (which your code actually does! q->shutdown() is in the Consumer task too).
6. Result with your code: Consumer quits -> calls shutdown() -> Producer wakes up -> push returns false -> Producer exits. Safe!

