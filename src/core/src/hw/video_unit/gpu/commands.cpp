/*
 * Copyright (C) 2020 fleroviux
 */

#include <algorithm>

#include "gpu.hpp"

namespace Duality::Core {

void GPU::CMD_SetMatrixMode() {
  matrix_mode = static_cast<MatrixMode>(Dequeue().argument & 3);
}

void GPU::CMD_PushMatrix() {
  Dequeue();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.Push();
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.Push();
      direction.Push();
      break;
    case MatrixMode::Texture:
      texture.Push();
      break;
  }
}

void GPU::CMD_PopMatrix() {
  int offset = Dequeue().argument & 63;
  
  if (offset & 32) {
    offset -= 64;
  }
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.Pop(offset);
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.Pop(offset);
      direction.Pop(offset);
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.Pop(offset);
      break;
  }
}

void GPU::CMD_StoreMatrix() {
  auto address = Dequeue().argument & 31;
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.Store(address);
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.Store(address);
      direction.Store(address);
      break;
    case MatrixMode::Texture:
      texture.Store(address);
      break;
  }
}

void GPU::CMD_RestoreMatrix() {
  auto address = Dequeue().argument & 31;

  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.Restore(address);
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.Restore(address);
      direction.Restore(address);
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.Restore(address);
      break;
  }
}

void GPU::CMD_LoadIdentity() {
  Dequeue();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current.identity();
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current.identity();
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current.identity();
      direction.current.identity();
      break;
    case MatrixMode::Texture:
      texture.current.identity();
      break;
  }
}

void GPU::CMD_LoadMatrix4x4() {
  auto mat = DequeueMatrix4x4();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current = mat;
      direction.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = mat;
      break;
  }
}

void GPU::CMD_LoadMatrix4x3() {
  auto mat = DequeueMatrix4x3();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current = mat;
      direction.current = mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = mat;
      break;
  }
}

void GPU::CMD_MatrixMultiply4x4() {
  auto mat = DequeueMatrix4x4();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = projection.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current = modelview.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current = modelview.current * mat;
      direction.current = direction.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = texture.current * mat;
      break;
  }
}

void GPU::CMD_MatrixMultiply4x3() {
  auto mat = DequeueMatrix4x3();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = projection.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current = modelview.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current = modelview.current * mat;
      direction.current = direction.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = texture.current * mat;
      break;
  }
}

void GPU::CMD_MatrixMultiply3x3() {
  auto mat = DequeueMatrix3x3();
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = projection.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
      modelview.current = modelview.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Simultaneous:
      modelview.current = modelview.current * mat;
      direction.current = direction.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = texture.current * mat;
      break;
  }
}

void GPU::CMD_MatrixScale() {
  // TODO: this implementation is unoptimized.
  // A matrix multiplication is complete overkill for scaling.
  
  Matrix4<Fixed20x12> mat;
  for (int i = 0; i < 3; i++) {
    mat[i][i] = Dequeue().argument;
  }
  mat[3][3] = 0x1000;
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = projection.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.current = modelview.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = texture.current * mat;
      break;
  }
}

void GPU::CMD_MatrixTranslate() {
  Matrix4<Fixed20x12> mat;
  mat.identity();
  for (int i = 0; i < 3; i++) {
    mat[3][i] = Dequeue().argument;
  }
  
  switch (matrix_mode) {
    case MatrixMode::Projection:
      projection.current = projection.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Modelview:
    case MatrixMode::Simultaneous:
      modelview.current = modelview.current * mat;
      UpdateClipMatrix();
      break;
    case MatrixMode::Texture:
      texture.current = texture.current * mat;
      break;
  }
}

void GPU::CMD_SetColor() {
  // TODO: fix this absolutely atrocious code...

  auto arg = Dequeue().argument;

  auto r = (arg >>  0) & 31;
  auto g = (arg >>  5) & 31;
  auto b = (arg >> 10) & 31;

  vertex_color[0] = r * 2 + (r + 31) / 32;
  vertex_color[1] = g * 2 + (g + 31) / 32;
  vertex_color[2] = b * 2 + (b + 31) / 32;
}

