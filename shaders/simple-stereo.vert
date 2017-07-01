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
    float s = -sin(angle);
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

// rodriguez rotation
// https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
vec3 rotate(vec3 axis, float theta, vec3 v)
{
    vec3 k = normalize(axis);
    return v * cos(theta) + cross(k,v)*sin(theta) + k*dot(k,v)*(1-cos(theta));
}

float deg2rad(float d)
{
    return 3.1415926535 / 180.0 * d;
}

void main()
{
    // note: screen is in XZ plane by default, but rendering goes to XY plane
    vec3 center3 = vec3(originX, originY, originZ);
    vec3 corner_offset3 = vec3(width/2 * pos_in.x, 0, height/2 * pos_in.y);
    vec3 corner = center3 + corner_offset3;
    
    mat4 rot_r = rotationMatrix(vec3(0,1,0), deg2rad(roll));
    mat4 rot_p = rotationMatrix(vec3(1,0,0), deg2rad(pitch));
    mat4 rot_h = rotationMatrix(vec3(0,0,1), deg2rad(heading));
    
    mat4 rot_theta = rotationMatrix(vec3(0,0,1), theta);
    mat4 rot_phi = rotationMatrix(vec3(1,0,0), phi);

    
    // FIXME: something wrong with rotationMatrix -- rot_h and rot_r
    // don't give identity as they should on WAVE! Note, had to change
    // to -sin() as suggested in comments. This may be fine for now on WAVE
    // as it only uses pitch, but other systems need the full rph setting...
    mat4 rot_rph = rot_h * rot_p * rot_r;
    //rot_rph = rot_p;
    
    vec4 facing = vec4(originX, originY, originZ, 1);
    vec4 up = vec4(0,0,1,1);
    vec4 right = vec4(1,0,0,1);
    
    vec4 co4 = vec4(corner_offset3, 1);
    co4 = rot_rph * co4;
    co4 += vec4(center3,0);
    co4 = rot_theta * rot_phi * co4;
    pos = co4.xyz;
    
    //vec4 co4 = vec4(corner_offset3, 1);
    //pos = rotate(vec3(1,0,0), pitch, co4.xyz) + center3;
    
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

