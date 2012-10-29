/*
 *  concurrent_queue.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

// concurrent_queue based on Anthony Williams,
// http://www.justsoftwaresolutions.co.uk/threading/implementing-a-thread-safe-queue-using-condition-variables.html


#ifndef CONCURRENT_QUEUE_H__
#define CONCURRENT_QUEUE_H__
#include <list>

#include <boost/thread.hpp>
#include <boost/thread/thread_time.hpp>

template<typename Data>
class concurrent_queue {
private:
  std::list<Data> the_queue;
  mutable boost::mutex the_mutex;
  boost::condition_variable data_available;
  boost::condition_variable data_consumed;
  
public:
  void push(const Data& data) {
    boost::mutex::scoped_lock lock(the_mutex);
    the_queue.push_back(data);
    lock.unlock();
    data_available.notify_one();
  }
  
  // Same as push, but locks caller until fewer than max_elems
  // have to be processed.
  void push(const Data& data, unsigned int max_elems) {
    boost::mutex::scoped_lock lock(the_mutex);
    while (the_queue.size() >= max_elems) {
      data_consumed.wait(lock);
    }
    
    the_queue.push_back(data);
    lock.unlock();
    data_available.notify_one();
  }
  
  template <class Predicate>
  bool contains(const Predicate& pred) const {
    boost::mutex::scoped_lock lock(the_mutex);
    typedef typename std::list<Data>::const_iterator const_iterator;
    for (const_iterator i = the_queue.begin(); i != the_queue.end(); ++i) {
      if (pred(*i))
        return true;
    }
    
    return false;
  }
  
  bool contains(const Data& data) const {
    return contains(std::bind2nd(std::equal_to<Data>(), data));
  }
  
  bool empty() const {
    boost::mutex::scoped_lock lock(the_mutex);
    return the_queue.empty();
  }
  
  int size() const {
    boost::mutex::scoped_lock lock(the_mutex);
    return the_queue.size();
  }
  
  bool try_pop(Data* popped_value) {
    boost::mutex::scoped_lock lock(the_mutex);
    if(the_queue.empty())
      return false;
    
    *popped_value = the_queue.front();
    the_queue.pop_front();
    
    // Notification for max_push operation.
    lock.unlock();
    data_consumed.notify_one();
    return true;
  }
  
  void wait_and_pop(Data* popped_value) {
    boost::mutex::scoped_lock lock(the_mutex);
    while(the_queue.empty())
      data_available.wait(lock);
    
    *popped_value = the_queue.front();
    the_queue.pop_front();
    
    // Notification for max_push operation.
    lock.unlock();
    data_consumed.notify_one();
  }
  
  // Same as above but returns after wait_duration.
  // Returns true if item was successfully popped.
  bool timed_wait_and_pop(Data* popped_value,
                          int wait_duration = 1000) {
    const boost::system_time timeout = boost::get_system_time() + 
        boost::posix_time::milliseconds(wait_duration);
    
    boost::mutex::scoped_lock lock(the_mutex);
    while(the_queue.empty()) {
      if(!data_available.timed_wait(lock, timeout))
        return false;
    }
    
    *popped_value = the_queue.front();
    the_queue.pop_front();
    
    // Notification for max_push operation.
    lock.unlock();
    data_consumed.notify_one();
    
    return true;
  }
};

#endif // CONCURRENT_QUEUE_H__