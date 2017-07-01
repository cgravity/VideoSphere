#version 120

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

    float lat = latlong.x;
    float lon = mod(latlong.y, TURN);
    
    float x = lon / TURN;
    float y = ((lat / (0.25*TURN) + 1.0)) / 2.0;
    
    y /= 2;
    y += stereo_half * 0.5;
    
    gl_FragColor = texture2D(video_texture, vec2(1-x,1-y));
}

