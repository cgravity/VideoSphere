#version 120

uniform sampler2D video_texture;
uniform float phi;
uniform float theta;

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
    int ix = 0;
    int iy = 0;
    
    int xsteps = 4;
    int ysteps = 4;
    
    float sx = 1.0 / xsteps;
    float sy = 1.0 / ysteps;
    
    
    vec4 color = vec4(0,0,0,0);
    
    for(iy = -ysteps/2; iy < ysteps/2; iy++)
    for(ix = -xsteps/2; ix < xsteps/2; ix++)
    {
        vec3 p_step_x = ix * sx * dFdx(pos);
        vec3 p_step_y = iy * sy * dFdy(pos);
        vec3 p = pos + p_step_x + p_step_y;
        
        vec2 latlong = vec3_to_latlong(normalize(p));

        float lat = latlong.x;
        float lon = mod(latlong.y, TURN);
    
        float x = lon / TURN;
        float y = ((lat / (0.25*TURN) + 1.0)) / 2.0;
    
        color += texture2D(video_texture, vec2(1-x,1-y));
    }
    
    gl_FragColor = color / (xsteps * ysteps);
}