void GPU::CMD_SetNormal() {
}

void GPU::CMD_SetUV() {
  auto arg = Dequeue().argument;
  vertex_uv = Vector2<Fixed12x4>{
    s16(arg & 0xFFFF),
    s16(arg >> 16)
  };
}

void GPU::CMD_SubmitVertex_16() {
  auto arg0 = Dequeue().argument;
  auto arg1 = Dequeue().argument;
  AddVertex({
    s16(arg0 & 0xFFFF),
    s16(arg0 >> 16),
    s16(arg1 & 0xFFFF),
    0x1000
  });
}

void GPU::CMD_SubmitVertex_10() {
  auto arg = Dequeue().argument;
  AddVertex({
    s16((arg >>  0) << 6),
    s16((arg >> 10) << 6),
    s16((arg >> 20) << 6),
    0x1000
  });
}

void GPU::CMD_SubmitVertex_XY() {
  auto arg = Dequeue().argument;
  AddVertex({
    s16(arg & 0xFFFF),
    s16(arg >> 16),
    position_old[2],
    0x1000
  });
}

void GPU::CMD_SubmitVertex_XZ() {
  auto arg = Dequeue().argument;
  AddVertex({
    s16(arg & 0xFFFF),
    position_old[1],
    s16(arg >> 16),
    0x1000
  });
}

void GPU::CMD_SubmitVertex_YZ() {
  auto arg = Dequeue().argument;
  AddVertex({
    position_old[0],
    s16(arg & 0xFFFF),
    s16(arg >> 16),
    0x1000
  });
}

void GPU::CMD_SubmitVertex_Offset() {
  auto arg = Dequeue().argument;
  AddVertex({
    position_old[0] + (s16((arg >>  0) << 6) >> 6),
    position_old[1] + (s16((arg >> 10) << 6) >> 6),
    position_old[2] + (s16((arg >> 20) << 6) >> 6),
    0x1000
  });
}

void GPU::CMD_SetTextureParameters() {
  auto arg = Dequeue().argument;

  texture_params.address = (arg & 0xFFFF) << 3;
  texture_params.repeat[0] = arg & (1 << 16);
  texture_params.repeat[1]= arg & (1 << 17);
  texture_params.flip[0] = arg & (1 << 18);
  texture_params.flip[1] = arg & (1 << 19);
  texture_params.size[0] = (arg >> 20) & 7;
  texture_params.size[1] = (arg >> 23) & 7;
  texture_params.format = static_cast<TextureParams::Format>((arg >> 26) & 7);
  texture_params.color0_transparent = arg & (1 << 29);
  texture_params.transform = static_cast<TextureParams::Transform>(arg >> 30);
}

void GPU::CMD_SetPaletteBase() {
  texture_params.palette_base = Dequeue().argument & 0x1FFF;
}

void GPU::CMD_BeginVertexList() {
  auto arg = Dequeue().argument;
  in_vertex_list = true;
  is_quad = arg & 1;
  is_strip = arg & 2;
  is_first = true;

  vertices.clear();

  // TODO: this is likely inaccurate.
  // I don't know when exactly (and if) vertex attributes are reset.
  for (int i = 0; i < 3; i++) {
    vertex_color[i] = 63;
  }
  vertex_uv = {};
}

void GPU::CMD_EndVertexList() {
  Dequeue();
  // TODO: allegedly this command is a no-operation on the DS...
  in_vertex_list = false;
}

void GPU::CMD_SwapBuffers() {
  Dequeue();
  gx_buffer_id ^= 1;
  vertex[gx_buffer_id].count = 0;
  polygon[gx_buffer_id].count = 0;
}

} // namespace Duality::Core

