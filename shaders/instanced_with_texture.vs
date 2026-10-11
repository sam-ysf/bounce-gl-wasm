R"(
attribute vec3 a_pos;
attribute vec2 a_texCoord;
attribute mat4 a_inst;

varying vec2 v_texCoord;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    v_texCoord = a_texCoord;
    gl_Position = projection * view * a_inst * vec4(a_pos, 1.0);
}
)"