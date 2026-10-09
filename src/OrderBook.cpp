#include "OrderBook.h"

#include <algorithm>
#include <stdexcept>

// Matches `incoming` against one side of the book (whichever opposes it).
// `priceIsEligible` decides whether a given price level still crosses the
// incoming order's limit price. Returns the trades generated.
//
// Walks price levels best-to-worst. Within each level, walks orders
// oldest-to-newest, but skips (does not match, does not remove) any
// resting order owned by the same participant as the incoming order --
// see the self-trade prevention note on the class declaration. A level is
// only erased from the book once it is actually empty; a level that still
// contains skipped same-owner orders is left in the book and the price
// iterator simply advances to the next level.
template <typename Book, typename PriceEligible>
static std::vector<Trade> matchAgainst(
        Order& incoming, Book& book, PriceEligible priceIsEligible,
        std::unordered_map<OrderId, std::pair<Side, Price>>& locate) {
    std::vector<Trade> trades;

    auto levelIt = book.begin();
    while (incoming.quantity > 0 && levelIt != book.end() &&
           priceIsEligible(levelIt->first)) {
        std::deque<Order>& level = levelIt->second;

        auto orderIt = level.begin();
        while (incoming.quantity > 0 && orderIt != level.end()) {
            if (orderIt->owner == incoming.owner) {
                ++orderIt;  // self-trade prevention: skip, leave resting
                continue;
            }

            Order& resting = *orderIt;
            Quantity filled = std::min(incoming.quantity, resting.quantity);
            trades.push_back(Trade{resting.id, incoming.id, resting.price, filled});

            incoming.quantity -= filled;
            resting.quantity -= filled;

            if (resting.quantity == 0) {
                locate.erase(resting.id);
                orderIt = level.erase(orderIt);
            } else {
                ++orderIt;
            }
        }

        auto nextLevelIt = std::next(levelIt);
        if (level.empty()) {
            book.erase(levelIt);
        }
        levelIt = nextLevelIt;
    }

    return trades;
}

std::vector<Trade> OrderBook::match(Order& incoming) {
    if (incoming.side == Side::Buy) {
        // A buy crosses the spread as long as there's an ask at or below
        // the incoming order's limit price.
        return matchAgainst(
            incoming, asks_, [&](Price p) { return p <= incoming.price; }, locate_);
    }
    // A sell crosses the spread as long as there's a bid at or above the
    // incoming order's limit price.
    return matchAgainst(
        incoming, bids_, [&](Price p) { return p >= incoming.price; }, locate_);
}

void OrderBook::rest(const Order& order) {
    if (order.side == Side::Buy) {
        bids_[order.price].push_back(order);
    } else {
        asks_[order.price].push_back(order);
    }
    locate_[order.id] = {order.side, order.price};
}

std::vector<Trade> OrderBook::addOrder(Order incoming) {
    std::vector<Trade> trades = match(incoming);
    if (incoming.quantity > 0) {
        rest(incoming);
    }
    return trades;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = locate_.find(id);
    if (it == locate_.end()) {
        return false;
    }
    Side side = it->second.first;
    Price price = it->second.second;

    auto eraseFrom = [&](auto& book) {
        auto levelIt = book.find(price);
        std::deque<Order>& level = levelIt->second;
        auto orderIt = std::find_if(level.begin(), level.end(),
                                     [id](const Order& o) { return o.id == id; });
        level.erase(orderIt);
        if (level.empty()) {
            book.erase(levelIt);
        }
    };

    if (side == Side::Buy) {
        eraseFrom(bids_);
    } else {
        eraseFrom(asks_);
    }
    locate_.erase(it);
    return true;
}

Price OrderBook::bestBid() const {
    if (bids_.empty()) {
        throw std::runtime_error("bestBid() called on empty bid side");
    }
    return bids_.begin()->first;
}

Price OrderBook::bestAsk() const {
    if (asks_.empty()) {
        throw std::runtime_error("bestAsk() called on empty ask side");
    }
    return asks_.begin()->first;
}
