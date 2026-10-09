cmake_minimum_required(VERSION 3.10)
project(orderbook_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(orderbook src/OrderBook.cpp)

add_executable(orderbook_demo src/main.cpp)
target_link_libraries(orderbook_demo PRIVATE orderbook)

add_executable(orderbook_tests tests/test_order_book.cpp)
target_link_libraries(orderbook_tests PRIVATE orderbook)

enable_testing()
add_test(NAME order_book_tests COMMAND orderbook_tests)
