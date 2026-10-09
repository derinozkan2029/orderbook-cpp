#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>

// Prices are stored as integer ticks, not doubles. Comparing floating-point
// prices for equality/ordering across many orders is a classic source of
// bugs (0.1 + 0.2 != 0.3), so real order books quote in ticks (e.g. cents,
// or a venue-defined minimum increment) and only convert to a display price
// at the edges.
using Price = int64_t;
using Quantity = int64_t;
using OrderId = int64_t;
using OwnerId = int64_t;  // which participant/trader submitted the order

enum class Side { Buy, Sell };

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;  // remaining, unfilled quantity
    OwnerId owner;
};

struct Trade {
    OrderId restingOrderId;   // the order that was already resting in the book
    OrderId incomingOrderId;  // the order that arrived and crossed the spread
    Price price;              // trades execute at the resting order's price
    Quantity quantity;
};

// A single-symbol limit order book with price-time priority matching.
//
// Price-time priority: at a given price level, the order that arrived first
// is filled first (time priority). Across price levels, the best price
// (highest bid / lowest ask) is matched first (price priority).
//
// Bids are kept in descending price order (best bid = highest price = first
// element), asks in ascending price order (best ask = lowest price = first
// element). Within a price level, orders are FIFO in a deque, so the front
// of the deque is always the oldest resting order at that price.
//
// Self-trade prevention (STP): an incoming order is never matched against a
// resting order with the same `owner`. This implementation uses "skip"
// semantics: a same-owner resting order is left in place (not canceled,
// not moved) and the incoming order looks past it for the next eligible
// resting order, at that price level or a worse-but-still-crossing one.
// The tradeoff: the skipped order keeps resting for other counterparties,
// but it loses strict FIFO priority for this particular incoming order
// (orders behind it in the queue can fill ahead of it). The alternative
// designs -- canceling the resting order, or canceling the incoming order,
// or canceling both -- are simpler to reason about but destroy resting
// liquidity or reject a legitimate order outright, which is why "skip" is
// the more common real-world default.
class OrderBook {
public:
    // Adds a new order to the book. It is matched immediately against
    // resting orders on the opposite side while prices cross; any
    // unfilled remainder rests in the book. Returns the trades generated.
    std::vector<Trade> addOrder(Order incoming);

    // Cancels a resting order by id. Returns true if it was found and
    // removed, false if no such resting order exists (e.g. already filled
    // or canceled).
    bool cancelOrder(OrderId id);

    // Best bid / ask price, for display or testing. Throws if that side
    // is empty.
    Price bestBid() const;
    Price bestAsk() const;

    bool bidsEmpty() const { return bids_.empty(); }
    bool asksEmpty() const { return asks_.empty(); }

private:
    // Descending comparator so the best (highest) bid sorts first.
    std::map<Price, std::deque<Order>, std::greater<Price>> bids_;
    // Default ascending order so the best (lowest) ask sorts first.
    std::map<Price, std::deque<Order>> asks_;

    // Where to find a resting order by id, so cancel doesn't have to scan
    // the whole book. Maps id -> (side, price), then we look up the deque
    // for that price level and scan within it (levels are usually small).
    std::unordered_map<OrderId, std::pair<Side, Price>> locate_;

    std::vector<Trade> match(Order& incoming);
    void rest(const Order& order);
};
