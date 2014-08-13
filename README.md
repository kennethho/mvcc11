mvcc11
======

A simple header-only Multiversion Concurrency Control (MVCC) implementation in C++11.

What is MVCC?
=============

Quote from [1024cores] (http://www.1024cores.net/home/lock-free-algorithms/reader-writer-problem/multi-version-concurrency-control), one of my favorite concurrency sites:

> Multi-Version Concurrency Control (MVCC) is a basic technique for elimination of starvation. MVCC allows several versions of an object to exist at the same time. That is, there are the "current" version and one or more previous versions. Readers acquire the current version and work with it as much as they want. During that a writer can create and publish a new version of an object, which becomes the current. Readers still work the previous version and can't block/starve writers. When readers end with an old version of an object, it goes away.


Synopsis
========

Note: The following assertions holds, assuming no new value was published concurrently (i.e. in another thread).

```C++
mvcc11::mvcc<string> x{"initial value"};

// snapshots are instances of std::shared_ptr<mvcc11::snapshot<string> const>
auto inital_snapshot = x.current();
assert(inital_snapshot->version == 0);
assert(inital_snapshot->value == "initial value");

auto snapshot1 = x.overwrite("overwritten");
assert(snapshot1 != inital_snapshot);
assert(snapshot1 == x.current());
assert(snapshot1->version == 1);
assert(snapshot1->value == "overwritten");

auto snapshot2 = x.update(
  [](size_t version, string const &value) {
    assert(version == 1);
    assert(value == "overwritten");
    return "updated";
  });
assert(snapshot2 != snapshot1);
assert(snapshot2 == x.current());
assert(snapshot2->version == 2);
assert(snapshot2->value == "updated");
```

In addition to `update()`, `mvcc` class template has failable update functions, `try_update()`, `try_update_until()` and `try_update_for()`. When they fail, they return a null `shared_ptr`.

```C++
mvcc11::mvcc<int> x{0};

auto not_used = std::async(
  std::launch::async,
  [&x] { // async task
    x.overwrite(1);
  });

auto snapshot = x.try_update(
  [](size_t version, int value) { // updater
    return value + 1;
  });
```
The snapshot above may be null, if the async task and updater happened to run concurrently.
