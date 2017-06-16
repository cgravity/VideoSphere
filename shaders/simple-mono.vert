#version 120

in vec3 pos_in;
varying vec3 pos;

void main()
{
    pos = pos_in;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

