# C++ Concurrency Sandbox 🚀
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](https://abc.github.io/repository-name/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A professional-grade collection of modern C++ concurrency patterns, synchronization primitives, and advanced task schedulers. This project serves as a technical portfolio for high-performance systems engineering.

---

## 📚 Documentation
The full API reference, including collaboration diagrams and call graphs, is automatically generated via Doxygen and hosted on GitHub Pages:
👉 **[View Documentation](https://akshaykhoje.github.io/threads/)**

---

## 🛠 Project Structure
The repository is organized into distinct modules, each demonstrating a specific concurrency challenge:

* **`Bounded-Blocking-Queue/`**: Implementation of a thread-safe blocking queue for Producer-Consumer synchronization.
* **`Readers-Writers/`**: Patterns for shared resource management using both `std::shared_mutex` and custom implementations with **Writer Preference** to prevent starvation.
* **`ThreadPools/`**: 
    * *Basic*: Fire-and-forget worker pool.
    * *Priority*: Importance-based task execution.
    * *Aging*: Advanced scheduler that dynamically boosts task priority to ensure fairness.
    * *Message-Passing*: Decoupled "Inbox" model for asynchronous communication.
* **`Request-Response/`**: Sophisticated pools using `std::future` and `std::packaged_task` for async result retrieval.

---

## 🚀 Highlights
### 1. Task Aging Algorithm
To solve the **Priority Starvation** problem common in standard priority queues, I implemented a background monitor thread that "ages" waiting tasks, ensuring that low-priority jobs eventually jump the line.

### 2. CI/CD Pipeline
This project features a fully automated documentation workflow. Every push to `main` triggers a **GitHub Action** that:
1. Installs Doxygen and Graphviz.
2. Regenerates the HTML documentation.
3. Deploys the updated site to GitHub Pages.

---

## 💻 Getting Started

### Prerequisites
- A C++17 compatible compiler (GCC/Clang)
- Doxygen (optional, for local doc generation)
- Graphviz (optional, for diagrams)

### Building the Docs Locally
```bash
doxygen Doxyfile
# Open docs/html/index.html in your browser
```
