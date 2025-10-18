#include <math.h>
#include <stdio.h>
#define WIDTH 1920
#define HEIGHT 1080
#define MAX_STEPS 100
#define MAX_DIST 100.0
#define SURF_DIST 0.001

typedef struct { float x, y, z; } vec3;
typedef struct { float r, g, b; } color;

vec3 add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
vec3 sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
vec3 mul(vec3 a, float s){ return (vec3){a.x*s, a.y*s, a.z*s}; }
float dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float length_vec(vec3 a) { return sqrt(a.x*a.x + a.y*a.y + a.z*a.z); }
vec3 norm(vec3 a){ float l=length_vec(a); return mul(a,1.0f/l); }
float max_f(float a, float b) { return a > b ? a : b; }
float min_f(float a, float b) { return a < b ? a : b; }

// SDF for sphere
float sphereSDF(vec3 p, float r) {
    return length_vec(p) - r;
}

// SDF for box
float boxSDF(vec3 p, vec3 b) {
    vec3 q = {fabs(p.x) - b.x, fabs(p.y) - b.y, fabs(p.z) - b.z};
    float dx = max_f(q.x, 0.0);
    float dy = max_f(q.y, 0.0);
    float dz = max_f(q.z, 0.0);
    return sqrt(dx*dx + dy*dy + dz*dz) + min_f(max_f(q.x, max_f(q.y, q.z)), 0.0);
}

// SDF for cylinder
float cylinderSDF(vec3 p, float r, float h) {
    float dx = sqrt(p.x*p.x + p.z*p.z) - r;
    float dy = fabs(p.y) - h;
    float outside = sqrt(max_f(dx,0)*max_f(dx,0) + max_f(dy,0)*max_f(dy,0));
    float inside = min_f(max_f(dx, dy), 0.0);
    return outside + inside;
}

// SDF for plane (checkered floor)
float planeSDF(vec3 p) {
    return p.y + 1.0; // plane at y = -1
}

// Scene SDF with multiple objects
float sceneSDF(vec3 p, int *objID) {
    // Green Cylinder (left, forward)
    vec3 p1 = sub(p, (vec3){-2.0, 0.5, 1.0});
    float cyl = cylinderSDF(p1, 0.7, 1.5);
    
    // Red/Brown Sphere (center, behind box)
    vec3 p2 = sub(p, (vec3){0.3, -0.2, -1.5});
    float sph = sphereSDF(p2, 0.9);
    
    // Blue Box (right, forward)
    vec3 p3 = sub(p, (vec3){2.2, -0.2, 0.5});
    float box = boxSDF(p3, (vec3){0.85, 0.85, 0.85});
    
    // Checkered floor
    float plane = planeSDF(p);
    
    // Find closest object
    float minDist = cyl;
    *objID = 1; // cylinder
    
    if (sph < minDist) { minDist = sph; *objID = 2; } // sphere
    if (box < minDist) { minDist = box; *objID = 3; } // box
    if (plane < minDist) { minDist = plane; *objID = 4; } // floor
    
    return minDist;
}

vec3 getNormal(vec3 p) {
    int dummy;
    float d = sceneSDF(p, &dummy);
    float eps = 0.001;
    vec3 n = {
        sceneSDF((vec3){p.x+eps, p.y, p.z}, &dummy) - d,
        sceneSDF((vec3){p.x, p.y+eps, p.z}, &dummy) - d,
        sceneSDF((vec3){p.x, p.y, p.z+eps}, &dummy) - d
    };
    return norm(n);
}

vec3 rayDirection(float fov, int x, int y) {
    float aspect = (float)WIDTH / HEIGHT;
    float zoom = 1.4; // zoom factor - higher value = wider view
    float px = (2 * ((x + 0.5) / WIDTH) - 1) * tan(fov / 2 * M_PI / 180) * aspect * zoom;
    float py = (1 - 2 * ((y + 0.5) / HEIGHT)) * tan(fov / 2 * M_PI / 180) * zoom;
    return norm((vec3){px, py, -1});
}

// Reflect vector around normal
vec3 reflect_vec(vec3 I, vec3 N) {
    return sub(I, mul(N, 2.0 * dot(N, I)));
}

// Soft shadow calculation
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;
    for (int i = 0; i < 32; i++) {
        int dummy;
        vec3 p = add(ro, mul(rd, t));
        float h = sceneSDF(p, &dummy);
        if (h < SURF_DIST) return 0.0;
        res = min_f(res, k * h / t);
        t += h;
        if (t >= maxt) break;
    }
    return res;
}

float raymarch(vec3 ro, vec3 rd, int *objID) {
    float dist = 0.0;
    for (int i=0; i<MAX_STEPS; i++) {
        vec3 p = add(ro, mul(rd, dist));
        float d = sceneSDF(p, objID);
        if (d < SURF_DIST) return dist;
        dist += d;
        if (dist > MAX_DIST) break;
    }
    return -1.0;
}

