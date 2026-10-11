R"(
precision mediump float;

varying vec2 v_texCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    gl_FragColor = mix(texture2D(texture1, v_texCoord), texture2D(texture2, v_texCoord), 0.4);
}
)"