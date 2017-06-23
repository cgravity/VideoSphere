#version 120

uniform float theta; // shift longitude
uniform float phi;   // shift latitude

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
    // screen is in XZ plane by default, but rendering goes to XY plane
    vec4 v = vec4(gl_Vertex.x, 0, gl_Vertex.y, 1);
    v.x *= width/2;
    v.z *= height/2;
    
    mat4 rot_theta = rotationMatrix(vec3(0,0,1), theta);
    mat4 rot_phi  = rotationMatrix(vec3(1,0,0), phi);
    mat4 rot_tp = rot_theta * rot_phi;
    
    // FIXME: remove old position calculation
    //pos = (rot * vec4(pos_in, 1.0)).xyz;
    
    mat4 rot_r = rotationMatrix(vec3(0,1,0), deg2rad(roll));
    mat4 rot_p = rotationMatrix(vec3(1,0,0), deg2rad(pitch));
    mat4 rot_h = rotationMatrix(vec3(0,0,1), deg2rad(heading));
    
    mat4 rot_rph = rot_h * rot_p * rot_r;
    mat4 rot_mix = rot_theta * rot_h * rot_phi * rot_p * rot_r;
    
    mat4 rot = rot_tp * rot_rph;
    
    v = (rot * v);
    
    pos = vec3(v.x + originX, v.y + originY, v.z + originZ);
    //pos = (rot_tp * vec4(pos_in, 1.0)).xyz;
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

