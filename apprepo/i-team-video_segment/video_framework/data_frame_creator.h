/*
 *  data_frame_creator.h
 *  make-test
 *
 *  Created by Matthias Grundmann on 8/22/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <boost/unordered_map.hpp>
#include <boost/utility.hpp>
#include <string>

namespace VideoFramework {

  // Creates new DataFrame or any derived classes from string;
  // Although the singleton is implemented as a static member
  // it is only written during initialization before main is called.
  // Otherwise it is only used in a read-only fashion.

  class DataFrame;

  class DataFrameCreator : boost::noncopyable {
    typedef DataFrame* (*CreateFunPtr)(void);
  private:
    DataFrameCreator() {};
    DataFrameCreator(const DataFrameCreator& rhs) {};
  public:
    static DataFrameCreator* GetInstance();
    void RegisterFrameClass(const std::string& class_name, CreateFunPtr fun_ptr);
    DataFrame* CreateInstance(const std::string& class_name);

  private:
    boost::unordered_map<std::string, CreateFunPtr> ptrfun_map;
  };


#define DECLARE_DATA_FRAME(CLASS) \
public: \
  virtual std::string ClassName() { return #CLASS; } \
private: \
  friend class VideoFramework::DataFrameCreator; \
  static VideoFramework::DataFrame* CreateInstance() { return new CLASS(); } \
  \
  class CLASS##RegisterCreator { \
  public: \
    CLASS##RegisterCreator() { \
      VideoFramework::DataFrameCreator::GetInstance()->RegisterFrameClass(#CLASS, \
                                                                          CLASS::CreateInstance); \
    } \
  }; \
  static CLASS##RegisterCreator _the_creator;

#define DEFINE_DATA_FRAME(CLASS) \
  CLASS::CLASS##RegisterCreator CLASS::_the_creator;

}
