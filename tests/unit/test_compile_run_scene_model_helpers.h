#pragma once

#include <string>

inline std::string sceneModelCppEmitterSource() {
  return R"(
[struct]
Transform() {
  [f32] translationX{0.0f32}
  [f32] translationY{0.0f32}
  [f32] translationZ{0.0f32}
  [f32] scaleX{1.0f32}
  [f32] scaleY{1.0f32}
  [f32] scaleZ{1.0f32}
  [f32] rotationZDegrees{0.0f32}
}

[struct]
Camera() {
  [i32] projectionMode{1i32}
  [f32] viewportWidth{1.0f32}
  [f32] viewportHeight{1.0f32}
  [f32] unitsPerPixel{1.0f32}
  [f32] originX{0.0f32}
  [f32] originY{0.0f32}
}

[struct]
Material() {
  [f32] r{1.0f32}
  [f32] g{1.0f32}
  [f32] b{1.0f32}
  [f32] opacity{1.0f32}
  [f32] shadeStrength{1.0f32}
}

[struct]
Light() {
  [i32] kind{1i32}
  [f32] weight{1.0f32}
  [f32] directionX{0.0f32}
  [f32] directionY{0.0f32}
  [f32] directionZ{1.0f32}
}

[struct]
Primitive() {
  [i32] kind{1i32}
  [f32] width{0.0f32}
  [f32] height{0.0f32}
  [f32] radius{0.0f32}
  [i32] materialId{-1i32}
}

[struct]
Node() {
  [i32] id{-1i32}
  [i32] parentId{-1i32}
  [Transform] transform{Transform{}}
  [i32] painterOrder{0i32}
  [f32] localZ{0.0f32}
  [i32] primitiveId{-1i32}
  [i32] materialId{-1i32}
}

[return<int>]
main() {
  [Camera] camera{Camera{
    [projectionMode] 1i32,
    [viewportWidth] 320.0f32,
    [viewportHeight] 200.0f32,
    [unitsPerPixel] 1.0f32
  }}
  [Material] material{Material{[r] 0.25f32, [g] 0.5f32, [b] 0.75f32, [opacity] 0.875f32}}
  [Light] light{Light{[kind] 2i32, [weight] 0.45f32, [directionX] -1.0f32, [directionY] -1.0f32}}
  [Primitive] primitive{Primitive{[kind] 1i32, [width] 50.0f32, [height] 25.0f32, [radius] 4.0f32, [materialId] 0i32}}
  [Node] node{Node{
    [id] 1i32,
    [parentId] 0i32,
    [transform] Transform{[translationX] 3.0f32, [translationY] 5.0f32, [translationZ] 4.0f32},
    [painterOrder] 2i32,
    [localZ] 4.0f32,
    [primitiveId] 0i32,
    [materialId] 0i32
  }}
  return(camera.projectionMode + light.kind + primitive.kind + node.id +
         node.painterOrder + convert<int>(material.opacity * 8.0f32))
}
)";
}

inline std::string sceneModelNativeDescriptorSource() {
  return sceneModelCppEmitterSource();
}

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

inline std::string uiSceneAdapterSource() {
  return R"(
import /std/ui/*
import /std/collections/*

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
  [LayoutTree mut] layout{LayoutTree{}}
  root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  panel{layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)}
  label{layout.append_leaf(panel, 10i32, 10i32)}
  button{layout.append_leaf(panel, 20i32, 14i32)}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 32i32)

  [UiScene mut] scene{UiScene{}}
  [UiSceneTextOverlays mut] overlays{UiSceneTextOverlays{}}
  nodes{layout.emit_scene_panel_button(
    scene,
    overlays,
    panel,
    label,
    button,
    10i32,
    2i32,
    4i32,
    3i32,
    Rgba8{[r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32},
    Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32},
    Rgba8{[r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32},
    Rgba8{[r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32},
    2i32,
    "Hi"utf8,
    2i32,
    "Go"utf8
  )}

  dump_words(scene.serialize())
  dump_words(overlays.serialize())
  print_line(nodes.panel)
  print_line(nodes.label)
  print_line(nodes.button)
  return(
    scene.nodeCount() +
    scene.primitiveCount() +
    scene.materialCount() +
    overlays.overlay_count() +
    nodes.button
  )
}
)";
}

inline std::string expectedUiSceneAdapterOutput() {
  return "1,3,0,1,-1,0,0,0,0,7,8,1000,1,2,0,1,0,-1,-1,"
         "9,10,1000,2,3,0,2,3000,1,1,9,21,3000,2,0,1,38,"
         "29,4,0,1,2,34,14,3,1,2,0,9,8,7,255,1,50,60,70,"
         "255\n"
         "1,2,2,1,9,10,10,1,2,3,255,2,72,105,3,2,11,23,10,"
         "250,251,252,255,2,71,111\n"
         "0\n"
         "1\n"
         "2\n";
}
