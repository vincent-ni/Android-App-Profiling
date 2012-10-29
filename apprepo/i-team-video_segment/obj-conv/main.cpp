/*
 *  main.cpp
 *  obj-conv
 *
 *  Created by Matthias Grundmann on 7/20/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
using namespace std;

#include "objfile.h"

int main(int argc, char** argv) {

  if (argc < 2) {
    std::cout << "Error: Specify obj-file!\n";
    return 1;
  }
    
  string file(argv[1]);
  cout << "Converting obj-file " << file << "\n";
  if (file.find_last_of(".obj") < 0) {
    std::cerr << "File needs to have .obj filetype";
    return 1;
  }
  
  string output = file.substr(0, file.find_last_of(".obj")) + ".db";
  
  ObjFile obj_file;
  obj_file.LoadFromFile(file);
  
  vector<VertNormal> vert_normals;
  vector<int> triangle_idx;
  
  obj_file.ConvertForDrawElements(&vert_normals, &triangle_idx);
  
  // Write to file.
  std::ofstream ofs(output.c_str(), std::ios_base::out | std::ios_base::binary);
  
  int vert_sz = vert_normals.size();
  int triangle_sz = triangle_idx.size();
  ofs.write(reinterpret_cast<const char*>(&vert_sz), sizeof(vert_sz));
  ofs.write(reinterpret_cast<const char*>(&vert_normals[0]), sizeof(VertNormal) * vert_sz);
  
  ofs.write(reinterpret_cast<const char*>(&triangle_sz), sizeof(triangle_sz));
  ofs.write(reinterpret_cast<const char*>(&triangle_idx[0]), sizeof(int) * triangle_sz);

  ofs.close();
  
  return 0;
}
