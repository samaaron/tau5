// Tau5 Loading Screen Fragment Shader

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform float time;
uniform vec2 resolution;
uniform sampler2D logoTexture;
uniform float fadeValue;

#define PI 3.14159265359

mat2 rot(float a) {
  float s = sin(a);
  float c = cos(a);
  return mat2(c, -s, s, c);
}

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 toGamma(vec3 col) {
  return pow(col, vec3(0.454545));
}

vec3 warpEffect(vec2 p, float t) {
  vec3 col = vec3(0.0);
  float centerDist = length(p);
  if(centerDist < 0.35) return col;
  
  vec3 ray = vec3(p * 2.0, 1.0);
  
  float clampedTime = min(t, 10.0);
  
  float warpActivation = smoothstep(0.0, 2.0, clampedTime * 0.35);
  float continuousAccel = clampedTime * 0.1;
  float speedMultiplier = 0.5 + warpActivation * 6.5 + continuousAccel * continuousAccel * 2.5;
  
  float offset;
  if (t <= 10.0) {
    offset = t * 0.02 * speedMultiplier;
  } else {
    float finalSpeed = 0.02 * (0.5 + smoothstep(0.0, 2.0, 3.5) * 6.5 + 1.0 * 2.5);
    float offsetAt10 = 10.0 * finalSpeed;
    offset = offsetAt10 + (t - 10.0) * finalSpeed;
  }
  
  float speed2 = warpActivation * 0.6;
  float speed = 0.1 + warpActivation * 0.5;
  offset += sin(offset) * 0.1;
  
  vec3 stp = ray / max(abs(ray.x), abs(ray.y));
  // Scale up the stepping to avoid precision issues
  vec3 pos = 4.0 * stp + 0.5;
  
  vec3 deepPinkColor = vec3(0.9, 0.1, 0.5);
  vec3 blueColor = vec3(0.1, 0.4, 1.0);
  float centerFade = smoothstep(0.35, 0.5, centerDist);
  
  // Fixed iterations for mobile compatibility
  // Use weight to control visibility instead of dynamic iteration count
  float iterationWeight = (12.0 + min(8.0, continuousAccel * 20.0)) / 20.0;
  float brightnessBoost = 1.0 + min(1.5, continuousAccel);
  
  for(int i = 0; i < 20; i++) {
    // Control visibility with weight instead of breaking
    float weight = 1.0 - step(iterationWeight * 20.0, float(i));
    
    // Separate floor and fract calculations for better precision
    vec2 posFloor = floor(pos.xy);
    vec2 posFract = pos.xy - posFloor;
    
    float z = hash(posFloor);
    z = fract(z - offset);
    float d = 30.0 * z - pos.z;
    
    // Check if star is in range (avoid continue statement for compatibility)
    float inRange = 1.0 - step(1.0, step(31.0, d) + step(d, -1.0));
    
    // Star shape calculation with explicit fract calculation
    float starDist = max(0.0, 1.0 - 8.0 * length(posFract - 0.5));
    float w = starDist * starDist;
    
    // Color streaks with proper RGB separation for motion blur effect
    vec3 c = max(vec3(0.0), vec3(
      1.0 - abs(d + speed2 * 0.5) / speed,
      1.0 - abs(d) / speed,
      1.0 - abs(d - speed2 * 0.5) / speed
    ));
    
    vec3 starColor = mix(deepPinkColor, blueColor, 0.5 + 0.5 * sin(z * PI));
    c *= starColor;
    
    col += inRange * weight * brightnessBoost * (1.0 - z) * c * w * centerFade;
    
    pos += stp;
  }
  
  return toGamma(col) * 0.6;
}

