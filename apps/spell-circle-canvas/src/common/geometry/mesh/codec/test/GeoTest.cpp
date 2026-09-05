/** @file
 * The Houdini `.geo` reader: a polygon archive unwelds into vertex and
 * primitive classes, and a point archive comes back an honest cloud
 * rather than a mesh with no faces.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;


TEST(ReadGeo, PolygonsUnweldWithTheirVertexAndPrimitiveClasses) {
  // A quad and a triangle over five points, written the way Houdini
  // saves ASCII .geo: alternating key/value arrays, paged attribute
  // storage for P, a plain tuple list for a point N, a VERTEX uv (which
  // outranks any point uv), a primitive Cd, a point group and a
  // primitive group. Points 0-3 are the quad (y = 0 and y = 100),
  // point 4 sits above and forms a triangle with points 2 and 3.
  const char* geo = R"([
    "fileversion","20.5.278",
    "hasindex",false,
    "pointcount",5,
    "vertexcount",7,
    "primitivecount",2,
    "info",{"software":"Houdini 20.5.278"},
    "topology",["pointref",["indices",[0,1,2,3,3,2,4]]],
    "attributes",[
      "vertexattributes",[
        [
          ["scope","public","type","numeric","name","uv","options",{}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,1,0],[1,1,0],[0.5,1,0]]]]
        ]
      ],
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{"type":{"type":"string","value":"point"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",1024,
             "constantpageflags",[[false]],
             "rawpagedata",[0,0,0, 100,0,0, 100,100,0, 0,100,0, 50,180,0]]]
        ],
        [
          ["scope","public","type","numeric","name","N","options",{"type":{"type":"string","value":"normal"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",3,"storage","fpreal32","tuples",
             [[0,0,1],[0,0,1],[0,0,1],[0,0,1],[0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","pscale","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",1,"storage","fpreal32","arrays",[[1,2,3,4,5]]]]
        ]
      ],
      "primitiveattributes",[
        [
          ["scope","public","type","numeric","name","Cd","options",{"type":{"type":"string","value":"color"}}],
          ["size",3,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[1]],
           "values",["size",3,"storage","fpreal32","tuples",[[1,0,0],[0,0,1]]]]
        ]
      ],
      "globalattributes",[
        [
          ["scope","public","type","numeric","name","frame","options",{}],
          ["size",1,"storage","fpreal32","defaults",["size",1,"storage","fpreal64","values",[0]],
           "values",["size",1,"storage","fpreal32","arrays",[[12]]]]
        ]
      ]
    ],
    "primitives",[
      [["type","Polygon"],["vertex",[0,1,2,3],"closed",true]],
      [["type","Polygon"],["vertex",[4,5,6],"closed",true]]
    ],
    "pointgroups",[
      [["name","top"],["selection",["unordered",["boolRLE",[2,false,2,true,1,true]]]]]
    ],
    "primitivegroups",[
      [["name","front"],["selection",["unordered",["i8",[1,0]]]]]
    ]
  ])";
  const std::optional<codec::decode::Model> model =
      codec::decode::model(geo, std::strlen(geo), "scene.geo");
  ASSERT_TRUE(model);
  ASSERT_EQ(model->parts.size(), 1u);
  const codec::decode::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  // Unwelded: 4 + 3 vertices; the quad fans into two triangles.
  EXPECT_EQ(mesh.vertexCount(), 7u);
  EXPECT_EQ(mesh.triangleCount(), 3u);
  EXPECT_EQ(mesh.positions[2].x, 100.0f);
  EXPECT_EQ(mesh.positions[2].y, 100.0f);
  EXPECT_EQ(mesh.positions[6].y, 180.0f);  // vertex 6 -> point 4
  ASSERT_EQ(mesh.normals.size(), 7u);
  EXPECT_FLOAT_EQ(mesh.normals[0].z, 1.0f);
  ASSERT_EQ(mesh.uvs.size(), 7u);
  // Vertex uv, v flipped to the top-left convention.
  EXPECT_FLOAT_EQ(mesh.uvs[2].x, 1.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[2].y, 0.0f);
  EXPECT_FLOAT_EQ(mesh.uvs[6].x, 0.5f);
  // Primitive Cd -> the "Color" prim lane, replicated over the fan.
  const std::vector<glm::vec4>* color = mesh.primIf("Color");
  ASSERT_TRUE(color);
  ASSERT_EQ(color->size(), 3u);
  EXPECT_FLOAT_EQ((*color)[0].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[1].r, 1.0f);
  EXPECT_FLOAT_EQ((*color)[2].b, 1.0f);
  // Primitive group -> a 0/1 prim lane.
  const std::vector<glm::vec4>* front = mesh.primIf("front");
  ASSERT_TRUE(front);
  EXPECT_FLOAT_EQ((*front)[0].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*front)[2].x, 0.0f);
  // Point attributes ride to the Part through the owning point;
  // point groups are 0/1 scalar lanes.
  const auto pscale = part.scalarLanes.find("pscale");
  ASSERT_NE(pscale, part.scalarLanes.end());
  ASSERT_EQ(pscale->second.size(), 7u);
  EXPECT_FLOAT_EQ(pscale->second[6], 5.0f);
  const auto top = part.scalarLanes.find("top");
  ASSERT_NE(top, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(top->second[0], 0.0f);
  EXPECT_FLOAT_EQ(top->second[2], 1.0f);
  EXPECT_FLOAT_EQ(top->second[6], 1.0f);
  // Sniffed without an extension too.
  EXPECT_TRUE(codec::decode::model(geo, std::strlen(geo), ""));
  // ...and it feeds the pop system through asCloud like any import.
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 7u);
  EXPECT_TRUE(cloud.scalarIf("top"));
}

TEST(ReadGeo, APointArchiveComesBackACloudRatherThanAFacelessMesh) {
  // No primitives: a particle-style file. P in a paged layout whose
  // second page is CONSTANT (every point on it shares one tuple), an
  // int id, a float4 orient, a string name (kept out of the lanes), and
  // a Cd point colour that lands on the mesh colour lane.
  const char* geo = R"([
    "fileversion","20.5.278",
    "pointcount",6,
    "vertexcount",0,
    "primitivecount",0,
    "topology",["pointref",["indices",[]]],
    "attributes",[
      "pointattributes",[
        [
          ["scope","public","type","numeric","name","P","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[3],"pagesize",4,
             "constantpageflags",[[false,true]],
             "rawpagedata",[0,0,0, 1,0,0, 2,0,0, 3,0,0,  9,9,9]]]
        ],
        [
          ["scope","public","type","numeric","name","id","options",{}],
          ["size",1,"storage","int32",
           "values",["size",1,"storage","int32","arrays",[[10,11,12,13,14,15]]]]
        ],
        [
          ["scope","public","type","numeric","name","orient","options",{}],
          ["size",4,"storage","fpreal32",
           "values",["size",4,"storage","fpreal32","tuples",
             [[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1],[0,0,0,1]]]]
        ],
        [
          ["scope","public","type","numeric","name","Cd","options",{}],
          ["size",3,"storage","fpreal32",
           "values",["size",3,"storage","fpreal32","packing",[1,1,1],"pagesize",8,
             "constantpageflags",[[true],[true],[false]],
             "rawpagedata",[0.5, 0.25, 0,0.2,0.4,0.6,0.8,1.0]]]
        ],
        [
          ["scope","public","type","string","name","name","options",{}],
          ["size",1,"storage","int32","strings",["a","b"],
           "indices",["size",1,"storage","int32","arrays",[[0,1,0,1,0,1]]]]
        ]
      ]
    ],
    "primitives",[]
  ])";
  const std::optional<codec::decode::Model> model =
      codec::decode::model(geo, std::strlen(geo), "particles.geo");
  ASSERT_TRUE(model);
  const codec::decode::Part& part = model->parts.front();
  const Mesh& mesh = part.mesh;
  EXPECT_EQ(mesh.vertexCount(), 6u);
  EXPECT_EQ(mesh.triangleCount(), 0u);
  EXPECT_FLOAT_EQ(mesh.positions[3].x, 3.0f);
  // The constant page: points 4 and 5 both read the one tuple.
  EXPECT_FLOAT_EQ(mesh.positions[4].x, 9.0f);
  EXPECT_FLOAT_EQ(mesh.positions[5].z, 9.0f);
  // Split packing [1,1,1]: R and G constant pages, B a full page.
  ASSERT_EQ(mesh.colors.size(), 6u);
  EXPECT_FLOAT_EQ(mesh.colors[0].r, 0.5f);
  EXPECT_FLOAT_EQ(mesh.colors[5].g, 0.25f);
  EXPECT_FLOAT_EQ(mesh.colors[2].b, 0.4f);
  EXPECT_FLOAT_EQ(mesh.colors[5].b, 1.0f);
  const auto id = part.scalarLanes.find("id");
  ASSERT_NE(id, part.scalarLanes.end());
  EXPECT_FLOAT_EQ(id->second[5], 15.0f);
  const auto orient = part.colorLanes.find("orient");
  ASSERT_NE(orient, part.colorLanes.end());
  EXPECT_FLOAT_EQ(orient->second[0].w, 1.0f);
  EXPECT_EQ(part.scalarLanes.count("name"), 0u);
  const Cloud cloud = part.asCloud();
  EXPECT_EQ(cloud.size(), 6u);
  EXPECT_TRUE(cloud.colorIf("tint"));
  EXPECT_TRUE(cloud.scalarIf("id"));
}
