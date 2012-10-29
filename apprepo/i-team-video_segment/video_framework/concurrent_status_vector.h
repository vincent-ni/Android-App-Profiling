/*
 *  concurrent_vector.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef CONCURRENT_VECTOR_H__
#define CONCURRENT_VECTOR_H__

#include <boost/thread.hpp>
#include <vector>
#include <functional>

template<typename Data>
class concurrent_status_vector {
private:
  std::vector<Data> the_vector;
  mutable boost::mutex the_mutex;
  boost::condition_variable status_updated;
  
public:
  void push_back(const Data& data) {
    boost::mutex::scoped_lock lock(the_mutex);
    the_vector.push_back(data);
    lock.unlock();
    status_updated.notify_one();
  }
  
  bool empty() const {
    boost::mutex::scoped_lock lock(the_mutex);
    return the_vector.empty();
  }
  
  int size() const {
    boost::mutex::scoped_lock lock(the_mutex);
    return the_vector.size();
  }
  
  template <class Predicate>
  bool contains(const Predicate& pred) const {
    boost::mutex::scoped_lock lock(the_mutex);
    typedef typename std::vector<Data>::const_iterator const_iterator;
    
    for (const_iterator i = the_vector.begin(); i != the_vector.end(); ++i) {
      if (pred(*i))
        return true;
    }
    
    return false;
  }  
  
  bool contains(const Data& data) const {
    return contains(std::bind2nd(std::equal_to<Data>(), data));
  }
   
  template <class Pred>
  void wait_for_and_remove(Data* data, Pred pred) {
    boost::mutex::scoped_lock lock(the_mutex);
    
    // As long as item is not in vector and has been successfully removed wait
    // on status_updated_ condition variable.
    typedef typename std::vector<Data>::iterator iterator;
    bool removed = false; 
    while (!removed) {
      for (iterator d = the_vector.begin(); d != the_vector.end(); ++d) {
        if (pred(*d)) {
          *data = *d;
          the_vector.erase(d);
          removed = true;
          break;
        }
      }
      
      if (!removed)
        status_updated.wait(lock);
    }
  }
  
  void wait_for_and_remove(Data* data) {
    wait_for_and_remove(data, std::bind2nd(std::equal_to<Data>(), *data));
  }
  
  template<class Pred>
  bool timed_wait_for_and_remove(Data* data, int wait_duration, Pred pred) {    
    const boost::system_time timeout = boost::get_system_time() +
        boost::posix_time::milliseconds(wait_duration);
    
    boost::mutex::scoped_lock lock(the_mutex);
    
    // As long as item is not in vector and has been successfully removed wait
    // on status_updated condition variable.
    typedef typename std::vector<Data>::iterator iterator; 
    while (true) {
      for (iterator d = the_vector.begin(); d != the_vector.end(); ++d) {
        if (pred(*d)) {
          *data = *d;
          the_vector.erase(d);
          return true;
        }
      }
      
      // Not found.
      if (!status_updated.timed_wait(lock, timeout))
        return false;
    }
  }
  
  bool timed_wait_for_and_remove(Data* data, int wait_duration) {
    return timed_wait_for_and_remove(data, wait_duration, std::bind2nd(
        std::equal_to<Data>(), *data));
  }
  
};

#endif // CONCURRENT_VECTOR_H__.
