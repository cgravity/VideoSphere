#version 120

uniform float theta;
uniform float phi;
in vec3 pos_in;
varying vec3 pos;

// http://www.neilmendoza.com/glsl-rotation-about-an-arbitrary-axis/
mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(
        oc * axis.x * axis.x + c,           
        oc * axis.x * axis.y - axis.z * s,  
        oc * axis.z * axis.x + axis.y * s,  
        0.0,
        
        oc * axis.x * axis.y + axis.z * s,  
        oc * axis.y * axis.y + c,           
        oc * axis.y * axis.z - axis.x * s,  
        0.0,
        
        oc * axis.z * axis.x - axis.y * s,  
        oc * axis.y * axis.z + axis.x * s,  
        oc * axis.z * axis.z + c,           
        0.0,
        
        
        0.0,
        0.0,
        0.0,                                
        1.0);
}


void main()
{
    mat4 rot_long = rotationMatrix(vec3(0,0,1), theta);
    mat4 rot_lat  = rotationMatrix(vec3(1,0,0), phi);
    mat4 rot = rot_long * rot_lat;
    
    pos = (rot * vec4(pos_in, 1.0)).xyz;
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

