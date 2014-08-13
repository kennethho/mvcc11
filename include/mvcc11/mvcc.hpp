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

#include <mvcc11/shared_ptr.hpp>

#include <utility>
#include <chrono>

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

  snapshot(size_t ver)
  : version{ver}
  , value{}
  {}

  template <class U>
  snapshot(size_t ver, U&& arg)
  : version{ver}
  , value{std::forward<U>(arg)}
  {}

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

  mvcc() MVCC11_NOEXCEPT(true)
  : mutable_current_{smart_ptr::make_shared<snapshot_type>(0)}
  {
  }
  template <class U>
  explicit mvcc(U &&value)
  : mutable_current_{smart_ptr::make_shared<snapshot_type>(0, std::forward<U>(value))}
  {
  }

  mvcc(mvcc const &other) MVCC11_NOEXCEPT(true) = default;

  ~mvcc() = default;

  mvcc& operator=(mvcc const &other) MVCC11_NOEXCEPT(true) = default;

  const_snapshot_ptr current() MVCC11_NOEXCEPT(true)
  {
    return smart_ptr::atomic_load(&mutable_current_);
  }

  const_snapshot_ptr operator*() MVCC11_NOEXCEPT(true)
  {
    return this->current();
  }
  const_snapshot_ptr operator->() MVCC11_NOEXCEPT(true)
  {
    return this->current();
  }

  template <class U>
  const_snapshot_ptr overwrite(U &&value) //__attribute__((no_sanitize_thread))
  {
    auto desired = smart_ptr::make_shared<snapshot_type>(
      0,
      std::forward<U>(value));

    while(true)
    {
      auto expected = smart_ptr::atomic_load(&mutable_current_);
      desired->version = expected->version + 1;

      if(smart_ptr::atomic_compare_exchange_strong(
        &mutable_current_,
        &expected,
        desired))
      {
        return this->current();
      }
    }
  }

  template <class Updater>
  const_snapshot_ptr update(Updater updater)
  {
    while(true)
    {
      auto updated = this->try_update_impl(updater);
      if(updated != nullptr)
        return updated;
    }
  }

  template <class Updater>
  const_snapshot_ptr try_update(Updater updater)
  {
    return this->try_update_impl(updater);
  }

  template <class Updater, class Clock, class Duration>
  const_snapshot_ptr try_update_until(
    Updater updater,
    std::chrono::time_point<Clock, Duration> const &timeout_time)
  {
    return this->try_update_until_impl(updater, timeout_time);
  }

  template <class Updater, class Rep, class Period>
  const_snapshot_ptr try_update_for(
    Updater updater,
    std::chrono::duration<Rep, Period> const &timeout_duration)
  {
    auto timeout_time = std::chrono::high_resolution_clock::now() + timeout_duration;
    return this->try_update_until_impl(updater, timeout_time);
  }

private:
  template <class Updater>
  const_snapshot_ptr try_update_impl(Updater &updater) //__attribute__((no_sanitize_thread))
  {
    auto expected = smart_ptr::atomic_load(&mutable_current_);
    auto const const_expected_version = expected->version;
    auto const &const_expected_value = expected->value;

    auto desired = smart_ptr::make_shared<snapshot_type>(
      const_expected_version + 1,
      updater(const_expected_version, const_expected_value));

    if(smart_ptr::atomic_compare_exchange_strong(
      &mutable_current_,
      &expected,
      desired))
    {
      return this->current();
    }

    return nullptr;
  }
  template <class Updater, class Clock, class Duration>
  const_snapshot_ptr try_update_until_impl(
    Updater &updater,
    std::chrono::time_point<Clock, Duration> const &timeout_time)
  {
    while(true)
    {
      auto updated = this->try_update_impl(updater);

      if(updated != nullptr)
        return updated;

      if(std::chrono::high_resolution_clock::now() > timeout_time)
	return nullptr;
    }
  }

  mutable_snapshot_ptr mutable_current_;
};

} // namespace mvcc11

#endif // MVCC11_MVCC_HPP
