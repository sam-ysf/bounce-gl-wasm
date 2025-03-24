R"(
attribute vec3 a_pos;
attribute mat4 a_inst;

varying vec4 v_color;

uniform vec4 color;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    v_color = color;
    gl_Position = projection * view * a_inst * vec4(a_pos, 1.0);
}
)"
