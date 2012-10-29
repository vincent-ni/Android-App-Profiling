/*
 *  generic_swap.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 3/9/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef GENERIC_SWAP_H__
#define GENERIC_SWAP_H__

#include <functional>
#include <string>
using std::string;

#include <vector>
using std::vector;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <fstream>

#include "concurrent_queue.h"
#include "concurrent_status_vector.h"

// Generic swapping framework.
// For each type of item, derive a specific class from
// - GenericHeader      // describes and identifies the item
// - GenericBody        // holds the data to be written to file
// - GenericJob         // specifies file prefix and new object creation method.

// Generic Header
/////////////////

class GenericHeader {  
public:
  virtual ~GenericHeader() {};
  virtual bool IsEqual(const GenericHeader&) const = 0;    
  // Identifies header - has to be unique for each item!
  virtual string ToString() const = 0;              
};

typedef shared_ptr<GenericHeader> GenericHeaderPtr;

// Generic Body
///////////////

class GenericBody {
public:
  GenericBody () {};
  virtual ~GenericBody() {};
  virtual void WriteToStream(std::ofstream& ofs) const = 0;
  virtual void ReadFromStream(std::ifstream& ifs) = 0;
};

typedef shared_ptr<GenericBody> GenericBodyPtr;

class GenericBodyCreator {
public:
  virtual ~GenericBodyCreator() {}
  virtual GenericBody* Create() const = 0;
};

template <class TheBody>
class StandardBodyCreator : public GenericBodyCreator {
public:
  GenericBody* Create() const { return new TheBody(); }
};

// Generic Item
///////////////
  
class GenericItem {
public:
  GenericItem(GenericHeaderPtr header, GenericBodyPtr body) : header_(header), body_(body) {}
  bool IsEqual(const GenericItem& item) const { return header_->IsEqual(*item.header_); }
  GenericHeaderPtr get_header() const { return header_; }
  GenericBodyPtr get_body() const { return body_; }
protected:
  GenericHeaderPtr header_;
  GenericBodyPtr body_;
};

typedef shared_ptr<GenericItem> GenericItemPtr;

// Generic Swap
///////////////

class GenericJob;
typedef shared_ptr<GenericJob> GenericJobPtr;

class GenericSwap : boost::noncopyable {
public:
  GenericSwap(const string& work_dir, int read_threads, int write_threads);
  ~GenericSwap();
  
  // Registers a new job.
  // Using a non-registered job will result in un-defined behavior.
  
  void RegisterJob(GenericJobPtr job);
  void RemoveJob(GenericJob* job);
  
  // Starts read and write threads.
  void Start(); 
  bool IsRunning() { return running_; }
private:
  
  struct ThreadSynchronization {
    ThreadSynchronization() : job_modification_pending(false), threads_waiting(0) {}
    
    boost::mutex job_mutex;
    bool job_modification_pending;
    int threads_waiting;
    boost::condition_variable job_modification_done;    
    boost::condition_variable thread_paused;
  };
  
  class WriteThread {
  public:
    WriteThread(const string& work_dir, int wt_id, vector<GenericJobPtr>* jobs,
                ThreadSynchronization* ts);
    void Run();
    void Stop() { stopped_ = true; }
    
  private:
    const string work_dir_;
    const int wt_id_;
    vector<GenericJobPtr>* jobs_;
    bool stopped_;
    ThreadSynchronization* ts_;
  };
  
  class ReadThread {
  public:
    ReadThread(const string& work_dir, int rt_id, vector<GenericJobPtr>* jobs,
               ThreadSynchronization* ts);
    void Run();
    void Stop() { stopped_ = true; }
  private:
    const string work_dir_;
    const int rt_id_;
    vector<GenericJobPtr>* jobs_;
    bool stopped_;
    ThreadSynchronization* ts_;
  };
  
private:
  bool running_;
  string work_dir_;
  int num_read_threads_;
  int num_write_threads_;
  boost::thread_group threads_;
  
  ThreadSynchronization ts_;
  
  vector<GenericJobPtr> jobs_;
  vector<WriteThread*> write_threads_;
  vector<ReadThread*> read_threads_;
  
   
  friend class GenericJob;
};

// Generic Job
//////////////

class GenericJob : boost::noncopyable {
public:  
  GenericJob(const string& file_prefix, shared_ptr<GenericBodyCreator> creator) 
      : file_prefix_(file_prefix), creator_(creator) {}
  
  // Puts item in the corresponding jobs queue. 
  void WriteItem(GenericItemPtr item);
  void WriteItem(GenericHeaderPtr header, GenericBodyPtr body);
  // Issues a read notice that triggers the corresponding body to be read
  // as soon as a read thread become available.
  // Note, that issueing the same read notice twice causes a logic error
  // except discard_duplicates is set to true.
  void IssueReadNotice(GenericHeaderPtr header);
  void ReadItem(GenericHeaderPtr header, GenericBodyPtr* output);
  
  const string GetPrefix() const { return file_prefix_; }
  
protected:
  const string file_prefix_;
  shared_ptr<GenericBodyCreator> creator_;

  concurrent_status_vector<GenericHeaderPtr> status_;
  concurrent_queue<GenericItemPtr> items_to_write_;
  concurrent_queue<GenericHeaderPtr> items_to_read_;
  concurrent_status_vector<GenericItemPtr> items_read_;
  
  friend class GenericSwap;
  friend class GenericSwap::ReadThread;
  friend class GenericSwap::WriteThread;
};

#endif  // GENERIC_SWAP_H__