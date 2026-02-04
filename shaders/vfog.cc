#include "shaders.hh"

static int u_mode = 0;
static int u_steps = 8;
static float u_sigma = 4.0f;

static void ui()
{
  const char* modes[] = { "basic", "vfade", "waves" };
  if (ImGui::BeginCombo("mode", modes[u_mode])) {
    for (int i = 0; i < (int)COUNTOF(modes); ++i) {
      if (ImGui::Selectable(modes[i], i == u_mode)) {
        u_mode = i;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SliderInt("steps", &u_steps, 1, 8);
  ImGui::InputFloat("sigma", &u_sigma);
}

static void uniforms(Shader* shader)
{
  GL(glUniform1i(shader->get_required_uniform("u_mode"), u_mode));
  GL(glUniform1i(shader->get_required_uniform("u_steps"), u_steps));
  GL(glUniform1f(shader->get_required_uniform("u_sigma"), u_sigma));
}

static Shader vfog = {
  .path   = "vfog.glsl",
  .model  = MODEL_CUBE,
  .ui_fn  = ui,
  .uf_fn  = uniforms,
};
ENABLE_SHADER(vfog);
