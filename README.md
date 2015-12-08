# mvcc11

A simple header-only Multiversion Concurrency Control (MVCC) implementation in C++11.

# What is MVCC?

Quote from [1024cores] (http://www.1024cores.net/home/lock-free-algorithms/reader-writer-problem/multi-version-concurrency-control), one of my favorite concurrency sites:

> Multi-Version Concurrency Control (MVCC) is a basic technique for elimination of starvation. MVCC allows several versions of an object to exist at the same time. That is, there are the "current" version and one or more previous versions. Readers acquire the current version and work with it as much as they want. During that a writer can create and publish a new version of an object, which becomes the current. Readers still work the previous version and can't block/starve writers. When readers end with an old version of an object, it goes away.


Synopsis
========

```C++
namespace mvcc11 {

template <class ValueType>
struct snapshot
{
  using value_type = ValueType;

  snapshot(size_t ver) noexcept;

  template <class U>
  snapshot(size_t ver, U&& arg)
    noexcept( noexcept(value_type{std::forward<U>(arg)}) );

  size_t version;
  value_type value;
};

template <class ValueType>
class mvcc
{
public:
  using value_type = ValueType;
  using snapshot_type = snapshot<value_type>;
  using const_snapshot_ptr = shared_ptr<snapshot_type const>;

  mvcc() noexcept;

  mvcc(value_type const &value);
  mvcc(value_type &&value);

  mvcc(mvcc const &other) noexcept;
  mvcc(mvcc &&other) noexcept;

  ~mvcc();

  mvcc& operator=(mvcc const &other) noexcept;
  mvcc& operator=(mvcc &&other) noexcept;

  const_snapshot_ptr current() noexcept;
  const_snapshot_ptr operator*() noexcept;
  const_snapshot_ptr operator->() noexcept;

  const_snapshot_ptr overwrite(value_type const &value);
  const_snapshot_ptr overwrite(value_type &&value);

  template <class Updater>
  const_snapshot_ptr update(Updater updater);

  template <class Updater>
  const_snapshot_ptr try_update(Updater updater);

  template <class Updater, class Clock, class Duration>
  const_snapshot_ptr try_update_until(
    Updater updater,
    std::chrono::time_point<Clock, Duration> const &timeout_time);

  template <class Updater, class Rep, class Period>
  const_snapshot_ptr try_update_for(
    Updater updater,
    std::chrono::duration<Rep, Period> const &timeout_duration);
};

} // namespace mvcc11
```

Note: The following assertions hold, assuming no new value was published concurrently (i.e. in another thread).

Initializing an mvcc object
--------

An `mvcc` object could be intialized with a user-specified initial value.

```C++
mvcc11::mvcc<ValueType> x{initial_value};
```

If `ValueType` is DefaultConstructible, `mvcc<ValueType>` is also DefaultConstructible.

```C++
mvcc11::mvcc<ValueType> y;
```


Snapshots
--------
In *mvcc11*, a snapshot represents the value of a versioned-object managed by a `mvcc<ValueType>` instance at a point in time. A snapshot is accessible through a `snapshot_ptr`, an alias of `shared_ptr` to `snapshot<ValueType> const`, obtained from a `mvcc<ValueType>` instance.

The data structrute `snapshot` consists of of two fields, `version` (of `size_t`) and `value` (of `ValueType`).

Calls to `x.current()` (or `*x`) yields up-to-date snapshots, and their versions start from 0.

```C++
auto inital_snapshot = x.current();
assert(inital_snapshot->version == 0);
assert(inital_snapshot->value == initial_value);
```

Publishing new versions
--------

### Overwriting

Overwriting an `mvcc<ValueType>` instance is straight forward.

```C++
auto snapshot1 = x.overwrite(overwritten_value);

assert(snapshot1 != inital_snapshot);
assert(snapshot1 == x.current());
assert(snapshot1->version == 1);
assert(snapshot1->value == overwritten_value);
```

It forces a newer snapshot to be published, regardless the state of the current snapshot of an `mvcc<ValueType>` instance.

Publishing via `overwrite()` is convenient. But it's oftentimes not what one needs. In many applications, new snapshots could only be computed using the lastest snapshot.

```C++
// Naive and unsafe
x.overwrite( compute(x.current()->value) );
```

Veterans in concurrecy should notice this is a race condition, there is no synchronization. By the time `x.overwrite()` is executed, newer snapshots could be available.

```C++
// Not scalable
{
  auto lock = make_lock(mutex_for_x);
  x.overwrite( compute(x.current()->value) );
}
```

One solution is to use lock. It's semantically correct, at the cost of blocking other concurrent access to the same `mvcc<ValueType>` instance. It potentially results in contention, especially when the computation is non-trivial.

This is where the `update()` and `try_update_xxx()` member functions comes to rescue.

### Updating

All update member functions require a user-specified updater. An updater is a callable object that is compatible to the signature `ValueType updater(size_t version, ValueType const &value)`.

```C++
auto updater = 
  [](size_t version, ValueType const &value)
  {
    return value + 1;
  };
  
auto updated_snapshot = x.try_update(updater);

if(updated_snapshot == nullptr)
{
  // update failed
}
else
{
  // update succeeded
}
```

`x.try_update()` does the following:

```C++
// Pseudo code
const_snapshot_ptr mvcc::try_update(Updater updater)
{
  auto expected = current_snapshot_;                // 1
  
  auto new_value = updater(expected->version, expected->value);

  auto desired =                                    // 2
    make_shared<snapshot>(
      expected->version + 1,
      new_value);

  auto updated =                                    // 3
    atomic_compare_exchange_strong(
      &current_snapshot_,
      &expected,
      desired);

  if(updated)
    return current_snapshot_;

  return nullptr;                                   // 4
}
```

1. Caches a copy of its current snapshot called `expected`
2. Calls user-specified updater with the version and value from `expected` and create a snapshot called `desired`
3. If no newer snapshot has been published since `expected`, publish *our* snapshot created from step 2, then returns true.
4. Otherwise, returns null

`x.update()` is a wrapper around `x.try_update():

```C++
// Pseudo code
const_snapshot_ptr mvcc::update(Updater updater)
{
  while(true)
  {
    auto updated = this->try_update(updater);        // 1
    if(updated != nullptr)                           // 2
      return updated;

    this_thread::sleep_for(chrono::milliseconds(50)) // 3
  }
}
```

1. Calls `try_update()`
2. Returns `updated` snapshot if the publication succeeded
3. Sleep a bit to avoid spining, go back to step 1

# Installing and using mvcc11

Though you do need a C++11 conforming compiler, *mvcc11* is header only, just drop it in your include path.

