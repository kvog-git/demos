#include "shaders.hh"

static int u_checker = 16;
static float u_checker_c1[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
static float u_checker_c2[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float u_ww = 500;
static float u_wh = 400;

static void ui()
{
  ImGui::InputInt("Checker", &u_checker);
  ImGui::SliderFloat("window w", &u_ww, 10, 1000);
  ImGui::SliderFloat("window h", &u_wh, 10, 1000);
}

static void uniforms(Shader* shader)
{
  GL(glUniform1i(shader->get_required_uniform("u_checker"), u_checker));
  GL(glUniform4fv(shader->get_required_uniform("u_checker_c1"), 1, u_checker_c1));
  GL(glUniform4fv(shader->get_required_uniform("u_checker_c2"), 1, u_checker_c2));
  GL(glUniform1f(shader->get_required_uniform("u_ww"), u_ww));
  GL(glUniform1f(shader->get_required_uniform("u_wh"), u_wh));
}

static Shader aero = {
  .path   = "aero.glsl",
  .model  = MODEL_QUAD,
  .ui_fn  = ui,
  .uf_fn  = uniforms,
};
ENABLE_SHADER(aero);
