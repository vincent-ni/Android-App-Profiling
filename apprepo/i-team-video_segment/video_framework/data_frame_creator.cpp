/*
 *  data_frame_creator.cpp
 *  make-test
 *
 *  Created by Matthias Grundmann on 8/22/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "data_frame_creator.h"

using std::string;

namespace VideoFramework {
  DataFrameCreator* DataFrameCreator::GetInstance() {
    static DataFrameCreator the_creator;
    return &the_creator;
  }

  void DataFrameCreator::RegisterFrameClass(const string& class_name,
                                            DataFrameCreator::CreateFunPtr fun_ptr) {
    ptrfun_map[class_name] = fun_ptr;
  }

  DataFrame* DataFrameCreator::CreateInstance(const string& class_name) {
    CreateFunPtr fun_ptr = ptrfun_map[class_name];
    return fun_ptr();
  }
}
