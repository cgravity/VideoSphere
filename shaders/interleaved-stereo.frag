#version 120

//layout(origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;

uniform sampler2D video_texture;
uniform float phi;
uniform float theta;
uniform float stereo_half;

varying vec3 pos;

const float TURN = 6.283185307179586;

vec2 vec3_to_latlong(vec3 vec)
{
    float out_long = atan(vec.y, vec.x);
    if(vec.y < 0)
        out_long += TURN;
    
    float out_lat = (vec.z == 1.0 ? TURN/4 :
        (vec.z == -1.0? -TURN/4 :
        (asin(vec.z))));
    
    return vec2(out_lat, out_long);
}

void main()
{
    vec2 latlong = vec3_to_latlong(normalize(pos));
    
    // eye separation angle to add, based on empirical testing w/ Dan on CAVE2
    float empirical_eye_sep = stereo_half * 0.9 * 2.0 / 30.0 * TURN/16.0;

    float lat = latlong.x;
    float lon = mod(latlong.y - empirical_eye_sep, TURN);
    
    float x = lon / TURN;
    float y = ((lat / (0.25*TURN) + 1.0)) / 2.0;
    
    y /= 2;
    y += stereo_half * 0.5;
    
    if(mod(floor(gl_FragCoord.y),2) == stereo_half)
        discard;
    
    gl_FragColor = texture2D(video_texture, vec2(1-x,1-y));
    //gl_FragColor = vec4(0,1,0,1);
}

