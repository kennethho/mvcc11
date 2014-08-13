mvcc11
======

A simple header-only Multiversion Concurrency Control (MVCC) implementation in C++11.

What is MVCC?
=============

Quote from [1024cores] (http://www.1024cores.net/home/lock-free-algorithms/reader-writer-problem/multi-version-concurrency-control), one of my favorite concurrency sites:

> Multi-Version Concurrency Control (MVCC) is a basic technique for elimination of starvation. MVCC allows several versions of an object to exist at the same time. That is, there are the "current" version and one or more previous versions. Readers acquire the current version and work with it as much as they want. During that a writer can create and publish a new version of an object, which becomes the current. Readers still work the previous version and can't block/starve writers. When readers end with an old version of an object, it goes away.


Synopsis
========
```C++
mvcc11::mvcc<string> x{"initial value"};

// inital_snapshot is a shared_ptr<snapshot<string> const>
auto inital_snapshot = x.current();
assert(inital_snapshot->version == 0);
cout << inital_snapshot->value << endl; // prints "initial value"

auto overwritten = x.overwrite("overwritten");
assert(overwritten->version == 1);
assert(overwritten->value == "overwritten");

auto updated = x.update(
  [](size_t version, string const &value) {
    assert(version == 1);
    assert(value == "overwritten");
    return "updated";
  });

assert(udpated->version == 2);
assert(udpated->value == "updated");
```
