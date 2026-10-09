#include <iostream>

#include "OrderBook.h"

static const char* sideName(Side s) { return s == Side::Buy ? "BUY" : "SELL"; }

static void printTrade(const Trade& t) {
    std::cout << "  TRADE: resting order " << t.restingOrderId
              << " x incoming order " << t.incomingOrderId
              << " @ " << t.price << " for " << t.quantity << " shares\n";
}

int main() {
    OrderBook book;

    // Owners 1-5 below are each their own participant (owner == id), so
    // nothing in this first scenario triggers self-trade prevention --
    // that's demonstrated separately further down.
    std::cout << "Resting two sell orders and two buy orders, none of which cross:\n";
    for (const Trade& t : book.addOrder({1, Side::Sell, 101, 10, 1})) printTrade(t);
    for (const Trade& t : book.addOrder({2, Side::Sell, 102, 5, 2})) printTrade(t);
    for (const Trade& t : book.addOrder({3, Side::Buy, 99, 8, 3})) printTrade(t);
    for (const Trade& t : book.addOrder({4, Side::Buy, 98, 12, 4})) printTrade(t);

    std::cout << "Best bid: " << book.bestBid() << ", best ask: " << book.bestAsk() << "\n\n";

    std::cout << "Incoming BUY order 5, price 102, qty 12 (crosses the spread):\n";
    std::vector<Trade> trades = book.addOrder({5, Side::Buy, 102, 12, 5});
    for (const Trade& t : trades) printTrade(t);
    std::cout << "(order 1 fills completely at 101, order 2 fills partially at 102,\n"
              << " leaving order 2 resting with 3 shares left)\n\n";

    std::cout << "Canceling resting buy order 4:\n";
    bool canceled = book.cancelOrder(4);
    std::cout << "  canceled: " << (canceled ? "true" : "false") << "\n\n";

    std::cout << "Best bid: " << book.bestBid() << ", best ask: " << book.bestAsk() << "\n\n";

    std::cout << "--- Self-trade prevention (fresh order book) ---\n";
    std::cout << "Trader 100 rests a sell at 50 (the best price); trader 200 rests\n"
              << "a sell at 51 (a worse price). Trader 100 then sends an incoming\n"
              << "buy at 51 -- it should skip its own resting order at 50 (the best\n"
              << "price) rather than trade with itself, and fill against trader\n"
              << "200's order at 51 instead:\n";
    OrderBook stpBook;
    for (const Trade& t : stpBook.addOrder({6, Side::Sell, 50, 3, 100})) printTrade(t);
    for (const Trade& t : stpBook.addOrder({7, Side::Sell, 51, 4, 200})) printTrade(t);
    std::vector<Trade> stpTrades = stpBook.addOrder({8, Side::Buy, 51, 5, 100});
    for (const Trade& t : stpTrades) printTrade(t);
    std::cout << "(order 6, same owner as the incoming order, is skipped and stays\n"
              << " resting at 50; order 7 fills for 4 shares at 51; the remaining\n"
              << " 1 share of order 8 rests as a new buy at 51)\n";

    return 0;
}
