#pragma once

#include <string>

inline std::string sceneModelAuthoringSource() {
  return R"(
import /std/scene/*
import /std/collections/*
import /std/math/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  len{words.count()}
  for([i32 mut] index{0i32}, index < len, ++index) {
    if(index > 0i32) {
      print(","utf8)
    }
    print(words[index])
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [Scene mut] scene{Scene{}}
  cameraId{scene.appendCamera(ui_camera(320i32, 200i32))}
  rejectedCameraId{scene.appendCamera(Camera{[projectionMode] 2i32})}
  lightRigId{scene.appendUiLightRig()}
  materialId{scene.appendMaterial(Material{
    [r] 0.25f32,
    [g] 0.5f32,
    [b] 0.75f32,
    [opacity] 0.875f32,
    [shadeStrength] 1.0f32
  })}
  primitiveId{scene.appendPrimitive(Primitive{
    [kind] primitive_rect(),
    [width] 50.0f32,
    [height] 25.0f32,
    [radius] 4.0f32,
    [materialId] materialId
  })}
  rootId{scene.appendNode(
    -1i32,
    Transform{[translationX] 10.0f32, [translationY] 20.0f32},
    2i32,
    0.0f32,
    primitiveId,
    materialId
  )}
  childId{scene.appendNode(
    rootId,
    Transform{
      [translationX] 3.0f32,
      [translationY] 5.0f32,
      [translationZ] 4.0f32,
      [scaleX] 2.0f32,
      [rotationZDegrees] 45.0f32
    },
    1i32,
    4.0f32,
    primitiveId,
    materialId
  )}
  activeCamera{scene.cameraAt(cameraId)}
  child{scene.nodeAt(childId)}
  dump_words(scene.serialize())
  print_line(convert<int>(activeCamera.sceneXForPixel(7i32) * 1000.0f32))
  print_line(convert<int>(activeCamera.sceneYForPixel(9i32) * 1000.0f32))
  print_line(activeCamera.pixelXForScene(7.0f32))
  print_line(activeCamera.pixelYForScene(9.0f32))
  print_line(rejectedCameraId)
  return(scene.nodeCount() + scene.materialCount() + scene.lightCount() + scene.cameraCount() + lightRigId + child.parentId)
}
)";
}

inline std::string expectedSceneModelAuthoringOutput() {
  return "1,0,2,0,-1,2,0,0,0,10000,20000,0,1000,1000,1000,0,"
         "1,0,1,4000,0,0,3000,5000,4000,2000,1000,1000,45000,"
         "1,0,1,50000,25000,4000,0,"
         "1,0,250,500,750,875,1000,"
         "2,0,1,550,0,0,1000,1,2,450,-1000,-1000,1000,"
         "1,0,1,320000,200000,1000,0,0,-1000000,1000000\n"
         "7000\n"
         "9000\n"
         "7\n"
         "9\n"
         "-1\n";
}
