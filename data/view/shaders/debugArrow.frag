#version 300 es
precision mediump float;

uniform float time;

in vec4 inout_color;
in vec2 inout_uv;
in float inout_speed;

layout(location = 0) out vec4 out_color;

// signed distance to an equilateral triangle by IQ: https://www.shadertoy.com/view/Xl2yDW
float sdEquilateralTriangle(  in vec2 p, in float r )
{
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r/k;
    if( p.x+k*p.y>0.0 ) p=vec2(p.x-k*p.y,-k*p.x-p.y)/2.0;
    p.x -= clamp( p.x, -2.0*r, 0.0 );
    return -length(p)*sign(p.y);
}

void main()
{
    vec2 uv = inout_uv;
    uv.y -= time * inout_speed;

    uv = fract(uv);
    uv = uv*2.0-1.0;
    uv.y += 0.2;


    float d = sdEquilateralTriangle( uv, 1.0 );
    float a = d > 0.0 ? 0.0 : 1.0;
    vec3 col = d > 0.0 ? vec3(1.0, 0.0, 0.0) : inout_color.rgb;

    //out_color = vec4(fract(uv), 0.0, 1.0);
    out_color = vec4(col, a);
}

