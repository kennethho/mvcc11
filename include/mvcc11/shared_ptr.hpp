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
#ifndef MVCC11_SHARED_PTR_HPP
#define MVCC11_SHARED_PTR_HPP

// Optionally uses std::shared_ptr instead of boost::shared_ptr
#ifdef MVCC11_USES_STD_SHARED_PTR

#include <memory>

namespace mvcc11 {
namespace smart_ptr {

using std::shared_ptr;
using std::make_shared;
using std::atomic_load;
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

template <class T>
bool atomic_compare_exchange_strong(shared_ptr<T> * p, shared_ptr<T> * v, shared_ptr<T> w)
{
  return boost::atomic_compare_exchange(p, v, w);
}

} // namespace smart_ptr
} // namespace mvcc11

#endif // MVCC11_USES_STD_SHARED_PTR

#endif // MVCC11_SHARED_PTR_HPP
