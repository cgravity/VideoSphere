#version 120

varying vec4 tex_coord;

void main()
{
    tex_coord = gl_MultiTexCoord0;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

