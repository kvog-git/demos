#include "shaders.hh"

static int u_n = 3;
static float u_bg[4] = { 0, 0, 0, 1 };
static float u_fg[4] = { 1, 1, 1, 1 };

static void ui()
{
  ImGui::SliderInt("N", &u_n, 3, 24);
  ImGui::ColorPicker4("bg", u_bg);
  ImGui::ColorPicker4("fg", u_fg);
}

static void uniforms(Shader* shader)
{
  GL(glUniform1i(shader->get_required_uniform("u_n"), u_n));
  GL(glUniform4fv(shader->get_required_uniform("u_bg"), 1, u_bg));
  GL(glUniform4fv(shader->get_required_uniform("u_fg"), 1, u_fg));
}

static Shader polygon = {
  .path   = "polygon.glsl",
  .model  = MODEL_QUAD,
  .ui_fn  = ui,
  .uf_fn  = uniforms,
};
ENABLE_SHADER(polygon);
