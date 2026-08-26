# Limit Order Book: Next Steps (as of 2026-07-01)

Status update (2026-08-13): **the project is done.** Part 1 (the MVP) and all of part 2 (memory pool, intrusive list, benchmarking, lock free SPSC queue) are complete. See [README.md](README.md) for the current state, including the benchmark results. The rest of this file is the original plan, kept as a record of the reasoning behind the steps -- it no longer describes open work.

Scope change (2026-08-13): the ITCH parser step (originally step 6) was dropped partway through. It didn't add enough to justify the time, and it would have been code the project's owner couldn't fully explain themselves. Everything else in part 2 below was completed as planned; only step numbering that referenced ITCH is now stale.

## Starting point

The core is solid: inserting, matching with price time priority, cancel, 12 green tests. What is missing: the project is not runnable yet and not at production quality. The plan has two parts: first **finish the MVP** (runnable and clean), then the **receive side masterclass** (the roughly 70h extension).

---

## PART 1: Actually finish the MVP

Get it runnable and clean first, before any new feature gets added. A project that does not start is worthless, no matter how good the engine is.

### Step 1: Make it runnable (about 5h)

**main.cpp**: connects the simulator and the order book:
```
create the order book
simulator generates orders
  each order through add_order
  after each add: display()
```

**Makefile**: so `make` builds everything:
```
build target for the app
build target for the tests
clean
```

Result: someone can clone the repo, type `make`, and see it run. Baseline requirement to be presentable.

### Step 2: Trade output (about 3h)

The most important functional gap. An order book that settles trades silently is like a calculator that never shows the result.

```
in match(): create a Trade object for every execution
struct Trade { Price price; Volume qty; OrderId buy_id, sell_id; }
  push into a trade log vector
  optionally print live: "TRADE: 100 @ $67.20"
```

Also needed later for the send side's P&L calculation, so build it properly now.

### Step 3: Cleanup (about 2h)

```
clean up header warnings
either use timestamp or comment it as "coming in phase 2"
make the README honest (the Makefile now really exists)
```

After that the MVP is **done and clean**: commit it and consider it complete.

---

## PART 2: Receive side masterclass (the roughly 70h)

Now the solid MVP becomes an impressive system, in this order.

### Step 4: Memory pool (about 15h)
Instead of `std::list` (allocates per node individually), a pre allocated pool with a free stack. First real performance jump, first benchmark number. Orders come out of the pool in about 10ns instead of about 1000ns.

### Step 5: Intrusive linked list (roughly in parallel with step 4)
`next_idx`/`prev_idx` directly inside the Order struct, interlocked with the pool. Cache locality: the orders sit contiguously in memory.

### Step 6: ITCH parser -- dropped
The wow feature. Real NASDAQ ITCH binary data instead of the simulator. This turns a university project into a "runs on real exchange data" project. Big endian byte swapping, message types A/D/E/X/U.

Dropped on 2026-08-13 as a scope cut (see the note at the top of this file).

### Step 7: Benchmarking -- done
Naive `std::map`/`std::list` version vs. pool version. Latency p50/p99 with `std::chrono`. This becomes the README table and the speedup factor.

Implemented in `benchmark/`; results and methodology are in [README.md](README.md#benchmark).

### Step 8: Lock free SPSC queue -- done
Producer thread (feed simulator) plus matching thread, with the lock free queue in between, `alignas(64)` against false sharing. The crowning piece of the receive side, production grade.

Implemented in [spsc_queue.h](spsc_queue.h), wired up in `main.cpp`, tested in `tests/test_spsc_queue.cpp` (including a two-thread stress test). Originally scoped for an ITCH-parser thread feeding the matching thread; since ITCH was dropped, the producer is the feed simulator instead.

---

## Order at a glance

```
1. main.cpp + Makefile          done
2. trade output/log             done
3. cleanup + README             done
                                 (MVP complete)
4. memory pool                  done
5. intrusive list               done
6. ITCH parser                  dropped (scope cut, 2026-08-13)
7. benchmarking                 done -- README table
8. lock free SPSC queue         done -- crowning piece
                                 (receive side complete, project done)
```

---

## Most important next step (historical, part 1 is done)

**main.cpp + Makefile.** Not the memory pool, not ITCH: get it runnable first. Write it yourself (MVP principle), ask conceptual questions if needed instead of copying code.

---

## Open points from the project report (mapped)

| Open point | Solved in |
|---|---|
| No build system / main.cpp | Step 1 |
| No trade output | Step 2 |
| Header warnings | Step 3 |
| timestamp unused | Step 3 (documented), later the pool |
| Market orders / execute path | after the MVP, optionally before step 4 |
| display() without L2 aggregation | optional, alongside benchmarking/polish |
