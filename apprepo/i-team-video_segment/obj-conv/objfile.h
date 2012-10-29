/*
 *  objfile.h
 *  obj-conv
 *
 *  Created by Matthias Grundmann on 7/20/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */


#ifndef OBJFILE_H
#define OBJFILE_H

#include <string>
#include <vector>

class Triangle {
public:
	enum TriangleMode { VERTICES =0, NORMALS = 1, TEXTURE = 2};

	int v[3];	// index to vertices
	int n[3];	// index to normals
	int t[3];	// index to texture

	int mode;
};

class Point3D32f { 
public:
  Point3D32f(float fx=.0f, float fy=.0f, float fz=.0f ) : x(fx), y(fy), z(fz) {}
  void Set(float fx=.0f, float fy=.0f, float fz=.0f ) { x = fx, y = fy, z = fz; }
  // Exact comparision.
  bool operator==(float vertex[3]) const;
  float x, y, z;
};


struct VertNormal {
  VertNormal();
  VertNormal(const Point3D32f& vert, const Point3D32f& norm);
  float vertex[3];
  float normal[3];
};


class Point2D32f { 
public:
  Point2D32f(float fx=.0f, float fy=.0f) : x(fx), y(fy) {}
  void Set(float fx=.0f, float fy=.0f) { x = fx, y = fy; }
  float x, y;
};

// This is not a 100% compatible parser for OBJ files,
// but it should read files exported from Autodesk 3D Studio Max

class ObjFile {
public:
	ObjFile();
	void LoadFromFile(std::string filename);
  void ConvertForDrawElements(std::vector<VertNormal>* vert_normals,
                              std::vector<int>* triangle_idx) const;

protected:
	std::vector<Point3D32f>		normals_;
	std::vector<Point3D32f>		vertices_;
  std::vector<Point2D32f>   uvCoords_;
	std::vector<Triangle>		  triangles_;
};

#endif