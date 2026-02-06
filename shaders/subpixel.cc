#include "shaders.hh"

static float u_quad[2] = { 256.0f, 2.0f };

static void ui()
{
  ImGui::SliderFloat("quad width", &u_quad[0], 1.0f, 512.0f);
  ImGui::SliderFloat("quad height", &u_quad[1], 1.0f, 512.0f);
}

static void uniforms(Shader* shader)
{
  GL(glUniform2fv(shader->get_required_uniform("u_quad"), 1, u_quad));
}

static Shader subpixel = {
  .path   = "subpixel.glsl",
  .model  = MODEL_QUAD,
  .ui_fn  = ui,
  .uf_fn  = uniforms,
};
ENABLE_SHADER(subpixel);
