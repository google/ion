//-----------------------------------------------------------------------------
// Product:     OpenCTM tools
// File:        obj.cpp
// Description: Implementation of the OBJ file format importer/exporter.
//-----------------------------------------------------------------------------
// Copyright (c) 2009-2010 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//     1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//     2. Altered source versions must be plainly marked as such, and must not
//     be misrepresented as being the original software.
//
//     3. This notice may not be removed or altered from any source
//     distribution.
//-----------------------------------------------------------------------------

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <set>
#include <string>
#include <sstream>
#include <vector>
#include "obj.h"
#include "common.h"

using namespace std;

class OBJFaceNode {
  public:
    OBJFaceNode()
    {
      v = vt = vn = 0;
    }

    void Set(int aIndex, int aValue)
    {
      if(aIndex == 0)
        v = aValue;
      else if(aIndex == 1)
        vt = aValue;
      else
        vn = aValue;
    }

    int v, vt, vn;
};

// OBJ file face description class (three triangle corners, with one vertex,
// texcoord and normal index each).
class OBJFace {
  public:
    OBJFace()
    {
    }

    // Contruct a face (one triangle) from an OBJ face description string
    OBJFace(const std::string aStr)
    {
      // Start by finding the first and last non-whitespace char (trim)
      size_t l = aStr.size();
      if (l == 0)
        throw runtime_error("Invalid face element (empty).");
      size_t pos = 0, strEnd = l - 1;
      while((pos < strEnd) && ((aStr[pos] == ' ') || (aStr[pos] == '\t')))
        ++ pos;
      while((strEnd > pos) && ((aStr[strEnd] == ' ') || (aStr[strEnd] == '\t')))
        -- strEnd;

      // Extract three face corners (one triangle)
      while((pos <= strEnd) && (aStr[pos] != ' ') && (aStr[pos] != '\t'))
      {
        // Extract three /-separated strings (v/vt/vn)
        std::string v_s[3];
        int j = 0;
        while((pos <= strEnd) && (aStr[pos] != ' ') && (aStr[pos] != '\t') && (j < 3))
        {
          if(aStr[pos] != '/')
            v_s[j] += aStr[pos];
          else
            ++ j;
          ++ pos;
        }

        // Skip whitespaces
        while((pos <= strEnd) && ((aStr[pos] == ' ') || (aStr[pos] == '\t')))
          ++ pos;

        // Convert the strings to integers
        mNodes.push_back(OBJFaceNode());
        OBJFaceNode &n = mNodes.back();
        for(int j = 0; j < 3; ++ j)
        {
          int value = 0;
          if(v_s[j].size() > 0)
          {
            istringstream ss(v_s[j]);
            ss >> value;
            if(value > 0)
              value --;
            else if(value < 0)
              throw runtime_error("Negative vertex references in OBJ files are not supported.");
            else
              throw runtime_error("Invalid index (zero) in OBJ file.");
          }
          n.Set(j, value);
        }
      }
    }

    vector<OBJFaceNode> mNodes;
};

struct OBJVertex {
  OBJVertex() : v(0), vt(0), vn(0), index(-1) {}
  OBJVertex(int v_in, int vt_in, int vn_in, int index_in)
      : v(v_in), vt(vt_in), vn(vn_in), index(index_in) {}
  bool operator<(const OBJVertex& other) const
  {
    return std::lexicographical_compare(&v, &index, &other.v, &other.index);
  }
  int v, vt, vn;
  int index;
};

// Parse a 2 x float string as a Vector2
static Vector2 ParseVector2(const std::string aString)
{
  Vector2 result;
  istringstream sstr(aString);
  sstr >> result.u;
  sstr >> result.v;
  return result;
}

// Parse a 3 x float string as a Vector3
static Vector3 ParseVector3(const std::string aString)
{
  Vector3 result;
  istringstream sstr(aString);
  sstr >> result.x;
  sstr >> result.y;
  sstr >> result.z;
  return result;
}

/// Import a mesh from an OBJ file.
void Import_OBJ(const char * aFileName, Mesh * aMesh)
{
  // Open the input file
  ifstream inFile(aFileName, ios_base::in);
  if(inFile.fail())
    throw runtime_error("Could not open input file.");

  Import_OBJ(inFile, aMesh);

  // Close the input file
  inFile.close();
}

