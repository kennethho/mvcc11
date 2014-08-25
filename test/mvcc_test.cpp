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
//#define MVCC11_NO_PTHREAD_SPINLOCK 1

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MVCC_TEST
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

#include <mvcc11/mvcc.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cassert>

using namespace std;
using namespace chrono;

using namespace mvcc11;

namespace
{
  auto hr_now() -> decltype(high_resolution_clock::now())
  {
    return high_resolution_clock::now();
  }

  auto INIT = "init";
  auto OVERWRITTEN = "overwritten";
  auto UPDATED = "updated";
  auto DISTURBED = "disturbed";
}

template <class Mutex>
auto make_lock(Mutex& mtx) -> std::unique_lock<Mutex>
{
  return std::unique_lock<Mutex>{mtx};
}

template <class Mutex, class Computation>
auto locked(Mutex& mtx, Computation comp)
  -> decltype(comp())
{
  auto lock = make_lock(mtx);
  return comp();
}

#define LOCKED(mtx)                                                     \
  if(bool locked_done_eiN8Aegu = false)                                 \
    {}                                                                  \
  else                                                                  \
    for(auto mtx ## _lock_eiN8Aegu = make_lock(mtx);                    \
        !locked_done_eiN8Aegu;                                          \
        locked_done_eiN8Aegu = true)

#define LOCKED_LOCK(mtx) mtx ## _lock_eiN8Aegu

BOOST_AUTO_TEST_SUITE(MVCC_TESTS)

BOOST_AUTO_TEST_CASE(test_null_snapshot_on_1st_reference)
{
  mvcc<string> x;
  auto snapshot = *x;
  BOOST_REQUIRE(snapshot->version == 0);
  BOOST_REQUIRE(snapshot->value.empty() == true);
}

BOOST_AUTO_TEST_CASE(test_current_and_op_deref_yields_equivalent_results)
{
  mvcc<string> x{INIT};
  auto snapshot = x.current();

  BOOST_REQUIRE(snapshot == x.current());
  BOOST_REQUIRE(snapshot == *x);

  BOOST_REQUIRE(snapshot != nullptr);
  BOOST_REQUIRE(snapshot->version == 0);
  BOOST_REQUIRE(snapshot->value == INIT);
}

BOOST_AUTO_TEST_CASE(test_snapshot_overwrite)
{
  mvcc<string> x{INIT};
  auto snapshot = x.overwrite(OVERWRITTEN);
  BOOST_REQUIRE(snapshot != nullptr);
  BOOST_REQUIRE(snapshot->version == 1);
  BOOST_REQUIRE(snapshot->value == OVERWRITTEN);
}

BOOST_AUTO_TEST_CASE(test_snapshot_isolation)
{
  mvcc<string> x{INIT};
  auto snapshot1 = *x;
  BOOST_REQUIRE(snapshot1 != nullptr);
  BOOST_REQUIRE(snapshot1->version == 0);
  BOOST_REQUIRE(snapshot1->value == INIT);

  auto snapshot2 = x.overwrite(OVERWRITTEN);
  BOOST_REQUIRE(snapshot2 != nullptr);
  BOOST_REQUIRE(snapshot2->version == 1);
  BOOST_REQUIRE(snapshot2->value == OVERWRITTEN);

  BOOST_REQUIRE(snapshot1 != snapshot2);
  BOOST_REQUIRE(snapshot1 != nullptr);
  BOOST_REQUIRE(snapshot1->version == 0);
  BOOST_REQUIRE(snapshot1->value == INIT);
}

// Ensuring different instance contain different snapshots.
BOOST_AUTO_TEST_CASE(test_instance)
{
  auto x = mvcc<string>{"x"}.current();
  auto y = mvcc<string>{"y"}.overwrite("Y");

  BOOST_REQUIRE(x->version == 0);
  BOOST_REQUIRE(x->value == "x");

  BOOST_REQUIRE(y->version == 1);
  BOOST_REQUIRE(y->value == "Y");
}

