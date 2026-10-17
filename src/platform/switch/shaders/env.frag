#version 460
layout(binding = 0) uniform sampler2D atlas;
layout(std140, binding = 0) uniform Params
{
    vec4 p[64];
    mat4 bones[20];
};
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
#define right p[1].xyz
#define up p[2].xyz
#define forward p[3].xyz
#define face_size int(p[0].x)
#define map_size int(p[0].y)
const vec3 F[6] = vec3[6](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 0, 1), vec3(0, 0, -1),
                          vec3(0, 1, 0), vec3(0, -1, 0));
const vec3 U[6] = vec3[6](vec3(0, 1, 0), vec3(0, 1, 0), vec3(0, 1, 0), vec3(0, 1, 0),
                          vec3(0, 0, -1), vec3(0, 0, 1));
vec3 get(int face, int x, int y)
{
    return texelFetch(atlas, ivec2(face * face_size + x, y), 0).rgb * 255.;
}
void main()
{
    vec2 pixel = uv * float(map_size);
    float mx = (pixel.x / float(map_size) - .5) * 2.;
    float my = -(pixel.y / float(map_size) - .5) * 2.;
    float m2 = mx * mx + my * my;
    if (m2 > 1.) {
        float k = 1. / sqrt(m2);
        mx *= k;
        my *= k;
        m2 = 1.;
    }
    float nz = sqrt(1. - m2);
    vec3 R = vec3(2. * nz * mx, 2. * nz * my, 2. * nz * nz - 1.);
    vec3 D = R.x * right + R.y * up - R.z * forward;
    int best = 0;
    float big = -1.;
    for (int i = 0; i < 6; i++) {
        float q = dot(D, F[i]);
        if (q > big) {
            big = q;
            best = i;
        }
    }
    vec3 rr = cross(F[best], U[best]);
    float qx = dot(D, rr) / big, qy = dot(D, U[best]) / big;
    float halfsize = float(face_size) / 2.;
    float fx = clamp(halfsize + qx * halfsize - .5, 0., float(face_size - 1));
    float fy = clamp(halfsize - qy * halfsize - .5, 0., float(face_size - 1));
    int x0 = int(fx), y0 = int(fy), x1 = min(x0 + 1, face_size - 1),
        y1 = min(y0 + 1, face_size - 1);
    float wx = fx - float(x0), wy = fy - float(y0);
    vec3 a = get(best, x0, y0) * (1. - wx) + get(best, x1, y0) * wx;
    vec3 b = get(best, x0, y1) * (1. - wx) + get(best, x1, y1) * wx;
    color = vec4(floor(clamp(a * (1. - wy) + b * wy + .5, 0., 255.)) / 255., 1.);
}
