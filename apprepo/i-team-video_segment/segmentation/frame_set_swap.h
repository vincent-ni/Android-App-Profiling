/*
 *  frame_set_swap.h
 *  make-test
 *
 *  Created by Matthias Grundmann on 8/20/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef FRAME_SET_SWAP_H__
#define FRAME_SET_SWAP_H__

#include "generic_swap.h"
#include "video_unit.h"
#include <fstream>

using VideoFramework::FrameSetPtr;
using VideoFramework::FrameSet;

// Indentified by frame number.
class FrameSetHeader : public GenericHeader {
public:
  FrameSetHeader(int frame_id) : frame_id_(frame_id) {}
  bool IsEqual(const GenericHeader& rhs) const;
  std::string ToString() const; 
protected:
  int frame_id_;
};

// Saves whole frameset to file.
class FrameSetBody : public GenericBody {
public:
  FrameSetBody(FrameSetPtr ptr = FrameSetPtr()) : frame_set_ptr_(ptr) {}
  void WriteToStream(std::ofstream& ofs) const;
  void ReadFromStream(std::ifstream& ifs);
  
  FrameSetPtr GetFrameSetPtr() const { return frame_set_ptr_; }
protected:
  FrameSetPtr frame_set_ptr_;
  
};

#endif // FRAME_SET_SWAP_H__