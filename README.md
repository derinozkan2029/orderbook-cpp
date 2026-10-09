# orderbook-cpp

A single-symbol limit order book implementing price-time priority matching
in C++, built to practice the kind of low-level and performance-minded
code that comes up in trading systems. The goal of this project was for me to get used to C++ and learn more about trading.

## Why I built this

When applying to trading internships, I realized I need to explore the basics of trading more especially through C++ so that I could apply my knowledge in this language to a demo project. A limit order book is the standard project people build to show they
understand trading systems code, since it's the one piece of software
every exchange and trading venue actually runs, so I picked it specifically to close that gap rather than write any other C++ exercise.

## How I learned the concepts for this project

I didn't know price time priority or self trade prevention going in, so I
researched how real exchanges match orders before writing any code.
Price time priority is the standard matching rule at almost every venue, which is why the data structures follow directly from it. I chose a `std::map` keeps price levels sorted so the best price is always first,
and a `std::deque` per level preserves arrival order. Self trade prevention came from reading about why venues avoid letting a participant match against their own resting order. There are a few common ways to
handle it , and I chose "skip" because it's the one that doesn't destroy resting liquidity or reject a legitimate order. The
reasoning behind my choice, I wrote it in `OrderBook.h`, next to the matching logic it applies to.

## What it does

The core is `OrderBook`, which supports:

- **Adding a limit order.** It is matched immediately against resting orders on the opposite side of the book while prices cross; any unfilled remainder rests in the book as a new resting order.
- **Canceling a resting order** by id.
- **Querying the best bid/ask.**

Matching follows **price-time priority**: the best price is matched first, and within a price level, orders are filled in the order they arrived.

**Self-trade prevention (STP):** every order carries an `owner`, and an incoming order is never matched
against a resting order with the same owner. This implementation uses
skip semantics like a same owner resting order is left exactly where it is,
untouched, and the incoming order looks past it for the next eligible resting order, at that price level or a worse but still crossing one. 

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

I intentionally scoped this to the matching core like no market orders, no
multiple symbols, no networking/market-data feed, no throughput
benchmarking. Those are natural next steps if I decide to expand the project. The next steps would be to add market orders,
create a support for order modification instead of just canceling and resubmitting. 
