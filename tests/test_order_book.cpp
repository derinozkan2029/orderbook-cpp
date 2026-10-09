// Plain assert-based tests, no external framework. Run via `make test`
// (see CMakeLists.txt) or by compiling and running this file directly.
#include <cassert>
#include <iostream>

#include "../src/OrderBook.h"

// Each order below uses owner == id unless a test is specifically about
// self-trade prevention, so no match is ever accidentally skipped.

static void testRestingOrdersDoNotMatchWhenPricesDoNotCross() {
    OrderBook book;
    assert(book.addOrder({1, Side::Sell, 101, 10, 1}).empty());
    assert(book.addOrder({2, Side::Buy, 99, 8, 2}).empty());
    assert(book.bestBid() == 99);
    assert(book.bestAsk() == 101);
}

static void testIncomingOrderMatchesAcrossMultiplePriceLevelsFIFO() {
    OrderBook book;
    book.addOrder({1, Side::Sell, 101, 10, 1});
    book.addOrder({2, Side::Sell, 102, 5, 2});

    // Incoming buy at 102 for 12 should fill order 1 completely (10 @ 101,
    // the better/lower price first) then order 2 partially (2 @ 102),
    // leaving order 2 resting with 3 shares.
    std::vector<Trade> trades = book.addOrder({3, Side::Buy, 102, 12, 3});

    assert(trades.size() == 2);
    assert(trades[0].restingOrderId == 1);
    assert(trades[0].price == 101);
    assert(trades[0].quantity == 10);
    assert(trades[1].restingOrderId == 2);
    assert(trades[1].price == 102);
    assert(trades[1].quantity == 2);

    assert(book.asksEmpty() == false);
    assert(book.bestAsk() == 102);  // order 2's remaining 3 shares
    assert(book.bidsEmpty());       // incoming order 3 fully filled, nothing rests
}

static void testTimePriorityWithinAPriceLevel() {
    OrderBook book;
    // Two sell orders at the same price; the first one in should be the
    // first one filled (time priority).
    book.addOrder({1, Side::Sell, 100, 5, 1});
    book.addOrder({2, Side::Sell, 100, 5, 2});

    std::vector<Trade> trades = book.addOrder({3, Side::Buy, 100, 5, 3});
    assert(trades.size() == 1);
    assert(trades[0].restingOrderId == 1);
}

static void testCancelRemovesAnOrderAndFreesItsPriceLevel() {
    OrderBook book;
    book.addOrder({1, Side::Buy, 99, 8, 1});
    book.addOrder({2, Side::Buy, 98, 12, 2});

    assert(book.cancelOrder(1) == true);
    assert(book.cancelOrder(1) == false);  // already canceled
    assert(book.cancelOrder(999) == false);  // never existed

    assert(book.bestBid() == 98);
}

static void testSelfTradePreventionSkipsOwnOrderAndFillsNextEligibleLevel() {
    OrderBook book;
    // Same owner (42) rests the best price; a different owner (7) rests a
    // worse price behind it.
    book.addOrder({1, Side::Sell, 100, 5, 42});
    book.addOrder({2, Side::Sell, 101, 5, 7});

    // Owner 42's incoming buy should skip its own order 1 (even though
    // it's the best price) and fill against order 2 instead.
    std::vector<Trade> trades = book.addOrder({3, Side::Buy, 101, 5, 42});

    assert(trades.size() == 1);
    assert(trades[0].restingOrderId == 2);
    assert(trades[0].price == 101);
    assert(trades[0].quantity == 5);

    // Order 1 was skipped, not removed: it's still resting, and it's still
    // the best (and only) ask.
    assert(book.bestAsk() == 100);
    assert(book.bidsEmpty());  // incoming order 3 fully filled
}

static void testSelfTradePreventionLeavesIncomingRestingWhenNoOtherCounterparty() {
    OrderBook book;
    book.addOrder({1, Side::Sell, 100, 5, 42});

    // The only crossing order belongs to the same owner, so no trade
    // should happen; the incoming order rests instead.
    std::vector<Trade> trades = book.addOrder({2, Side::Buy, 100, 5, 42});

    assert(trades.empty());
    assert(book.bestAsk() == 100);  // order 1, untouched
    assert(book.bestBid() == 100);  // order 2, now resting
}

int main() {
    testRestingOrdersDoNotMatchWhenPricesDoNotCross();
    testIncomingOrderMatchesAcrossMultiplePriceLevelsFIFO();
    testTimePriorityWithinAPriceLevel();
    testCancelRemovesAnOrderAndFreesItsPriceLevel();
    testSelfTradePreventionSkipsOwnOrderAndFillsNextEligibleLevel();
    testSelfTradePreventionLeavesIncomingRestingWhenNoOtherCounterparty();
    std::cout << "All tests passed.\n";
    return 0;
}
