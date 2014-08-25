/*
  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
  Version 2, December 2004

  Copyright (C) 2014 Kenneth Ho <ken@fsfoundry.org>

  Everyone is permitted to copy and distribute verbatim or modified
  copies of this license document, and changing it is allowed as long
  as the name is changed.

  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
  TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

  0. You just DO WHAT THE FUCK YOU WANT TO.
*/
#ifndef MVCC11_MVCC_HPP
#define MVCC11_MVCC_HPP

#ifndef MVCC11_CONTENSION_BACKOFF_SLEEP_MS
#define MVCC11_CONTENSION_BACKOFF_SLEEP_MS 50
#endif // MVCC11_CONTENSION_BACKOFF_SLEEP_MS

// Optionally uses std::shared_ptr instead of boost::shared_ptr
#ifdef MVCC11_USES_STD_SHARED_PTR

#include <memory>

namespace mvcc11 {
namespace smart_ptr {

using std::shared_ptr;
using std::make_shared;
using std::atomic_load;
using std::atomic_store;
using std::atomic_compare_exchange_strong

} // namespace smart_ptr
} // namespace mvcc11

#else // MVCC11_USES_STD_SHARED_PTR

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace mvcc11 {
namespace smart_ptr {

using boost::shared_ptr;
using boost::make_shared;
using boost::atomic_load;
using boost::atomic_store;

template <class T>
bool atomic_compare_exchange_strong(shared_ptr<T> * p, shared_ptr<T> * v, shared_ptr<T> w)
{
  return boost::atomic_compare_exchange(p, v, w);
}

} // namespace smart_ptr
} // namespace mvcc11

#endif // MVCC11_USES_STD_SHARED_PTR

#include <utility>
#include <chrono>
#include <thread>

#ifdef MVCC11_DISABLE_NOEXCEPT
#define MVCC11_NOEXCEPT(COND)
#else
#define MVCC11_NOEXCEPT(COND) noexcept(COND)
#endif

namespace mvcc11 {

template <class ValueType>
struct snapshot
{
  using value_type = ValueType;

  snapshot(size_t ver) MVCC11_NOEXCEPT(true);

  template <class U>
  snapshot(size_t ver, U&& arg)
    MVCC11_NOEXCEPT( MVCC11_NOEXCEPT(value_type{std::forward<U>(arg)}) );

  size_t version;
  value_type value;
};

template <class ValueType>
class mvcc
{
public:
  using value_type = ValueType;
  using snapshot_type = snapshot<value_type>;
  using mutable_snapshot_ptr = smart_ptr::shared_ptr<snapshot_type>;
  using const_snapshot_ptr = smart_ptr::shared_ptr<snapshot_type const>;

  mvcc() MVCC11_NOEXCEPT(true);

  mvcc(value_type const &value);
  mvcc(value_type &&value);

  mvcc(mvcc const &other) MVCC11_NOEXCEPT(true);
  mvcc(mvcc &&other) MVCC11_NOEXCEPT(true);

  ~mvcc() = default;

  mvcc& operator=(mvcc const &other) MVCC11_NOEXCEPT(true);
  mvcc& operator=(mvcc &&other) MVCC11_NOEXCEPT(true);

  const_snapshot_ptr current() MVCC11_NOEXCEPT(true);
  const_snapshot_ptr operator*() MVCC11_NOEXCEPT(true);
  const_snapshot_ptr operator->() MVCC11_NOEXCEPT(true);

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

private:
  template <class U>
  const_snapshot_ptr overwrite_impl(U &&value);

  template <class Updater>
  const_snapshot_ptr try_update_impl(Updater &updater);

  template <class Updater, class Clock, class Duration>
  const_snapshot_ptr try_update_until_impl(
    Updater &updater,
    std::chrono::time_point<Clock, Duration> const &timeout_time);