BOOST_AUTO_TEST_CASE(test_snapshot_update)
{
  mvcc<string> x{INIT};
  auto init = *x;
  BOOST_REQUIRE(init->version == 0);
  auto updated = x.update([](size_t version, string const &value) {
      BOOST_REQUIRE(version == 0);
      BOOST_REQUIRE(value == INIT);
      return UPDATED;
  });
  BOOST_REQUIRE(updated->version == 1);
  BOOST_REQUIRE(updated->value == UPDATED);
  BOOST_REQUIRE(init->value == INIT);

  x.update([](size_t version, string const &value) {
      BOOST_REQUIRE(version == 1);
      BOOST_REQUIRE(value == UPDATED);
      return UPDATED;
  });
}

// Updater wakes up disturber when ready, so that disturber
// could sneakily change the most recent version of snapshot
// and cause try_update_once() to fail
BOOST_AUTO_TEST_CASE(test_try_update_once_fails_with_disturber)
{
  mutex mtx;
  bool updater_ready = false;
  bool disturber_ready = false;
  condition_variable cv;

  mvcc<string> x{INIT};
  BOOST_REQUIRE(x.current()->version == 0);
  auto updater = 
    async(launch::async,
          [&]
          {
            //assert(x.current()->version == 0);
            auto updated =
              x.try_update(
                [&](size_t const version, string const &value)
                {
                  //assert(version == 0);
                  //assert(value == INIT);
                  LOCKED(mtx)
                  {
                    updater_ready = true;
                    cv.notify_one();
                  };
                  LOCKED(mtx)
                  {
                    cv.wait(LOCKED_LOCK(mtx), [&]() {
                        return disturber_ready;
                      });
                  }
           
                  return UPDATED;
                });
       
            //assert(updated == nullptr);
            return updated;
          });

  // disturber
  LOCKED(mtx)
  {
    cv.wait(LOCKED_LOCK(mtx), [&]() {
        return updater_ready;
      });
    x.overwrite(DISTURBED);
    BOOST_REQUIRE(x.current()->version == 1);
    disturber_ready = true;
    cv.notify_one();
  }

  BOOST_REQUIRE(updater.get() == nullptr);
  BOOST_REQUIRE(x.current()->value == DISTURBED);
}

// Similar to `test_try_update_once_fails_with_disturber`, except
// using update() instead of try_update_once().
// Since update() loops until success, so its 2nd attempt will
// result in success.
BOOST_AUTO_TEST_CASE(test_update_succeeds_with_disturber)
{
  bool updater_ready = false;
  bool disturber_ready = false;
  mutex mtx;
  condition_variable cv;

  atomic<size_t> update_attempts{0};
  mvcc<string> x{INIT};
  BOOST_REQUIRE(x.current()->version == 0);
  auto updater = 
    async(launch::async,
          [&] {
            auto updated = x.update([&](size_t, string const &value) {
                ++update_attempts;
                if(update_attempts == 1)
                {
                  //assert(value == INIT);
                  //assert(x.current()->version == 0);
                  LOCKED(mtx)
                  {
                    updater_ready = true;
                    cv.notify_one();
                  }

                  LOCKED(mtx)
                  {
                    cv.wait(LOCKED_LOCK(mtx), [&]() {
                        return disturber_ready;
                      });
                  }
                }
                else
                {
                  //assert(x.current()->version == 1);
                  //assert(value == DISTURBED);
                }
              
                return UPDATED;
              });
            //assert(x.current()->version == 2);
            //assert(updated->value == UPDATED);
            return updated;
          });

  // disturber
  LOCKED(mtx)
  {
    BOOST_REQUIRE(x.current()->version == 0);
    cv.wait(LOCKED_LOCK(mtx), [&]() {
        return updater_ready;
      });
    x.overwrite(DISTURBED);
    BOOST_REQUIRE(x.current()->version == 1);
    disturber_ready = true;
    cv.notify_one();
  }

  BOOST_REQUIRE(updater.get() != nullptr);
  BOOST_REQUIRE(x.current()->version == 2);
  BOOST_REQUIRE(update_attempts == 2);
  BOOST_REQUIRE(x.current()->value == UPDATED);
}