color getColor(vec3 p, int objID, vec3 normal, vec3 lightDir, vec3 viewDir, vec3 reflectColor, float shadow) {
    color col = {0, 0, 0};
    float shininess = 0.0;
    float reflectivity = 0.0;
    
    // Object colors and material properties
    if (objID == 1) { // Green cylinder - glossy
        col = (color){0.2, 0.6, 0.3};
        shininess = 0.6;
        reflectivity = 0.4;
    } else if (objID == 2) { // Red/brown sphere - very shiny
        col = (color){0.7, 0.3, 0.3};
        shininess = 0.8;
        reflectivity = 0.5;
    } else if (objID == 3) { // Blue box - glossy
        col = (color){0.3, 0.3, 0.7};
        shininess = 0.7;
        reflectivity = 0.45;
    } else if (objID == 4) { // Checkered floor - reflective
        float scale = 1.0;
        int cx = (int)floor(p.x / scale);
        int cz = (int)floor(p.z / scale);
        if ((cx + cz) % 2 == 0) {
            col = (color){0.8, 0.8, 0.8};
        } else {
            col = (color){0.5, 0.2, 0.2};
        }
        shininess = 0.9;
        reflectivity = 0.3;
    }
    
    // Very low ambient light (dark room)
    float ambient = 0.03;
    
    // Diffuse lighting (affected by shadow)
    float diff = max_f(dot(normal, lightDir), 0.0) * shadow;
    
    // Specular lighting (Blinn-Phong) - also affected by shadow
    vec3 halfDir = norm(add(lightDir, mul(viewDir, -1)));
    float spec = pow(max_f(dot(normal, halfDir), 0.0), 32.0) * shininess * shadow;
    
    // Combine ambient, diffuse, specular, and reflection
    col.r = col.r * (ambient + diff * 0.6) + spec * 0.8 + reflectColor.x * reflectivity;
    col.g = col.g * (ambient + diff * 0.6) + spec * 0.8 + reflectColor.y * reflectivity;
    col.b = col.b * (ambient + diff * 0.6) + spec * 0.8 + reflectColor.z * reflectivity;
    
    // Clamp values
    col.r = min_f(col.r, 1.0);
    col.g = min_f(col.g, 1.0);
    col.b = min_f(col.b, 1.0);
    
    return col;
}

int main() {
    FILE *f = fopen("out.ppm", "w");
    fprintf(f, "P3\n%d %d\n255\n", WIDTH, HEIGHT);
    
    vec3 ro = {0, 1.2, 5}; // camera position - higher up
    vec3 lightDir = norm((vec3){0.5, 1, 0.5}); // light direction
    
    for (int y=0; y<HEIGHT; y++) {
        for (int x=0; x<WIDTH; x++) {
            vec3 rd = rayDirection(60, x, y);
            int objID = 0;
            float t = raymarch(ro, rd, &objID);
            
            color col = {0, 0, 0}; // black background
            
            if (t > 0) {
                vec3 hitPoint = add(ro, mul(rd, t));
                vec3 normal = getNormal(hitPoint);
                vec3 viewDir = rd;
                
                // Calculate shadow
                vec3 shadowOrigin = add(hitPoint, mul(normal, SURF_DIST * 2));
                float shadow = softShadow(shadowOrigin, lightDir, 0.02, 10.0, 8.0);
                
                // Calculate reflection
                vec3 reflectDir = reflect_vec(rd, normal);
                int reflectObjID = 0;
                float reflectT = raymarch(add(hitPoint, mul(normal, SURF_DIST * 2)), reflectDir, &reflectObjID);
                
                vec3 reflectColor = {0.02, 0.02, 0.03}; // very dark sky color
                if (reflectT > 0) {
                    vec3 reflectHitPoint = add(add(hitPoint, mul(normal, SURF_DIST * 2)), mul(reflectDir, reflectT));
                    vec3 reflectNormal = getNormal(reflectHitPoint);
                    
                    // Shadow for reflected surface
                    vec3 reflectShadowOrigin = add(reflectHitPoint, mul(reflectNormal, SURF_DIST * 2));
                    float reflectShadow = softShadow(reflectShadowOrigin, lightDir, 0.02, 10.0, 8.0);
                    
                    // Simple lighting for reflected color (darker)
                    float reflectDiff = max_f(dot(reflectNormal, lightDir), 0.0) * reflectShadow * 0.6 + 0.03;
                    
                    if (reflectObjID == 1) {
                        reflectColor = (vec3){0.2 * reflectDiff, 0.6 * reflectDiff, 0.3 * reflectDiff};
                    } else if (reflectObjID == 2) {
                        reflectColor = (vec3){0.7 * reflectDiff, 0.3 * reflectDiff, 0.3 * reflectDiff};
                    } else if (reflectObjID == 3) {
                        reflectColor = (vec3){0.3 * reflectDiff, 0.3 * reflectDiff, 0.7 * reflectDiff};
                    } else if (reflectObjID == 4) {
                        float scale = 1.0;
                        int cx = (int)floor(reflectHitPoint.x / scale);
                        int cz = (int)floor(reflectHitPoint.z / scale);
                        if ((cx + cz) % 2 == 0) {
                            reflectColor = (vec3){0.8 * reflectDiff, 0.8 * reflectDiff, 0.8 * reflectDiff};
                        } else {
                            reflectColor = (vec3){0.5 * reflectDiff, 0.2 * reflectDiff, 0.2 * reflectDiff};
                        }
                    }
                }
                
                col = getColor(hitPoint, objID, normal, lightDir, viewDir, reflectColor, shadow);
            }
            
            int r = (int)(col.r * 255);
            int g = (int)(col.g * 255);
            int b = (int)(col.b * 255);
            
            fprintf(f, "%d %d %d ", r, g, b);
        }
        fprintf(f, "\n");
    }
    
    fclose(f);
    return 0;
}