# orderbook-cpp

A single-symbol limit order book implementing price-time priority matching
in modern C++, built to practice the kind of low-level, performance-minded
code that comes up in trading systems.

## What it does

The core is `OrderBook`, which supports:

- **Adding a limit order.** It is matched immediately against resting
  orders on the opposite side of the book while prices cross; any unfilled
  remainder rests in the book as a new resting order.
- **Canceling a resting order** by id.
- **Querying the best bid/ask.**

Matching follows **price-time priority**: the best price is matched first
(highest bid, lowest ask), and within a price level, orders are filled in
the order they arrived (FIFO).

**Self-trade prevention (STP):** every order carries an `owner` (the
participant who submitted it), and an incoming order is never matched
against a resting order with the same owner. This implementation uses
"skip" semantics: a same-owner resting order is left exactly where it is,
untouched, and the incoming order looks past it for the next eligible
resting order, at that price level or a worse-but-still-crossing one. See
`OrderBook.h` for why "skip" was chosen over canceling the resting order,
the incoming order, or both.

## Design decisions

- **Prices are integer ticks (`int64_t`), not `double`.** Comparing
  floating-point prices for equality/ordering across many orders is a
  classic correctness bug (`0.1 + 0.2 != 0.3`); real venues quote in ticks
  and convert to a display price only at the edges.
- **`std::map<Price, std::deque<Order>>` for each side.** A `std::map`
  keeps price levels sorted automatically (ascending for asks, descending
  for bids, via `std::greater`), so the best price is always `begin()`.
  Each level is a `std::deque` so new orders at that price append to the
  back and the oldest order is always `front()`, giving FIFO time
  priority within a level in O(1) per operation. Add/cancel/match are
  O(log n) in the number of distinct price levels.
- **`unordered_map<OrderId, (Side, Price)>` for cancel.** Without it,
  canceling an order would mean scanning every price level. With it,
  cancel is a hash lookup to find the level, then a linear scan within
  that one (usually small) level.

## Example

```
$ ./orderbook_demo
Resting two sell orders and two buy orders, none of which cross:
Best bid: 99, best ask: 101

Incoming BUY order 5, price 102, qty 12 (crosses the spread):
  TRADE: resting order 1 x incoming order 5 @ 101 for 10 shares
  TRADE: resting order 2 x incoming order 5 @ 102 for 2 shares
(order 1 fills completely at 101, order 2 fills partially at 102,
 leaving order 2 resting with 3 shares left)

Canceling resting buy order 4:
  canceled: true

Final best bid: 99, best ask: 102
```

## Build and run

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./orderbook_demo
./orderbook_tests
```

## Tests

`tests/test_order_book.cpp` is a small set of plain `assert`-based checks
(no external framework) covering: non-crossing orders resting without a
trade, matching across multiple price levels in price order, FIFO time
priority within a single price level, cancel correctness (including
canceling an already-canceled or nonexistent order), and self-trade
prevention (skipping a same-owner order at the best price and matching
the next eligible one, and leaving an incoming order fully resting when
the only crossing order shares its owner).

## What's not here

This is intentionally scoped to the matching core: no market orders, no
multiple symbols, no networking/market-data feed, no throughput
benchmarking. Those are natural next steps if the project grows.
