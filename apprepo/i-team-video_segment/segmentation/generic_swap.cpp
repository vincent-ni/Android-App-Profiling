/*
 *  generic_swap.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 3/9/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "generic_swap.h"
#include <boost/utility.hpp>
#include <boost/bind.hpp>
#include <functional>
#include <iostream>

class GenericHeaderPtrComperator : 
public std::binary_function<GenericHeaderPtr, GenericHeaderPtr, bool> {
public:
  bool operator()(const GenericHeaderPtr lhs, const GenericHeaderPtr rhs) const {
    return lhs->IsEqual(*rhs);
  }
};

class GenericItemPtrComperator : 
public std::binary_function<GenericItemPtr, GenericItemPtr, bool> {
public:
  bool operator()(GenericItemPtr lhs, GenericItemPtr rhs) const {
    return lhs->IsEqual(*rhs);
  }
};

GenericSwap::GenericSwap(const string& work_dir, int read_threads, int write_threads) :
  running_(false), work_dir_(work_dir), num_read_threads_(read_threads), 
  num_write_threads_(write_threads) {
}

GenericSwap::~GenericSwap() {
  std::for_each(write_threads_.begin(), write_threads_.end(), std::mem_fun(&WriteThread::Stop));
  std::for_each(read_threads_.begin(), read_threads_.end(), std::mem_fun(&ReadThread::Stop));
  
  threads_.join_all();
  
  for (vector<WriteThread*>::iterator i = write_threads_.begin(); i != write_threads_.end(); ++i) {
    delete *i;
    *i = 0;
  }
  
  for (vector<ReadThread*>::iterator i = read_threads_.begin(); i != read_threads_.end(); ++i) {
    delete *i;
    *i = 0;
  }
}

void GenericSwap::RegisterJob(GenericJobPtr job) {
  // Signal to all threads that we will modify jobs_ if running.
  if (running_) {
    boost::mutex::scoped_lock lock(ts_.job_mutex);
    
    // If threads_waiting is > 0 some threads may have not woken up from previous job_modification.
    while (ts_.threads_waiting > 0) {
      ts_.thread_paused.wait(lock);
    }
    
    ts_.job_modification_pending = true;
    
 //   std::cerr << "Register Job: Waiting on threads!\n";
    
    while (ts_.threads_waiting < read_threads_.size() + write_threads_.size())
      ts_.thread_paused.wait(lock);
    
 //   std::cerr << "RegisterJob: All threads paused!\n";
    
    // Everybody is waiting here.
    jobs_.push_back(job);
    ts_.job_modification_pending = false;
    
    // Unlock and notify.
    lock.unlock();
    ts_.job_modification_done.notify_all();
  } else {
    jobs_.push_back(job);
  }
  
  usleep(50);
}

void GenericSwap::RemoveJob(GenericJob* job) {
  
  // Signal to all threads that we will modify jobs_ if running.
  boost::mutex::scoped_lock lock(ts_.job_mutex);
  
  while (ts_.threads_waiting > 0) {
    ts_.thread_paused.wait(lock);
  }  
  
  ts_.job_modification_pending = true;
  
 // std::cerr << "RemoveJob: Waiting on threads!\n";
  
  while (ts_.threads_waiting < read_threads_.size() + write_threads_.size())
    ts_.thread_paused.wait(lock);
  
 // std::cout << "RemoveJob: All threads paused!\n";
  
  // Everybody is waiting here.
  for (vector<GenericJobPtr>::iterator i = jobs_.begin(); i != jobs_.end(); ++i) {
    if (i->get() == job) {
      jobs_.erase(i);
      break;
    }
  }
  
  ts_.job_modification_pending = false;
  
  // Unlock and notify.
  lock.unlock();
  ts_.job_modification_done.notify_all();
  
  usleep(50);
}

void GenericSwap::Start() {
  if (running_)
    return;
  
  // Initialize ThreadSynchronization.
  ts_.job_modification_pending = false;
  ts_.threads_waiting = 0;
  
  // Create read and write threads.
  for (int i = 0; i < num_read_threads_; ++i) {
    ReadThread* rt = new ReadThread(work_dir_, i, &jobs_, &ts_);
    read_threads_.push_back(rt);
    threads_.create_thread(boost::bind(&GenericSwap::ReadThread::Run, rt));
  }
  
  for (int i = 0; i < num_write_threads_; ++i) {
    WriteThread* wt = new WriteThread(work_dir_, i, &jobs_, &ts_);
    write_threads_.push_back(wt);
    threads_.create_thread(boost::bind(&GenericSwap::WriteThread::Run, wt));
  }
  
  running_ = true;
}

void GenericJob::WriteItem(GenericItemPtr item) {
  // Stall after issueing n queued writes.
  items_to_write_.push(item, 20);
}

void GenericJob::WriteItem(GenericHeaderPtr header, GenericBodyPtr body) {
  GenericItemPtr item(new GenericItem(header, body));
  WriteItem(item);
}

void GenericJob::IssueReadNotice(GenericHeaderPtr header) { 
  if (items_to_read_.contains(std::bind2nd(GenericHeaderPtrComperator(), header))) {
    std::cerr << "GenericJob::IssueReadNotice: Issued two identical read notices. Aborting!\n";
    exit(1);
  }
  
  // Stall after issueing n read notices.
  items_to_read_.push(header, 20);
}

void GenericJob::ReadItem(GenericHeaderPtr header, GenericBodyPtr* output) {
  GenericItemPtr new_item (new GenericItem(header, GenericBodyPtr()));
  items_read_.wait_for_and_remove(&new_item, 
     std::bind2nd(GenericItemPtrComperator(), new_item));
  
  *output = new_item->get_body();
}

GenericSwap::WriteThread::WriteThread(const string& work_dir, 
                                    int wt_id, 
                                    vector<GenericJobPtr>* jobs,
                                    ThreadSynchronization* ts) :
    work_dir_(work_dir), wt_id_(wt_id), jobs_(jobs), stopped_(false), ts_(ts) {
  
}

void GenericSwap::WriteThread::Run() {
  std::cerr << "wt start\n";
  while(!stopped_) {
    
    // Is there a modification of jobs_ pending?
    boost::mutex::scoped_lock lock(ts_->job_mutex);
    if (ts_->job_modification_pending) {
      ++ts_->threads_waiting;
      ts_->thread_paused.notify_one();
      
      // Wait until modification is resolved.
      while (ts_->job_modification_pending)
        ts_->job_modification_done.wait(lock);

      --ts_->threads_waiting;
      ts_->thread_paused.notify_one();
    }
    lock.unlock();
    
    // Poll all jobs in round robin fashion
    // Use cascading thread arrangement.
    const int job_sz = jobs_->size();
    for (int j = wt_id_ % job_sz, i = j;
         j < wt_id_ % job_sz + job_sz;
         ++j, i = (i+1) % job_sz) {
      GenericItemPtr item_ptr;
      // Something to write?
      if ((*jobs_)[i]->items_to_write_.timed_wait_and_pop(&item_ptr, 100)) {
        // Consistency check.
        if ((*jobs_)[i]->status_.contains(
                std::bind2nd(GenericHeaderPtrComperator(), item_ptr->get_header()))) {
          std::cerr << "GenericSwap::WriteThread logic error: Data already swapped out."
                    << " Aborting.\n";
          exit(1);
        }
        
        // Write to file.
        string file = work_dir_ + "/" + (*jobs_)[i]->GetPrefix() + item_ptr->get_header()->ToString();
        
        std::ofstream ofs(file.c_str(), std::ios_base::out | std::ios_base::trunc |
                          std::ios_base::binary);
        
        if (!ofs) {
          std::cerr << "GenericSwap::WriteThread: Could not open " << file << " for writing.";
          exit(1);
        }
        
        item_ptr->get_body()->WriteToStream(ofs);
        
        // Update status.
        (*jobs_)[i]->status_.push_back(item_ptr->get_header());
      }
      if (stopped_)
        break;
    }

    usleep(500);
  }
  std::cerr << "wt stop\n";
}

GenericSwap::ReadThread::ReadThread(const string& work_dir, 
                                    int rt_id, 
                                    vector<GenericJobPtr>* jobs,
                                    ThreadSynchronization* ts) :
    work_dir_(work_dir), rt_id_(rt_id), jobs_(jobs), stopped_(false), ts_(ts) {
  
}

void GenericSwap::ReadThread::Run() {
  std::cerr << "rt start\n";
  while (!stopped_) {
    
    // Is there a modification of jobs_ pending?
    boost::mutex::scoped_lock lock(ts_->job_mutex);
    if (ts_->job_modification_pending) {
      ++ts_->threads_waiting;
      ts_->thread_paused.notify_one();
         
      // Wait until modification is resolved.
      while (ts_->job_modification_pending)
        ts_->job_modification_done.wait(lock);
      
      --ts_->threads_waiting;
      ts_->thread_paused.notify_one();
    }
    lock.unlock();    
    
    // Poll all jobs in round robin fashion
    // Use cascading thread arrangement.
    const int job_sz = jobs_->size();
    for (int j = rt_id_ % job_sz, i = j;
         j < rt_id_ % job_sz + job_sz;
         ++j, i = (i+1) % job_sz) {
      GenericHeaderPtr header;
      if ((*jobs_)[i]->items_to_read_.timed_wait_and_pop(&header, 100)) {
        
        string file = work_dir_ + "/" + (*jobs_)[i]->GetPrefix() + header->ToString();
        
        // Check status whether data is currently swapped out.
        // May be still about to be written - so wait until it is swapped out.
        if (!(*jobs_)[i]->status_.timed_wait_for_and_remove(&header, 5000,
                std::bind2nd(GenericHeaderPtrComperator(), header))) {
          std::cout << "GenericSwap::ReadThread: Waiting for " << file <<
                       " takes unusually long.\n";
          if (!(*jobs_)[i]->status_.timed_wait_for_and_remove(&header, 60000,
                  std::bind2nd(GenericHeaderPtrComperator(), header))) {
            std::cerr << "GenericSwap::ReadThread: Waiting for " << file <<
                         " timed out. Aborting!\n";
            exit(1);
          }
        }
        
        std::ifstream ifs(file.c_str(), std::ios_base::in | std::ios_base::binary);
        
        if (!ifs) {
          std::cerr << "GenericSwap::ReadThread: Could not open " << file << " for reading.";
          exit(1);
        }
        
        GenericBodyPtr body((*jobs_)[i]->creator_->Create());
        body->ReadFromStream(ifs);
        (*jobs_)[i]->items_read_.push_back(GenericItemPtr(new GenericItem(header, body)));
        ifs.close();
        
        // Remove from disk.
        std::remove(file.c_str());
      }
    }
    
    usleep(500);
  }
  std::cerr << "rt stop\n";
}