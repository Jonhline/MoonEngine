/* P: per-pixel evaluation keeps authored coefficients, pass isolation and opacity. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "video_gl.h"
#include "daylight.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SIZE 128
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "material shading %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct {
    int kind, curved, reflection, unlit, race, sun, hemispheric, dielectric, daylight, course,
        paint, halftex, overfill, stripes, local, coat;
    float specular, shine;
} fixture;

static void normalize(float p[3])
{
    float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    for (int i = 0; i < 3; i++)
        p[i] /= length;
}

static int sample(const fixture *f, int mode, uint8_t *pixels)
{
    gt4_triangle triangles[2] = {0};
    const float positions[6][3] = {{-2, -2, 3}, {2, -2, 3}, {2, 2, 3},
                                   {-2, -2, 3}, {2, 2, 3},  {-2, 2, 3}};
    for (int i = 0; i < 2; i++) {
        triangles[i].kind = f->kind;
        triangles[i].texture = f->reflection ? -2 : f->halftex ? 0 : -1;
        triangles[i].state.depth_func = 1;
        triangles[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &triangles[i].v[j];
            memcpy(v->p, positions[i * 3 + j], sizeof v->p);
            v->normal[0] = f->curved ? v->p[0] * .5f : .6f;
            v->normal[1] = f->curved ? v->p[1] * .5f : 0;
            v->normal[2] = f->curved ? -1 : -.8f;
            if (f->hemispheric) {
                v->normal[0] = 0;
                v->normal[1] = f->hemispheric > 0 ? .8f : -.8f;
                v->normal[2] = -.6f;
            }
            if (f->dielectric == 2) {
                v->normal[0] = .9949874f;
                v->normal[1] = 0;
                v->normal[2] = -.1f;
            }
            normalize(v->normal);
            if (f->coat) {
                v->normal[0] = v->normal[1] = 0;
                v->normal[2] = -1;
            }
            v->shine = f->shine;
            v->alpha = .5f;
            for (int c = 0; c < 3; c++) {
                v->rgb[c] = f->daylight ? 1.f : .8f;
                if (f->course && f->daylight) {
                    /* Supply a genuinely baked white reference surface. A value
                       of 1 is already exposed radiance, not white albedo. */
                    moon_daylight noon=moon_daylight_at(12);
                    float e=noon.sky[c]*.330863143f*.56f+
                            noon.direct[c]*(.8f*.573576436f);
                    v->rgb[c]=e<=.0031308f?12.92f*e:1.055f*powf(e,1.f/2.4f)-.055f;
                }
                v->ambient[c] = f->daylight ? 1.f : .5f;
                v->specular[c] = f->specular;
                if (f->paint) {
                    const float paint[3] = {.08f, .12f, .6f};
                    v->rgb[c] = paint[c];
                    v->ambient[c] = 0;
                }
            }
        }
    }
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 2;
    uint8_t grey[4] = {64,64,64,128};
    tex1_image grey_texture = {.width=1,.height=1,.tex_w=1,.tex_h=1,.rgba=grey};
    if (f->halftex) { model.textures=&grey_texture;model.ntextures=1; }

    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    uint8_t env[256 * 256 * 4];
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            int at = (y * 256 + x) * 4;
            env[at] = x;
            env[at + 1] = y;
            env[at + 2] = 128;
            env[at + 3] = 128;
            if (f->stripes) env[at] = env[at+1] = env[at+2] = (x / 8) % 2 ? 128 : 0;
            else if (f->dielectric)
                env[at] = env[at + 1] = env[at + 2] = 128;
            if (f->coat) env[at] = env[at+1] = env[at+2] = 0;
        }
    tex1_image environment = {0};
    environment.width = environment.height = 256;
    environment.rgba = env;
    r3d_environment_changed();
    r3d_begin(0, 0, 640, 448, &environment);
    if (!r3d_begin_target(SIZE))
        return 0;
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = f->unlit || f->course;
    if (f->course)
        view.color_scale = 255.f / 128.f;
    view.modern_light = 1;
    if (f->hemispheric || f->dielectric || f->daylight) {
        view.daylight = 1;
        for (int c = 0; c < 3; c++)
            view.sky_color[c] = f->daylight == 2 ? 0 : f->overfill ? .8f/.330863143f : 1;
        view.sun_direction[2] = -1;
        for (int c = 0; c < 3; c++)
            view.sun_color[c] = f->daylight >= 2 ? 1 : 0;
    }
    if (f->reflection) {
        view.env_color[0] = .8f;
        view.env_color[1] = -.2f;
    }
    if (f->coat) {
        view.daylight = 1;
        memset(view.sky_color, 0, sizeof view.sky_color);
        memset(view.sun_direction, 0, sizeof view.sun_direction);
        view.sun_direction[f->coat == 2 ? 0 : 2] = f->coat == 2 ? 1 : -1;
        if (f->coat == 3) {
            view.sun_direction[0] = .08f;
            view.sun_direction[2] = -sqrtf(1-.08f*.08f);
        }
        for (int c=0;c<3;c++) view.sun_color[c] = 1;
    }
    r3d_lights lights = {0};
    lights.count = 1;
    lights.direction[0][1] = f->sun ? .6f : 0;
    lights.direction[0][2] = f->sun ? -.8f : -1;
    lights.specular_scale = 1;
    lights.vertex_limit = f->race ? 255.f / 128 : 0;
    for (int c = 0; c < 3; c++) {
        lights.ambient[0][c] = .2f;
        lights.diffuse[0][c] = .25f;
        lights.specular[0][c] = .5f;
    }
    if(f->local) {
        lights.count=2;lights.authored_local=f->local==1;
        memset(lights.ambient,0,sizeof lights.ambient);
        memset(lights.diffuse,0,sizeof lights.diffuse);
        memset(lights.specular,0,sizeof lights.specular);
        lights.direction[1][2]=-1;
        lights.diffuse[1][0]=1;lights.ambient[1][0]=.1f;
        memset(view.sky_color,0,sizeof view.sky_color);
        memset(view.sun_color,0,sizeof view.sun_color);
    }
    r3d_material_shading(1);
    r3d_modern_lighting(mode);
    r3d_draw_lit(mesh, &view, &lights);
    r3d_end();
    r3d_end_target(pixels);
    r3d_mesh_destroy(mesh);
    return 1;
}

