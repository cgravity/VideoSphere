#version 120

uniform sampler2D video_texture;
varying vec4 tex_coord;

void main()
{
    gl_FragColor = texture2D(video_texture, tex_coord.xy);
}
