#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include "rpmbb/inspector.hpp"
#include "rpmbb/inspector/std.hpp"

TEST_CASE("Inspect std::array") {
    std::array<int, 3> arr = {1, 2, 3};
    CHECK(rpmbb::util::to_string(arr) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::deque") {
    std::deque<int> deq = {1, 2, 3};
    CHECK(rpmbb::util::to_string(deq) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::forward_list") {
    std::forward_list<int> flist = {1, 2, 3};
    CHECK(rpmbb::util::to_string(flist) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::list") {
    std::list<int> lst = {1, 2, 3};
    CHECK(rpmbb::util::to_string(lst) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::map") {
  std::map<std::string, int> mp{{"one", 1}, {"two", 2}, {"three", 3}};
  CHECK(rpmbb::util::to_string(mp) == "{one: 1, three: 3, two: 2}");
}

TEST_CASE("Inspect std::multimap") {
    std::multimap<std::string, int> mmap = {{"one", 1}, {"one", 2}, {"two", 2}};
    CHECK(rpmbb::util::to_string(mmap) == "{one: 1, one: 2, two: 2}");
}

TEST_CASE("Inspect std::set") {
    std::set<int> st = {1, 2, 3};
    CHECK(rpmbb::util::to_string(st) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::multiset") {
    std::multiset<int> mset = {1, 1, 2};
    CHECK(rpmbb::util::to_string(mset) == "[1, 1, 2]");
}

TEST_CASE("Inspect std::stack") {
    std::stack<int> stk;
    stk.push(1); stk.push(2); stk.push(3);
    CHECK(rpmbb::util::to_string(stk) == "[3, 2, 1]");
}

TEST_CASE("Inspect std::queue") {
    std::queue<int> que;
    que.push(1); que.push(2); que.push(3);
    CHECK(rpmbb::util::to_string(que) == "[1, 2, 3]");
}

TEST_CASE("Inspect std::priority_queue") {
    std::priority_queue<int> pq;
    pq.push(1); pq.push(3); pq.push(2);
    CHECK(rpmbb::util::to_string(pq) == "[3, 2, 1]");
}

TEST_CASE("Inspect std::unordered_map") {
    std::unordered_map<std::string, int> ump = {{"one", 1}, {"two", 2}, {"three", 3}};
    auto result = rpmbb::util::to_string(ump);
    CHECK(result.find("one: 1") != std::string::npos);
    CHECK(result.find("two: 2") != std::string::npos);
    CHECK(result.find("three: 3") != std::string::npos);
}

TEST_CASE("Inspect std::unordered_multimap") {
    std::unordered_multimap<std::string, int> ummap = {{"one", 1}, {"one", 2}, {"two", 2}};
    auto result = rpmbb::util::to_string(ummap);
    CHECK(result.find("one: 1") != std::string::npos);
    CHECK(result.find("one: 2") != std::string::npos);
    CHECK(result.find("two: 2") != std::string::npos);
}

TEST_CASE("Inspect std::unordered_set") {
    std::unordered_set<int> uset = {1, 2, 3};
    auto result = rpmbb::util::to_string(uset);
    CHECK(result.find("1") != std::string::npos);
    CHECK(result.find("2") != std::string::npos);
    CHECK(result.find("3") != std::string::npos);
}

TEST_CASE("Inspect std::unordered_multiset") {
    std::unordered_multiset<int> umset = {1, 1, 2};
    auto result = rpmbb::util::to_string(umset);
    CHECK(std::count(result.begin(), result.end(), '1') == 2);
    CHECK(result.find("2") != std::string::npos);
}