static float display(float x)
{
    float t = (x * (2.51f * x + .03f)) / (x * (2.43f * x + .59f) + .14f);
    t = fminf(1, fmaxf(0, t));
    return t <= .0031308f ? 12.92f * t : 1.055f * powf(t, 1.f / 2.4f) - .055f;
}
/* Evaluate the CPU BRDF with double precision, independently of the GLSL. */
static void expected(const fixture *f, float result[3])
{
    double v[3] = {-1. / SIZE, 1. / SIZE, -1}, length = sqrt(v[0] * v[0] + v[1] * v[1] + 1);
    for (int k = 0; k < 3; k++)
        v[k] /= length;
    double nv = .6 * v[0] - .8 * v[2],
           h[3] = {v[0], v[1] + (f->sun ? .6 : 0), v[2] - (f->sun ? .8 : 1)};
    length = sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
    for (int k = 0; k < 3; k++)
        h[k] /= length;
    double nh = .6 * h[0] - .8 * h[2], vh = v[0] * h[0] + v[1] * h[1] + v[2] * h[2];
    double a2 = pow(2. / (f->shine + 2), 2), nl = f->sun ? .64 : .8;
    double d = a2 / (3.141592653589793 * pow(nh * nh * (a2 - 1) + 1, 2));
    double smith = .5 / (nl * sqrt(nv * nv * (1 - a2) + a2) + nv * sqrt(nl * nl * (1 - a2) + a2));
    double fresnel = .04 * f->specular + (1 - .04 * f->specular) * pow(1 - vh, 5);
    double diffuse = f->kind == 4 ? 0 : (1 - fresnel) * .8 / 3.141592653589793;
    double scale = f->race ? 128. / 255 : 1;
    double direct = (diffuse * .25 + d * smith * fresnel * .5) * nl * scale * 3.141592653589793;
    const double sky[3] = {.815, .83, .84};
    for (int c = 0; c < 3; c++) {
        double ambient = f->kind == 4 ? 0 : .5 * .2 * sky[c] * scale;
        double trans[3] = {exp(-.0428 / .6), exp(-.1 / .6), exp(-.24414 / .6)};
        double luminance = trans[0] * .2126 + trans[1] * .7152 + trans[2] * .0722;
        double tint = f->sun ? trans[c] / luminance : 1;
        result[c] = display((float)(direct * tint + ambient)) * 255;
    }
}
/* Independent incident-direction quadrature for a white smooth coating. */
static float white_environment(float nv)
{
    const double a2=.0001;
    double sum=0;
    for(int y=0;y<4096;y++) {
        double nl=(y+.5)/4096.,lv=sqrt(1.-nl*nl),vx=sqrt(1.-nv*nv);
        for(int x=0;x<8192;x++) {
            double vh=sqrt(fmax(0.,(1.+lv*vx*cos((x+.5)*6.283185307179586/8192.)+nl*nv)*.5));
            double nh=(nl+nv)/(2*vh);
            double distribution=a2/(3.141592653589793*pow(nh*nh*(a2-1)+1,2));
            double visibility=.5/(nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2));
            double f=.04+.96*pow(1.-vh,5.);
            sum+=f*nl*distribution*visibility;
        }
    }
    return (float)(sum*6.283185307179586/(4096.*8192.));
}
int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    uint8_t original[65536], modern[65536], restored[65536];
    const int at = (64 * 128 + 64) * 4;
    int cases = 0, worst = 0;
    for (int sun = 0; sun < 2; sun++)
        for (int race = 0; race < 2; race++)
            for (int kind = 1; kind <= 4; kind += 3)
                for (int shine = 1; shine <= 16; shine *= 2)
                    for (int strength = 0; strength <= 2; strength++) {
                        fixture f = {.sun = sun,
                                     .kind = kind,
                                     .race = race,
                                     .shine = (float)shine,
                                     .specular = strength * .5f};
                        CHECK(sample(&f,0,original));
                        CHECK(sample(&f,argc > 1 && !strcmp(argv[1], "--negative") ? 0 : 1,modern));
                        float want[3];
                        expected(&f, want);
                        for (int c = 0; c < 3; c++) {
                            int error = abs((int)modern[at + c] - (int)floorf(want[c] + .5f));
                            if (error > worst)
                                worst = error;
                            CHECK(error<=2);
                        }
                        CHECK(modern[at+3]==original[at+3]);
                        CHECK(sample(&f,0,restored));
                        CHECK(!memcmp(original,restored,sizeof original));
                        cases++;
                    }
    fixture f = {.kind = 1, .shine = 8, .specular = 1, .unlit = 1};
    CHECK(sample(&f,0,original));
    CHECK(sample(&f,1,modern));
    CHECK(!memcmp(original,modern,sizeof original));
    printf("GGX analytic cases=%d max byte error=%d; unlit/alpha/Original restoration exact\n",
           cases, worst);
    /* Paint may have no authored direct-specular material: its reflection
       layer still needs a solar response, even with a black environment. */
    fixture coat = {.kind=2,.reflection=1,.coat=1};
    int coat_mode = argc>1 && !strcmp(argv[1],"--coat-negative") ? 0 : 1;
    CHECK(sample(&coat,coat_mode,modern));
    coat.coat=2;
    CHECK(sample(&coat,1,restored));
    printf("smooth coating direct sun / off-axis = %u/%u\n",modern[at],restored[at]);
    CHECK(modern[at]>220 && restored[at]<3);
    CHECK(modern[at+3]==restored[at+3]);
    coat.coat=3; CHECK(sample(&coat,1,modern));
    /* Independent double-precision direct-coat BRDF, away from saturation. */
    double v[3]={-1./SIZE,1./SIZE,-1},l[3]={.08,0,-sqrt(1-.08*.08)},hh[3];
    double vl=sqrt(v[0]*v[0]+v[1]*v[1]+1),hl=0;
    for(int k=0;k<3;k++){v[k]/=vl;hh[k]=v[k]+l[k];hl+=hh[k]*hh[k];}
    hl=sqrt(hl);for(int k=0;k<3;k++)hh[k]/=hl;
    double nv=-v[2],nl=-l[2],nh=-hh[2],vh=0;
    for(int k=0;k<3;k++)vh+=v[k]*hh[k];
    double distribution=.0001/(3.141592653589793*pow(nh*nh*(.0001-1)+1,2));
    double visibility=.5/(nl*sqrt(nv*nv*(1-.0001)+.0001)+nv*sqrt(nl*nl*(1-.0001)+.0001));
    double fresnel=.04+.96*pow(1-vh,5);
    float want_coat=display((float)(.64*distribution*visibility*fresnel*nl*3.141592653589793))*255;
    printf("direct coating analytic=%u expected=%.2f\n",modern[at],want_coat);
    CHECK(fabsf(modern[at]-want_coat)<=2);
    coat.coat=1;CHECK(sample(&coat,0,original));
    CHECK(sample(&coat,0,restored));CHECK(!memcmp(original,restored,sizeof original));
    fixture top = {.kind = 1, .hemispheric = 1}, bottom = {.kind = 1, .hemispheric = -1};
    int hemisphere_mode = argc > 1 && !strcmp(argv[1], "--hemisphere-negative") ? 0 : 1;
    CHECK(sample(&top,hemisphere_mode,modern));
    CHECK(sample(&bottom,hemisphere_mode,restored));
    printf("sky hemisphere up/down=%d/%d\n", modern[at], restored[at]);
    CHECK(modern[at]>restored[at]+10);
    fixture reflected = {.kind = 1, .reflection = 1, .dielectric = 1};
    int reflection_mode = argc > 1 && !strcmp(argv[1], "--reflection-negative") ? 0 : 1;
    CHECK(sample(&reflected,reflection_mode,modern));
    double cosine = (.6 * (-1. / SIZE) + .8) / sqrt(1. + 2. / (SIZE * SIZE));
    float reflect_expected = display(.64f * white_environment((float)cosine)) * 255;
    printf("dielectric reflection normal=%d expected=%.2f\n", modern[at], reflect_expected);
    CHECK(fabsf(modern[at]-reflect_expected)<=2);
    reflected.dielectric = 2;
    CHECK(sample(&reflected,reflection_mode,restored));
    printf("dielectric grazing=%d\n", restored[at]);
    CHECK(restored[at]>modern[at]+3);
    CHECK(restored[at]<230); /* dielectric grazing reflection remains bounded */
    float grazing_cosine=(.1f-.9949874f/SIZE)/sqrtf(1.f+2.f/(SIZE*SIZE));
    float grazing_expected=display(.64f*white_environment(grazing_cosine))*255.f;
    printf("grazing cosine %.6f expected %.2f\n",grazing_cosine,grazing_expected);
    CHECK(fabsf(restored[at]-grazing_expected)<4);
    fixture smooth={.kind=1,.reflection=1,.daylight=1,.stripes=1,.shine=1000000}, rough=smooth;
    rough.shine=0;
    if(argc>1 && !strcmp(argv[1],"--environment-negative"))rough.reflection=0;
    CHECK(sample(&smooth,1,modern));CHECK(sample(&rough,1,restored));
    double smooth_variance=0,rough_variance=0,sm=0,rm=0;
    int count=0;
    for(int y=32;y<96;y++)for(int x=32;x<96;x++){int at=(y*SIZE+x)*4;sm+=modern[at];rm+=restored[at];count++;}
    sm/=count;rm/=count;
    for(int y=32;y<96;y++)for(int x=32;x<96;x++){int at=(y*SIZE+x)*4;smooth_variance+=pow(modern[at]-sm,2);rough_variance+=pow(restored[at]-rm,2);}
    printf("environment stripe variance smooth/rough=%.4f/%.4f\n",smooth_variance/count,rough_variance/count);
    /* The legacy highlight exponent is not clearcoat roughness. */
    CHECK(!memcmp(modern,restored,sizeof modern));
    CHECK(smooth_variance/count>1.);
    SDL_setenv("MOON_REFLECTION_REFERENCE","1",1);
    CHECK(sample(&rough,1,original));
    SDL_setenv("MOON_REFLECTION_REFERENCE","",1);
    CHECK(sample(&rough,argc>1 && !strcmp(argv[1],"--cache-negative")?0:1,modern));
    int cache_error=0;
    for(unsigned i=0;i<sizeof original;i++){int e=abs((int)original[i]-modern[i]);if(e>cache_error)cache_error=e;}
    printf("linear environment cache max byte error=%d\n",cache_error);CHECK(cache_error<=2);
    fixture local={.kind=1,.daylight=1,.local=1};
    if(argc>1 && !strcmp(argv[1],"--local-negative"))local.local=2;
    CHECK(sample(&local,1,modern));
    printf("local authored lamp RGB=%u/%u/%u\n",modern[at],modern[at+1],modern[at+2]);
    CHECK(modern[at]>80 && modern[at+1]<3 && modern[at+2]<3);
    local.sun=1;
    CHECK(sample(&local,1,restored));
    CHECK(!memcmp(modern,restored,sizeof modern));
    CHECK(sample(&local,0,original));
    local.local=2;CHECK(sample(&local,0,restored));
    CHECK(!memcmp(original,restored,sizeof original));
    /* Neutral diffuse surfaces must receive the same solar/sky irradiance
       irrespective of the legacy course/car submission path. */
    int integration_mode = argc > 1 && !strcmp(argv[1], "--integration-negative") ? 0 : 1;
    for (int direct = 0; direct < 2; direct++) {
        fixture car = {.kind = 1, .daylight = direct + 1};
        fixture course = car;
        course.course = 1;
        CHECK(sample(&car, integration_mode, modern));
        CHECK(sample(&course, 1, restored));
        float energy = direct ? .8f : .330863143f * (.12f + .88f * .5f);
        float target = display(energy) * 255.f;
        printf("shared daylight direct=%d car/course=%d/%d target=%.2f\n", direct, modern[at],
               restored[at], target);
        CHECK(abs(modern[at] - restored[at]) <= 2);
        CHECK(fabsf(modern[at] - target) <= 2);
    }
    fixture sunlight={.kind=1,.daylight=2};
    CHECK(sample(&sunlight,1,modern));
    CHECK(abs(modern[at]-modern[at+1])<=1 && abs(modern[at+1]-modern[at+2])<=1);
    float overhead[3]={0,1,0},low[3]={.98f,.2f,0};
    moon_daylight noon=moon_daylight_authored(argc>1 && !strcmp(argv[1],"--warm-negative")?low:overhead,0);
    moon_daylight dawn=moon_daylight_authored(low,0);
    const float absent[3]={0,0,0};moon_daylight missing=moon_daylight_authored(absent,0);
    CHECK(missing.sun[0]==0 && missing.sun[1]==0 && missing.direct[0]==0);
    CHECK(fabsf(noon.direct[0]-noon.direct[2])<.01f);
    CHECK(dawn.direct[0]>dawn.direct[1] && dawn.direct[1]>dawn.direct[2]);
    CHECK(dawn.sun[0]>.9f && dawn.sun[1]>.19f);
    fixture shaded={.kind=1,.daylight=1}, illuminated={.kind=1,.daylight=3};
    shaded.overfill=illuminated.overfill=argc>1 && !strcmp(argv[1],"--contrast-negative");
    CHECK(sample(&shaded,1,modern));CHECK(sample(&illuminated,1,restored));
    /* Recover linear energies from the display values, then reject sky fill
       that erases more than 70 percent of the direct-light contrast. */
    float recovered[2];
    for(int j=0;j<2;j++) {
        float lo=0,hi=4,target=(j?restored[at]:modern[at])/255.f;
        for(int i=0;i<30;i++){float mid=(lo+hi)*.5f;if(display(mid)<target)lo=mid;else hi=mid;}
        recovered[j]=(lo+hi)*.5f;
    }
    printf("clear-day shadow/lit radiance=%.5f/%.5f\n",recovered[0],recovered[1]);
    CHECK(recovered[0]/recovered[1]<.3f);
    fixture paint = {.kind = 1, .daylight = 1, .paint = 1};
    CHECK(sample(&paint, argc>1 && !strcmp(argv[1],"--paint-negative")?0:1, modern));
    const float albedo[3] = {.08f, .12f, .6f};
    for (int c = 0; c < 3; c++) {
        float target = display(albedo[c] * .330863143f * (.12f + .88f * .5f)) * 255;
        printf("sky-only paint channel %d=%d target=%.2f\n", c, modern[at + c], target);
        CHECK(fabsf(modern[at+c]-target)<=2);
    }
    fixture halftex = {.kind=1,.daylight=1,.halftex=1};
    CHECK(sample(&halftex,argc>1 && !strcmp(argv[1],"--texture-negative")?0:1,modern));
    float texture_target=display(.21404114f*.330863143f*(.12f+.88f*.5f))*255;
    printf("GS half-range albedo=%d target=%.2f\n",modern[at],texture_target);
    CHECK(fabsf(modern[at]-texture_target)<=2);
    plat_video_shutdown();
    return 0;
}