// Similar to `test_update_succeeds_with_disturber`, except
// using try_update_for() instead of update().
BOOST_AUTO_TEST_CASE(test_try_update_for_succeeds_with_disturber)
{
  bool updater_ready = false;
  bool disturber_ready = false;
  mutex mtx;
  condition_variable cv;
  atomic<size_t> update_attempts{0};

  mvcc<string> x{INIT};

  auto updater = 
    async(launch::async,
          [&] {
            auto updated = x.try_update_for([&](size_t, string const &value) {
                ++update_attempts;
                if(update_attempts == 1)
                {
                  //assert(value == INIT);
                  //assert(x.current()->version == 0);
                  LOCKED(mtx)
                  {
                    updater_ready = true;
                    cv.notify_one();
                  }
                  LOCKED(mtx)
                  {
                    cv.wait(LOCKED_LOCK(mtx), [&]() {
                        return disturber_ready;
                      });
                  }
                }
                else
                {
                  //assert(x.current()->version != 0);
                  //assert(value == DISTURBED);
                }

                return UPDATED;
              },
              seconds(1));

            //assert(x.current()->version >= 2);
            //assert(updated->value == UPDATED);
            return updated;
          });

  // disturber
  LOCKED(mtx)
  {
    BOOST_REQUIRE(x.current()->version == 0);
    cv.wait(LOCKED_LOCK(mtx), [&]() {
        return updater_ready;
      });
    x.overwrite(DISTURBED);
    BOOST_REQUIRE(x.current()->version == 1);
    disturber_ready = true;
    cv.notify_one();
  }

  auto updated = updater.get();
  BOOST_REQUIRE(updated != nullptr);
  BOOST_REQUIRE(updated->version >= 2);
  BOOST_REQUIRE(updated->value == UPDATED);
  BOOST_REQUIRE(x.current() == updated);
  BOOST_REQUIRE(update_attempts == 2);
}

// try_update_for 1 second, while sleep for 100ms between
// udpate attempts so to give time for disturber to kick in
// and screw updater, and cause try_update_for to fail.
BOOST_AUTO_TEST_CASE(test_try_update_for_fails_with_disturber)
{
  bool updater_ready = false;
  bool disturber_ready = false;
  mutex mtx;
  condition_variable cv;
  atomic<size_t> update_attempts{0};

  mvcc<string> x{INIT};
  auto updater =
    async(launch::async,
          [&] {
            auto updated = x.try_update_for([&](size_t, const string &value) {
                ++update_attempts;
                if(update_attempts == 1)
                {
                  LOCKED(mtx)
                  {
                    updater_ready = true;
                    cv.notify_one();
                  }
                    
                  LOCKED(mtx)
                  {
                    cv.wait(LOCKED_LOCK(mtx), [&]() {
                        return disturber_ready;
                      });
                  }
                }

                this_thread::sleep_for(milliseconds(100));
                return UPDATED;
              },
              seconds(1));

            return updated;
          });

  // disturber
  LOCKED(mtx)
  {
    cv.wait(LOCKED_LOCK(mtx), [&]() {
        return updater_ready;
      });
    x.overwrite(DISTURBED);
    disturber_ready = true;
    cv.notify_one();
  }
  auto start = hr_now();
  do
  {
    x.overwrite(DISTURBED);
  } while(start + seconds(1) > hr_now());

  size_t const MINIMUM_NUMBER_OF_OVERWRITES = 100;
  auto updated = updater.get();
  BOOST_REQUIRE(updated == nullptr);
  auto snapshot = x.current();
  BOOST_REQUIRE(snapshot->version > MINIMUM_NUMBER_OF_OVERWRITES);
  BOOST_REQUIRE(snapshot->value == DISTURBED);
}


struct Base { Base(int n ) : n{n}{} int n; };
struct Derived : Base {
  Derived(int n) : Base{n} {}
};

BOOST_AUTO_TEST_CASE(test_conversion_matching_type)
{
  Base b{1};
  mvcc<Base> mb1{1};
  mvcc<Base> mb2{b};
  BOOST_REQUIRE(mb2.current()->value.n == 1);
  b = 2;
  mb2.overwrite(b);
  BOOST_REQUIRE(mb2.current()->value.n == 2);
  mb2.overwrite(3);
  BOOST_REQUIRE(mb2.current()->value.n == 3);
  mb2 = mb1;
  BOOST_REQUIRE(mb2.current()->value.n == 1);
  mb1.overwrite(4);
  BOOST_REQUIRE(mb1.current()->value.n == 4);
  BOOST_REQUIRE(mb2.current()->value.n == 1);
}

BOOST_AUTO_TEST_SUITE_END()
