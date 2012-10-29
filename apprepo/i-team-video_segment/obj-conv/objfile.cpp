/*
 *  objfile.cpp
 *  obj-conv
 *
 *  Created by Matthias Grundmann on 7/20/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "objfile.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <algorithm>
#include <functional>
#include <sstream>

using std::vector;

bool Point3D32f::operator==(float vertex[3]) const {
  return (x == vertex[0] && y == vertex[1] && z == vertex[2]);
}

VertNormal::VertNormal() {
  for (int i = 0; i < 3; ++i) 
    vertex[i] = normal[i] = 0.0f;
}

VertNormal::VertNormal(const Point3D32f& vert, const Point3D32f& norm) {
  vertex[0] = vert.x;
  vertex[1] = vert.y;
  vertex[2] = vert.z;
  
  normal[0] = norm.x;
  normal[1] = norm.y;
  normal[2] = norm.z;
}

ObjFile::ObjFile()
{
  
}

void ObjFile::LoadFromFile(std::string filename) {
	if (filename.empty() )
		return;

	vertices_.clear();
	normals_.clear();

	std::ifstream ifs (filename.c_str ());
	
	if (!ifs) {
		std::cout << "ERROR ObjFile::loadFromFile: "
              << "OBJ file does not exists or unable to open!\n";
		return;
	}

	Point3D32f p;
	std::string mode;
	std::string dummy;
	std::string face;
	Triangle tri;

	// parse Obj-File
	while (ifs) {
		ifs >> mode;
		
		if ( mode == "v" || mode == "vn" || mode == "vt") {
			ifs >> p.x >> p.y >> p.z;

			if ( mode == "vn" )
				normals_.push_back(p);
			else if ( mode == "vt" )
        uvCoords_.push_back(Point2D32f (p.x, p.y));
			else { // normal vertex
				vertices_.push_back (p);
			}
		}
		else if ( mode == "f" )
		{
      memset(&tri, 0, sizeof(Triangle));
			ifs >> face;
			int count = std::count (face.begin(), face.end(), '/');
			
			// assume triangles
			for ( int i =0; i < 3; ++i )
			{
				std::istringstream is(face);
        tri.mode = Triangle::VERTICES;

				if ( count == 0 ) {
					is >> tri.v[i];
					// convert to zero based index
					--tri.v[i];
				} else if (count == 1) {
					is >> tri.v[i];
					--tri.v[i];
					is.get ();
					is >> tri.t[i];
					--tri.t[i];
					tri.mode |= Triangle::TEXTURE;
				}	else {
					is >> tri.v[i];
					--tri.v[i];
					is.get ();
					char c = is.get ();
					if ( c != '/' ) {
						is.putback (c);
						is >> tri.t[i];
						--tri.t[i];
						is.get ();
						tri.mode |= Triangle::TEXTURE;
					}

					is >> tri.n[i];
					--tri.n[i];
					tri.mode |= Triangle::NORMALS;
				}	
				if (i < 2)
					ifs >> face;
			}
			triangles_.push_back (tri);
		}	
		// else or after suppress line
		std::getline ( ifs, dummy );
	}

	ifs.close();
  std::cout << "Loaded successfully " << filename << " with\n"
            << triangles_.size() << " triangles defined over " 
            << vertices_.size() << " vertices, " << normals_.size() << " normals "
            << uvCoords_.size() << " texture coordinates.\n";
}

void ObjFile::ConvertForDrawElements(vector<VertNormal>* verts, vector<int>* triangle_idx) const {
  const float max_float = std::numeric_limits<float>::max();
  Point3D32f unused_pt(max_float, max_float, max_float);
  
  verts->clear();
  verts->reserve(vertices_.size());
  triangle_idx->clear();
  triangle_idx->reserve(triangles_.size() * 3);
  
  int vertex_reallocations = 0;
  
  // Add all vertices_ to verts, assign unused_pt as normal.
  for (vector<Point3D32f>::const_iterator v = vertices_.begin(); v != vertices_.end(); ++v) {
    verts->push_back(VertNormal(*v, unused_pt));
  }
  
  for (vector<Triangle>::const_iterator t = triangles_.begin(); t != triangles_.end(); ++t) {
    for (int i = 0; i < 3; ++i) {
      // Check if normal index at vertex is already is used.
      float (&normal_ref)[3] = (*verts)[t->v[i]].normal;
      const Point3D32f& normal = normals_[t->n[i]];
      
      if (unused_pt == normal_ref) {
        normal_ref[0] = normal.x;
        normal_ref[1] = normal.y;
        normal_ref[2] = normal.z;
        triangle_idx->push_back(t->v[i]);
      } else if (normal == normal_ref) {
        triangle_idx->push_back(t->v[i]);
      } else {
        // We need to allocate a new position - vertex has multiple normals.
        
        verts->push_back(VertNormal(vertices_[t->v[i]], normals_[t->n[i]]));
        triangle_idx->push_back(verts->size() - 1);
        ++vertex_reallocations;
      }
    }
  }
  
  std::cout << "ObjFile::ConvertForDrawElements: Re-allocated " << vertex_reallocations 
            << " vertices, i.e. " << (float)vertex_reallocations / vertices_.size() * 100
            << "% of all vertices.\n";
}

