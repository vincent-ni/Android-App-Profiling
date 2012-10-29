/*
 *  frame_set_swap.cpp
 *  make-test
 *
 *  Created by Matthias Grundmann on 8/20/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "frame_set_swap.h"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>

using VideoFramework::DataFrame;
using VideoFramework::DataFrameCreator;

bool FrameSetHeader::IsEqual(const GenericHeader& rhs) const {
  const FrameSetHeader& header = dynamic_cast<const FrameSetHeader&>(rhs);
  return frame_id_ == header.frame_id_;
}

std::string FrameSetHeader::ToString() const {
  return boost::lexical_cast<string>(frame_id_);
}

void FrameSetBody::WriteToStream(std::ofstream& ofs) const {
  // Serialze each frame in the FrameSet.
  boost::archive::binary_oarchive oa(ofs);
  int sz = frame_set_ptr_->size();
  oa << sz;
  for (FrameSet::const_iterator f = frame_set_ptr_->begin(); f != frame_set_ptr_->end(); ++f) {
    // Serialize class name first.
    std::string class_name = (*f)->ClassName();
    oa << class_name;
    
    (*f)->SaveToBinary(oa);
  }
}

void FrameSetBody::ReadFromStream(std::ifstream& ifs) {
  boost::archive::binary_iarchive ia(ifs);
  int sz;
  ia >> sz;
  frame_set_ptr_.reset(new FrameSet(sz));
  for (int i = 0; i < sz; ++i) {
    string class_name;
    ia >> class_name;
    DataFrame* df = DataFrameCreator::GetInstance()->CreateInstance(class_name);
    df->LoadFromBinary(ia);
    frame_set_ptr_->at(i).reset(df);
  }
}
