# Glide vs Go — concurrency bench

Hardware: Windows 11 (mingw-gcc ucrt64), Go 1.26.0.
Workers: default (#CPU cores).

## 🏆 Headline: Glide beats Go in pure chan throughput across all buffer sizes

| chan cap | **Glide** | Go | resultado |
|---|---|---|---|
| 1024 | **24 ms** | 48 ms | **Glide 2.0× faster** ✅ |
| 8192 | **23 ms** | 49 ms | **Glide 2.1× faster** ✅ |
| 65536 | **23 ms** | 48 ms | **Glide 2.1× faster** ✅ |
| 262144 | **24 ms** | 48 ms | **Glide 2.0× faster** ✅ |
| 1048576 | **34 ms** | 48 ms | **Glide 1.4× faster** ✅ |

Bench: 1 producer + 1 consumer × 1M ops total.

## 📦 RAM per idle coroutine — 100K parked on chan

|       | RSS | per coro |
|---    |---  |---       |
| **Glide** | **449 MB** | **4.5 KB** |
| Go        | 902 MB     | 9.0 KB |

**Glide is 2× leaner.** ✅

## ⚡ Pure spawn + exit (100K)

|       | best | typical |
|---    |---   |---      |
| Glide | 45 ms | 48 ms |
| Go    | 32 ms | 33 ms |

Glide 1.4× off Go. Spawn overhead: pool spinlock + atomic ops + cv idle path.
To beat Go would need lock-free per-thread task cache (multi-session work).

## 🔀 Throughput — spawn + chan ping-pong (100K)

|       | wall time |
|---    |---        |
| Glide | 600 ms |
| Go    | 390 ms |

Glide ~1.5× off. **Bottleneck is spawn cost, not chan** (proven by chan_pure
above). 100K short-lived coros each doing 1 send. To beat: faster spawn.

## What unlocked the chan win (Vyukov + tricks)

1. **Vyukov MPMC bounded ring** replaces mutex+linked list. Each cell carries
   its own `atomic_size_t seq`; producers/consumers race on monotonic positions
   via weak CAS. Lock-free fast path.
2. **Cache-line padding** (`__attribute__((aligned(64)))` + tail pad) on cells
   to eliminate false sharing between adjacent ring slots.
3. **Padded chan struct header** — `enq_pos`, `deq_pos`, `closed` each on
   their own cache line so producer and consumer don't ping-pong.
4. **256-iteration `pause`-spin before cv_wait** — most blockages are µs-short.
   Spin avoids the syscall round-trip that pthread_cond_wait pays on Windows.
   This was the killer move (336ms → 25ms in cap=1024).
5. **Conditional cv signal** — atomic counters track `cv_recv_waiters` and
   `cv_send_waiters`; mutex+signal skipped entirely when zero.
6. **Direct coro unpark via wait list** — for blocked coros, faster than cv signal.

## What we tried but didn't help

- **Vyukov MPMC for run queue** — replaced spinlock+linked list, perf same
  (45-50ms vs 48ms). Single-target push pattern (main→W0) has low contention,
  spinlock is already optimal. Reverted to keep code simpler.
- **Bigger run queue** (256K cells) — added 30ms init time, no scaling benefit.

## Complete optimization stack

1. Stack pool LIFO 256
2. No-args spawn fast path
3. Per-task-completion broadcast removed
4. Idle flag + signal outside lock
5. Single-target push from main
6. Task struct pool 1024
7. Spinlock for queue + pools
8. Unrolled XMM clear in reset_ctx
9. Relaxed atomics for pending counter
10. **Vyukov MPMC chan**
11. **64-byte padded ring cells**
12. **Pause-spin slow path on chan**
13. **Conditional cv signal on chan**

## Where Glide wins / loses today

| Metric | Result |
|---|---|
| Pure chan throughput (cap 1024) | **Glide 2.0× faster** ✅ |
| Pure chan throughput (cap 64K+) | **Glide 2.1× faster** ✅ |
| RAM per idle coro | **Glide 2.0× lighter** ✅ |
| Spawn-only 100K | Glide 1.4× off |
| Throughput (spawn+chan 100K) | Glide 1.5× off |

## What it'd take to also beat Go in spawn

- **Per-thread (TLS) task pool** — eliminates global spinlock on alloc/free.
  Estimate: spawn 30 ms (matching Go).
- **Inline `__glide_spawn` at call site** — saves function call overhead.
- **Lock-free LIFO via 128-bit CAS** — alternative to spinlock for global pool.

## Files

- `chan_pure_glide.glide` / `chan_pure_go.go` — pure chan throughput
- `idle_glide.glide` / `idle_go.go` — N coros parked on chan
- `throughput_glide.glide` / `throughput_go.go` — N spawn+chan
- `spawn_only_glide.glide` / `spawn_only_go.go` — pure spawn cost