  mutable_snapshot_ptr mutable_current_;
};

template <class ValueType>
snapshot<ValueType>::snapshot(size_t ver) MVCC11_NOEXCEPT(true)
: version{ver}
, value{}
{}

template <class ValueType>
template <class U>
snapshot<ValueType>::snapshot(size_t ver, U&& arg)
  MVCC11_NOEXCEPT( MVCC11_NOEXCEPT(value_type{std::forward<U>(arg)}) )
: version{ver}
, value{std::forward<U>(arg)}
{}


template <class ValueType>
mvcc<ValueType>::mvcc() MVCC11_NOEXCEPT(true)
: mutable_current_{smart_ptr::make_shared<snapshot_type>(0)}
{}
template <class ValueType>
mvcc<ValueType>::mvcc(value_type const &value)
: mutable_current_{smart_ptr::make_shared<snapshot_type>(0, value)}
{
}
template <class ValueType>
mvcc<ValueType>::mvcc(value_type &&value)
: mutable_current_{smart_ptr::make_shared<snapshot_type>(0, std::move(value))}
{
}
template <class ValueType>
mvcc<ValueType>::mvcc(mvcc const &other) MVCC11_NOEXCEPT(true)
: mutable_current_{smart_ptr::atomic_load(other)}
{
}
template <class ValueType>
mvcc<ValueType>::mvcc(mvcc &&other) MVCC11_NOEXCEPT(true)
: mutable_current_{smart_ptr::atomic_load(other)}
{
}

template <class ValueType>
auto mvcc<ValueType>::operator=(mvcc const &other) MVCC11_NOEXCEPT(true) -> mvcc &
{
  smart_ptr::atomic_store(&this->mutable_current_,
                          smart_ptr::atomic_load(&other.mutable_current_));

  return *this;
}
template <class ValueType>
auto mvcc<ValueType>::operator=(mvcc &&other) MVCC11_NOEXCEPT(true) -> mvcc &
{
  smart_ptr::atomic_store(&this->mutable_current_,
                          smart_ptr::atomic_load(&other.mutable_current_));

  return *this;
}

template <class ValueType>
auto mvcc<ValueType>::current() MVCC11_NOEXCEPT(true) -> const_snapshot_ptr
{
  return smart_ptr::atomic_load(&mutable_current_);
}
template <class ValueType>
auto mvcc<ValueType>::operator*() MVCC11_NOEXCEPT(true) -> const_snapshot_ptr
{
  return this->current();
}
template <class ValueType>
auto mvcc<ValueType>::operator->() MVCC11_NOEXCEPT(true) -> const_snapshot_ptr
{
  return this->current();
}

template <class ValueType>
auto mvcc<ValueType>::overwrite(value_type const &value) -> const_snapshot_ptr
{
  return this->overwrite_impl(value);
}
template <class ValueType>
auto mvcc<ValueType>::overwrite(value_type &&value) -> const_snapshot_ptr
{
  return this->overwrite_impl(std::move(value));
}

template <class ValueType>
template <class U>
auto mvcc<ValueType>::overwrite_impl(U &&value) -> const_snapshot_ptr
{
  auto desired =
    smart_ptr::make_shared<snapshot_type>(
      0,
      std::forward<U>(value));

  while(true)
  {
    auto expected = smart_ptr::atomic_load(&mutable_current_);
    desired->version = expected->version + 1;

    auto const overwritten =
      smart_ptr::atomic_compare_exchange_strong(
        &mutable_current_,
        &expected,
        desired);

    if(overwritten)
      return desired;
  }
}

template <class ValueType>
template <class Updater>
auto mvcc<ValueType>::update(Updater updater) -> const_snapshot_ptr
{
  while(true)
  {
    auto updated = this->try_update_impl(updater);
    if(updated != nullptr)
      return updated;

    std::this_thread::sleep_for(std::chrono::milliseconds(MVCC11_CONTENSION_BACKOFF_SLEEP_MS));
  }
}

template <class ValueType>
template <class Updater>
auto mvcc<ValueType>::try_update(Updater updater) -> const_snapshot_ptr
{
  return this->try_update_impl(updater);
}

template <class ValueType>
template <class Updater, class Clock, class Duration>
auto mvcc<ValueType>::try_update_until(
  Updater updater,
  std::chrono::time_point<Clock, Duration> const &timeout_time)
  -> const_snapshot_ptr
{
  return this->try_update_until_impl(updater, timeout_time);
}

template <class ValueType>
template <class Updater, class Rep, class Period>
auto mvcc<ValueType>::try_update_for(
  Updater updater,
  std::chrono::duration<Rep, Period> const &timeout_duration)
  -> const_snapshot_ptr
{
  auto timeout_time = std::chrono::high_resolution_clock::now() + timeout_duration;
  return this->try_update_until_impl(updater, timeout_time);
}


template <class ValueType>
template <class Updater>
auto mvcc<ValueType>::try_update_impl(Updater &updater) -> const_snapshot_ptr
{
  auto expected = smart_ptr::atomic_load(&mutable_current_);
  auto const const_expected_version = expected->version;
  auto const &const_expected_value = expected->value;

  auto desired =
    smart_ptr::make_shared<snapshot_type>(
      const_expected_version + 1,
      updater(const_expected_version, const_expected_value));

  auto const updated =
    smart_ptr::atomic_compare_exchange_strong(
      &mutable_current_,
      &expected,
      desired);

  if(updated)
    return desired;

  return nullptr;
}
template <class ValueType>
template <class Updater, class Clock, class Duration>
auto mvcc<ValueType>::try_update_until_impl(
  Updater &updater,
  std::chrono::time_point<Clock, Duration> const &timeout_time)
  -> const_snapshot_ptr
{
  while(true)
  {
    auto updated = this->try_update_impl(updater);

    if(updated != nullptr)
      return updated;

    if(std::chrono::high_resolution_clock::now() > timeout_time)
      return nullptr;

    std::this_thread::sleep_for(std::chrono::milliseconds(MVCC11_CONTENSION_BACKOFF_SLEEP_MS));
  }
}

} // namespace mvcc11

#endif // MVCC11_MVCC_HPP
