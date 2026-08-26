# Limit Order Book

<!-- TODO: replace with terminal GIF (book output -> trade -> benchmark table), e.g. recorded with ScreenToGif or asciinema+agg -->
```
--- BID ---
104 |  [ 104 ]
102 |  [ 104 ]
--- ASK ---
105 |  [ 102 ]

Trade @ Quantity 104 @ Price: 104
Trade @ Quantity 1 @ Price: 102
--- BID ---
102 |  [ 103 ]
--- ASK ---
105 |  [ 102 ]
```

A C++ limit order book with price time priority matching, the core component exchanges (and HFT firms) use to bring buy and sell orders together.

This project is under active development. It is a learning project, not a finished product; see "Current status" below for what is done and what is still open.

**Architecture:** producer thread (feed simulator) &rarr; lock free SPSC queue &rarr; consumer thread &rarr; `OrderBook` (`MemoryPool` + intrusive list per price level).

## Why this is interesting

In every electronic trading system (NASDAQ, NYSE, crypto exchanges), an order does not meet a counterparty immediately. It waits in a queue, sorted by price, and at the same price by arrival time ("price time priority"). The order book is the data structure that manages these queues and checks after every new order whether a trade can happen. Latency matters in nanoseconds here, so the choice of data structures matters as much as the correctness of the logic.

Replacing `std::map<Price, std::list<Order>>` with a pre allocated `MemoryPool` and intrusive linked lists makes `add_order` consistently 1.0x-1.5x faster on p50/mean latency (see [Benchmark](#benchmark)).

## Project structure

- `types.h`: base types (`OrderId`, `Price`, `Volume` as `uint64_t`; `Side`, `MessageType`)
- `order.h`: the `Order` struct (including `next_order`/`prev_order` for the intrusive list)
- `trade.h`: the `Trade` struct (result of a match)
- `memory_pool.h` / `memory_pool.cpp`: a pre allocated pool with a free list instead of a heap allocation per order
- `order_book.h` / `order_book.cpp`: the engine, `add_order`, `cancel_order`, `display`, matching with price time priority; price levels are intrusive linked lists whose nodes come from the `MemoryPool`
- `spsc_queue.h`: lock free single-producer/single-consumer ring buffer connecting the producer and matching threads
- `feed_simulator.h` / `feed_simulator.cpp`: generates random test orders
- `main.cpp`: producer thread (feed simulator, pushes into the `SPSCQueue`) and consumer thread (pops and drives the `OrderBook`)
- `benchmark/naive_order_book.h` / `.cpp`: same matching logic as `OrderBook`, but backed by plain `std::map<Price, std::list<Order>>` -- the baseline the benchmark below measures against
- `benchmark/benchmark.cpp`: p50/p90/p99/mean `add_order` latency, naive vs. pool
- `tests/test_matching.cpp`: test cases for insert/match/cancel (doctest)
- `tests/test_memory_pool.cpp`: test cases for the memory pool (allocation, reuse, growth, guard against `blockCount == 0`)
- `tests/test_spsc_queue.cpp`: FIFO order, full/empty, wraparound, and a two-thread stress test for the queue
- `tests/test_naive_order_book.cpp`: sanity tests for the naive baseline, so a bug there can't make the benchmark comparison meaningless

## Build & run

Requires `g++` (C++17) and `make`, for example via [MSYS2](https://www.msys2.org/) on Windows.

```
make          # builds app.exe and test.exe
./app.exe     # 10 simulated orders over two threads, book snapshot after each processed order
./test.exe    # test suite (doctest)
make bench    # builds bench.exe (-O2, see note below)
./bench.exe   # naive vs. pool add_order latency
make clean    # clean up
```

## Example output

`app.exe` runs a producer thread (generates orders, pushes them into the `SPSCQueue`) and a consumer thread (pops orders, feeds them to the `OrderBook`) side by side. The consumer prints the best 5 price levels per side after every processed order; if a match happens, the trade is printed live in between:

```
--- BID ---
102 |  [ 103 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]

Trade @ Quantity 102 @ Price: 102
--- BID ---
102 |  [ 1 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]
```

Each book line: `price | [remaining volume order 1] [remaining volume order 2] ...`, sorted by price time priority (best bid/ask first). The trade price is the price of the order that was already resting in the book (maker price); the newly arriving order may get price improvement. All trades are also stored in `trade_log` (for a later P&L calculation).

## Benchmark

`benchmark/benchmark.cpp` times `add_order` on two engines fed the exact same sequence of orders: `NaiveOrderBook` (`std::list<Order>`, one heap allocation per order) and `OrderBook` (`MemoryPool` + intrusive list). Orders are drawn from two disjoint, non-overlapping price ranges so nothing crosses and every order ends up resting -- the point is to measure a book that actually grows deep, not the thin, constantly-matching book `feed_simulator` produces for the live demo. Trade-log `std::cout` output is redirected away during the timed section so console I/O doesn't swamp the numbers being measured.

A single 200k-order run swings a lot between invocations on an ordinary desktop OS with no CPU pinning or real-time thread priority, so `bench.exe` runs 10 interleaved trials per engine (plus one discarded warm-up trial each) and pools every sample before computing percentiles. Typical numbers on the development machine (Windows, MinGW g++, `-O2`):

| engine | p50 | mean |
|---|---|---|
| naive (`std::map` + `std::list`) | ~300-400 ns | ~400-750 ns |
| pool + intrusive list | ~200-300 ns | ~330-600 ns |

The pool version is consistently faster on p50 and mean (typically 1.0x-1.5x), run after run. p99 does *not* consistently favor the pool in this benchmark, most likely because `MemoryPool::allocate()`/`deallocate()` each take a `std::mutex` lock for thread-safety that this single-threaded benchmark gets no benefit from -- pure overhead on every call. Run `make bench && ./bench.exe` yourself; the exact numbers depend on the machine.

## What I learned

- Cache behavior dominates: the intrusive list + pool win comes from locality (contiguous blocks, no per-order heap allocation), not from a smarter algorithm.
- Memory ordering on the SPSC queue is easy to get subtly wrong -- cached producer/consumer indices avoid a cross-core atomic load on every push/pop, but only if the acquire/release pairing is right.
- Measuring latency honestly is hard: a single run swings a lot on a desktop OS without CPU pinning or real-time priority, which is why the benchmark pools 10 interleaved trials instead of trusting one run.

## Current status

Both parts of [TODO.md](TODO.md) are done: the MVP (part 1), and the "receive side masterclass" (part 2) -- memory pool, intrusive list, benchmarking, and the lock free SPSC queue. The ITCH parser step was dropped as a scope cut partway through (see the note at the top of TODO.md); everything else in the original plan is complete.

Done:
- `MemoryPool` (allocation, reuse, growth, guard against `blockCount == 0`), tested independently
- `add_order`: pool allocation, placement new, linking to the end of the queue
- `cancel_order`: find the order via `order_map`, unlink it from the chain (4 cases: only element, head, tail, middle), destructor, `deallocate`
- `match()`: works on `head` and manual unlinking instead of `front()`/`pop_front()`
- `display()`, `order_volume()`: switched to pointer traversal
- `SPSCQueue<T, Capacity>`: lock free ring buffer, `alignas(64)`-separated producer/consumer state, cached indices to avoid a cross-core atomic load on every push/pop
- `benchmark/`: naive baseline plus the p50/p90/p99/mean comparison above

Builds cleanly with `make all bench`, no compiler warnings, all tests passing.

## License

MIT, see [LICENSE](LICENSE).