vec3 cubeWireframe(vec2 p, float logoMask) {
  vec3 col = vec3(0.0);
  float rotSpeed = mix(0.05, 0.15, logoMask);
  float t = time * rotSpeed;
  vec3 angles = vec3(t, t * 0.7, t * 0.3);
  
  // Mobile-compatible vertex definition
  float scale = mix(0.3, 0.35, logoMask);
  vec2 proj[8];
  
  // Define cube vertices inline for mobile compatibility
  for(int i = 0; i < 8; i++) {
    vec3 v;
    // Manually set each vertex based on index
    if(i == 0) v = vec3(-1.0, -1.0, -1.0);
    else if(i == 1) v = vec3( 1.0, -1.0, -1.0);
    else if(i == 2) v = vec3( 1.0,  1.0, -1.0);
    else if(i == 3) v = vec3(-1.0,  1.0, -1.0);
    else if(i == 4) v = vec3(-1.0, -1.0,  1.0);
    else if(i == 5) v = vec3( 1.0, -1.0,  1.0);
    else if(i == 6) v = vec3( 1.0,  1.0,  1.0);
    else v = vec3(-1.0,  1.0,  1.0); // i == 7
    
    v.yz *= rot(angles.x);
    v.xz *= rot(angles.y);
    v.xy *= rot(angles.z);
    proj[i] = v.xy * (2.0 / (4.0 + v.z)) * scale;
  }
  
  float d = 10000.0;
  vec2 e;
  
  e = proj[1] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
  e = proj[2] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
  e = proj[3] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
  e = proj[0] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
  e = proj[5] - proj[4]; d = min(d, length(p - proj[4] - e * clamp(dot(p - proj[4], e) / dot(e, e), 0.0, 1.0)));
  e = proj[6] - proj[5]; d = min(d, length(p - proj[5] - e * clamp(dot(p - proj[5], e) / dot(e, e), 0.0, 1.0)));
  e = proj[7] - proj[6]; d = min(d, length(p - proj[6] - e * clamp(dot(p - proj[6], e) / dot(e, e), 0.0, 1.0)));
  e = proj[4] - proj[7]; d = min(d, length(p - proj[7] - e * clamp(dot(p - proj[7], e) / dot(e, e), 0.0, 1.0)));
  e = proj[4] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
  e = proj[5] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
  e = proj[6] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
  e = proj[7] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
  
  col += mix(vec3(1.0, 0.65, 0.0), vec3(0.0, 1.0, 0.0), logoMask) * smoothstep(0.02, 0.0, d) * 2.0;
  return col;
}

void main() {
  vec2 uv = gl_FragCoord.xy / resolution.xy;
  vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
  
  vec3 col = mix(vec3(0.02, 0.0, 0.05), vec3(0.05, 0.0, 0.1), uv.y * 0.5);
  col += warpEffect(p, time);
  col += vec3(0.05, 0.02, 0.1) * (1.0 - smoothstep(0.0, 1.5, length(p))) * 0.3;
  
  vec2 logoUV = p + 0.5;
  // Flip Y coordinate for WebGL (OpenGL and WebGL have opposite Y axes)
  #ifdef GL_ES
  logoUV.y = 1.0 - logoUV.y;
  #endif
  // Sample texture unconditionally to avoid dynamic texture sampling
  vec4 logoColor = texture2D(logoTexture, logoUV);
  float lum = dot(logoColor.rgb, vec3(0.299, 0.587, 0.114));
  
  // Check if we're in valid UV range
  float validUV = step(0.0, logoUV.x) * step(logoUV.x, 1.0) * step(0.0, logoUV.y) * step(logoUV.y, 1.0);
  
  // Calculate logo mask without ternary operator
  float logoMask = validUV * step(0.5, logoColor.a) * (1.0 - step(0.5, lum));
  
  col += cubeWireframe(p, logoMask);
  // Invert colors based on logo mask without if statement
  col = mix(col, 1.0 - col, step(0.5, logoMask));
  col *= 1.0 + length(p) * 0.7;
  
  col *= (1.0 - fadeValue);
  
  gl_FragColor = vec4(col, 1.0);
}