void Import_OBJ(std::istream &inFile, Mesh * aMesh) {
  // Clear the mesh
  aMesh->Clear();

  // Mesh description - parsed from the OBJ file
  vector<Vector3> verticesArray;
  vector<Vector2> texCoordsArray;
  vector<Vector3> normalsArray;
  vector<OBJFace> faces;

  // Parse the file
  while(!inFile.eof())
  {
    // Read one line from the file (concatenate lines that end with "\")
    std::string line;
    getline(inFile, line);
    while((line.size() > 0) && (line[line.size() - 1] == '\\') && !inFile.eof())
    {
      std::string nextLine;
      getline(inFile, nextLine);
      line = line.substr(0, line.size() - 1) + std::string(" ") + nextLine;
    }

    // Parse the line, if it is non-empty
    if(line.size() >= 1)
    {
      if(line.substr(0, 2) == std::string("v "))
        verticesArray.push_back(ParseVector3(line.substr(2)));
      else if(line.substr(0, 3) == std::string("vt "))
        texCoordsArray.push_back(ParseVector2(line.substr(3)));
      else if(line.substr(0, 3) == std::string("vn "))
        normalsArray.push_back(ParseVector3(line.substr(3)));
      else if(line.substr(0, 2) == std::string("f "))
        faces.push_back(OBJFace(line.substr(2)));
    }
  }

  // Prepare vertices
  aMesh->mVertices.reserve(verticesArray.size());
  if(texCoordsArray.size() > 0)
    aMesh->mTexCoords.reserve(verticesArray.size());
  if(normalsArray.size() > 0)
    aMesh->mNormals.reserve(verticesArray.size());

  // Prepare indices
  int triCount = 0;
  for(vector<OBJFace>::const_iterator i = faces.begin(); i != faces.end(); ++i)
  {
    int nodeCount = (*i).mNodes.size();
    if(nodeCount >= 3)
      triCount += (nodeCount - 2);
  }
  aMesh->mIndices.resize(triCount * 3);

  // Iterate faces and extract vertex data
  unsigned int idx = 0;
  // Store vertices uniquely.
  std::set<OBJVertex> vertexMap;
  for(vector<OBJFace>::iterator i = faces.begin(); i != faces.end(); ++i)
  {
    OBJFace &f = (*i);
    int nodeCount = 0;
    OBJVertex nodes[3];
    for(vector<OBJFaceNode>::iterator n = f.mNodes.begin(); n != f.mNodes.end();
        ++n)
    {
      // Collect polygon nodes for this face, turning it into triangles
      OBJVertex v((*n).v, (*n).vt, (*n).vn, vertexMap.size());
      std::set<OBJVertex>::const_iterator it = vertexMap.find(v);
      if(it == vertexMap.end())
      {
        // Create a new vertex
        vertexMap.insert(v);

        // Add the vertex data
        if (static_cast<size_t>(v.v) >= verticesArray.size())
          throw runtime_error("Invalid vertex index.");
        aMesh->mVertices.push_back(verticesArray[v.v]);
        if(texCoordsArray.size() > 0) {
          if (static_cast<size_t>(v.vt) >= texCoordsArray.size())
            throw runtime_error("Invalid texture coordinate index.");
          aMesh->mTexCoords.push_back(texCoordsArray[v.vt]);
        }
        if(normalsArray.size() > 0) {
          if (static_cast<size_t>(v.vn) >= normalsArray.size())
            throw runtime_error("Invalid vertex normal index.");
          aMesh->mNormals.push_back(normalsArray[v.vn]);
        }
      }
      else
      {
        v = *it;
      }

      if(nodeCount < 3)
      {
        nodes[nodeCount] = v;
      }
      else
      {
        // Create a new triangle by updating one vertex
        nodes[1] = nodes[2];
        nodes[2] = v;
      }
      ++ nodeCount;

      // Emit one triangle
      if(nodeCount >= 3)
      {
        aMesh->mIndices[idx ++] = nodes[0].index;
        aMesh->mIndices[idx ++] = nodes[1].index;
        aMesh->mIndices[idx ++] = nodes[2].index;
      }
    }
  }
}

/// Export a mesh to an OBJ file.
void Export_OBJ(const char * aFileName, Mesh * aMesh, Options &aOptions)
{
  // Open the output file
  ofstream f(aFileName, ios_base::out);
  if(f.fail())
    throw runtime_error("Could not open output file.");

  Export_OBJ(f, aMesh, aOptions);

  // Close the output file
  f.close();
}

/// Export a mesh to an OBJ stream.
void Export_OBJ(std::ostream &f, Mesh * aMesh, Options &aOptions) {
  // What should we export?
  bool exportTexCoords = aMesh->HasTexCoords() && !aOptions.mNoTexCoords;
  bool exportNormals = aMesh->HasNormals() && !aOptions.mNoNormals;

  // Set floating point precision
  f << setprecision(8);

  // Write comment
  if(aMesh->mComment.size() > 0)
  {
    stringstream sstr(aMesh->mComment);
    sstr.seekg(0);
    while(!sstr.eof())
    {
      std::string line;
      getline(sstr, line);
      line = TrimString(line);
      if(line.size() > 0)
        f << "# " << line << endl;
    }
  }

  // Write vertices
  for(unsigned int i = 0; i < aMesh->mVertices.size(); ++ i)
    f << "v " << aMesh->mVertices[i].x << " " << aMesh->mVertices[i].y << " " << aMesh->mVertices[i].z << endl;

  // Write UV coordinates
  if(exportTexCoords)
  {
    for(unsigned int i = 0; i < aMesh->mTexCoords.size(); ++ i)
      f << "vt " << aMesh->mTexCoords[i].u << " " << aMesh->mTexCoords[i].v << endl;
  }

  // Write normals
  if(exportNormals)
  {
    for(unsigned int i = 0; i < aMesh->mNormals.size(); ++ i)
      f << "vn " << aMesh->mNormals[i].x << " " << aMesh->mNormals[i].y << " " << aMesh->mNormals[i].z << endl;
  }

  // Write faces
  unsigned int triCount = aMesh->mIndices.size() / 3;
  f << "s 1" << endl; // Put all faces in the same smoothing group
  for(unsigned int i = 0; i < triCount; ++ i)
  {
    unsigned int idx = aMesh->mIndices[i * 3] + 1;
    f << "f " << idx << "/";
    if(exportTexCoords)
      f << idx;
    f << "/";
    if(exportNormals)
      f << idx;

    idx = aMesh->mIndices[i * 3 + 1] + 1;
    f << " " << idx << "/";
    if(exportTexCoords)
      f << idx;
    f << "/";
    if(exportNormals)
      f << idx;

    idx = aMesh->mIndices[i * 3 + 2] + 1;
    f << " " << idx << "/";
    if(exportTexCoords)
      f << idx;
    f << "/";
    if(exportNormals)
      f << idx;
    f << endl;
  }
}
