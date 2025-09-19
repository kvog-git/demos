
// keep the line break above

out vec4 f_color;

void main()
{
  vec4 color;
  mainImage(color, gl_FragCoord.xy);
  f_color = color;
}
