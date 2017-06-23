#version 120

//uniform float theta; // shift longitude
//uniform float phi;   // shift latitude

// per screen
uniform float roll;
uniform float pitch;
uniform float heading;
uniform float originX;
uniform float originY;
uniform float originZ;
uniform float width;
uniform float height;

attribute vec3 pos_in;
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

float deg2rad(float d)
{
    return 180.0 / 3.1415926535 * d;
}

void main()
{
    // note: screen is in XZ plane by default, but rendering goes to XY plane
    vec3 center3 = vec3(originX, originY, originZ);
    vec3 corner_offset3 = vec3(width/2 * gl_Vertex.x, 0, height/2 * gl_Vertex.y);
    vec3 corner = center3 + corner_offset3;
    
    mat4 rot_r = rotationMatrix(vec3(0,1,0), deg2rad(roll));
    mat4 rot_p = rotationMatrix(vec3(1,0,0), deg2rad(pitch));
    mat4 rot_h = rotationMatrix(vec3(0,0,1), deg2rad(heading));
    
    mat4 rot_rph = rot_h * rot_p * rot_r;
    
    pos = (rot_rph * vec4(corner,1)).xyz;
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

