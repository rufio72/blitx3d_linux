precision highp float;
precision highp int;

uniform mat4 bbProjMatrix;
uniform mat4 bbViewMatrix;
uniform mat4 bbWorldMatrix;
uniform mat3 bbNormalMatrix; // Optimization #4
uniform vec3 bbCameraPos;    // Camera world position for reflections

// Shadow mapping (perspective, one shadow-casting light). bbShadowLight is the
// index into LS.Light of the light that casts; -1 disables (default) -> no change.
uniform mat4 bbLightViewProj; // world space -> shadow light clip space
uniform int  bbShadowLight;   // index of shadow-casting light, -1 = none

uniform sampler2D bbTexture[8];
uniform samplerCube bbTextureCube[8];
uniform sampler2DShadow bbShadowMap;

#define FOG_NONE   0
#define FOG_LINEAR 1

struct BBLightData {
  mat4 TForm;
  vec4 Color;
  vec4 Position;  // xyz = world position, w = type (1=directional, 2=point, 3=spot)
  vec4 Params;    // x = range, y = inner_cone, z = outer_cone, w = unused
  vec4 Direction; // xyz = world-space direction, precomputed on the CPU
} ;

layout(std140) uniform BBLightState {
  BBLightData Light[8];

  int LightsUsed;
} LS;

// must be mindful of alignment when ordering...
struct BBTextureState {
  mat4 TForm;
  int Blend,SphereMap,Flags,CubeMap;
} ;

layout(std140) uniform BBRenderState {
  vec4 Ambient;
  vec4 BrushColor;
  vec4 FogColor;

  BBTextureState Texture[8];

  vec2 FogRange;

  int TexturesUsed;
  int UseVertexColor;
  float BrushShininess;
  int FullBright;
  int FogMode;
  int AlphaTest;
} RS;

// GPU skinning: world-space bone matrices; bbSkinCount==0 disables.
// 64 mat4 = 4KB, well within the 16KB minimum UBO size.
layout(std140) uniform BBBoneState {
  mat4 Bone[64];
} BS;

uniform int bbSkinCount;

#ifdef VERTEX
#define varying out
#else
#define varying in
#endif

varying vec3 bbVertex_Position;
varying vec3 bbVertex_WorldPosition;
varying vec3 bbVertex_WorldNormal;
varying vec4 bbVertex_Color;
varying vec3 bbVertex_Normal;
varying vec2 bbVertex_TexCoord[8];
varying float bbVertex_FogFactor;
varying vec3 bbVertex_ViewPos;   // view-space position (per-pixel lighting)

float fogFactorLinear(
  const float dist,
  const float start,
  const float end
) {
  return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}

vec2 sphereMap(in vec3 normal, in vec3 ecPosition3){
  float m;
  vec3 r, u;
  u = normalize(ecPosition3);
  r = reflect(u, normal);
  m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
  return vec2 (r.x / m + 0.5, r.y / m + 0.5);
}




#ifdef VERTEX
/**
 * VERTEX
 **/

layout(location = 0) in vec3 bbPosition;
layout(location = 1) in vec3 bbNormal;
layout(location = 2) in vec4 bbColor;
layout(location = 3) in vec2 bbTexCoord0;
layout(location = 4) in vec2 bbTexCoord1;
layout(location = 5) in vec4 bbBoneWeights;
layout(location = 6) in vec4 bbBoneIndices;

void main() {
  mat4 bbModelViewMatrix = bbViewMatrix * bbWorldMatrix;
  mat4 bbModelViewProjectionMatrix=bbProjMatrix * bbModelViewMatrix;
  // Optimization #4: bbNormalMatrix is now passed as uniform from CPU

  // GPU skinning: vertices are stored in bind pose, bone matrices map
  // straight to world space (the world matrix is identity for skinned
  // models, which render in RENDER_SPACE_WORLD)
  vec3 position = bbPosition;
  vec3 normal = bbNormal;
  if (bbSkinCount > 0) {
    vec3 p = vec3(0.0);
    vec3 n = vec3(0.0);
    for (int i = 0; i < 4; i++) {
      float w = bbBoneWeights[i];
      if (w <= 0.0) break;
      mat4 b = BS.Bone[int(bbBoneIndices[i] + 0.5)];
      p += w * (b * vec4(bbPosition, 1.0)).xyz;
      n += w * (mat3(b) * bbNormal);
    }
    position = p;
    normal = normalize(n);
  }

  bbVertex_Position = position;

  // Calculate world space position and normal for cubemap reflections
  vec4 worldPos = bbWorldMatrix * vec4(position, 1.0);
  bbVertex_WorldPosition = worldPos.xyz;
  bbVertex_WorldNormal = normalize(mat3(bbWorldMatrix) * normal);

  gl_Position = bbModelViewProjectionMatrix * vec4(position, 1.0);

  vec3 EyeNormal = bbNormalMatrix * normal;

  for( int i=0;i<RS.TexturesUsed;i++ ){
    vec2 coord;
    if( RS.Texture[i].SphereMap==1 ) {
      coord=sphereMap( EyeNormal,gl_Position.xyz/gl_Position.w );
    }else if( RS.Texture[i].Flags==1 ) {
      coord=bbTexCoord1;
    }else {
      coord=bbTexCoord0;
    }
    bbVertex_TexCoord[i] = (RS.Texture[i].TForm * vec4(coord, 0.0, 1.0)).xy;
  }


  vec4 bbMaterialColor;
  if( RS.UseVertexColor>0 ){
    bbMaterialColor = bbColor;
  }else{
    bbMaterialColor = RS.BrushColor;
  }

  // Always pass normal for cubemap reflections (even with fullbright)
  bbVertex_Normal = normalize( EyeNormal );

  // Per-pixel lighting: the light loop now lives in the FRAGMENT shader, so the
  // vertex shader only forwards the UNLIT material color and the view-space
  // position (the view-space normal is already in bbVertex_Normal). This was
  // Gouraud/per-vertex before; per-pixel is needed for smooth wall lighting and
  // for normal/bump mapping.
  bbVertex_ViewPos = (bbModelViewMatrix * vec4(position, 1.0)).xyz;
  bbVertex_Color = bbMaterialColor;

  switch( RS.FogMode ){
  case FOG_NONE: break;
  case FOG_LINEAR: bbVertex_FogFactor=fogFactorLinear( length( gl_Position.xyz ),RS.FogRange.x,RS.FogRange.y );break;
  }
}
#endif

#ifdef FRAGMENT
/**
 * FRAGMENT
 **/

out vec4 bbFragColor;

#define BLEND_REPLACE   0
#define BLEND_ALPHA     1
#define BLEND_MULTIPLY  2
#define BLEND_ADD       3
#define BLEND_DOT3      4
#define BLEND_MULTIPLY2 5

vec4 Sample2D( sampler2D tex,int i ){
  return texture( tex,bbVertex_TexCoord[i] );
}

#define CUBEMODE_REFLECTION 1
#define CUBEMODE_NORMAL     2
#define CUBEMODE_POSITION   3
#define CUBEMODE_WATER      4

vec4 SampleCube( samplerCube tex,int mode ){
  // View direction: from camera to fragment (both in Blitz world space)
  vec3 I = normalize(bbVertex_WorldPosition - bbCameraPos);

  // Normal in world space, forced to face the viewer (FlipMesh can invert it)
  vec3 N = normalize(bbVertex_WorldNormal);
  if( dot(N, I) > 0.0 ) N = -N;

  // Lookup direction per cube mode (SetCubeMode)
  vec3 R;
  if( mode == CUBEMODE_NORMAL ){
    R = N;
  }else if( mode == CUBEMODE_POSITION ){
    R = normalize(bbVertex_WorldPosition);
  }else{
    // CUBEMODE_REFLECTION / CUBEMODE_WATER
    R = reflect(I, N);
  }

  // Cube faces are stored via SetCubeFace/CopyRect with the up/down views in the
  // -Y/+Y faces and images copied 1:1 from the framebuffer; with that layout the
  // Blitz-space lookup direction only needs Y negated (verified with markers).
  R.y = -R.y;

  vec4 refl = texture(tex, R);

  if( mode != CUBEMODE_WATER ) return refl;

  // Fresnel (Schlick-like): full reflection at grazing angles, darker water color
  // looking straight down. Local wave slope changes the angle, so ripples shade.
  float cosTheta = clamp(dot(-I, N), 0.0, 1.0);
  float fresnel = 0.15 + 0.85 * pow(1.0 - cosTheta, 3.0);
  vec3 deep = RS.FogColor.rgb * 0.25;
  vec3 col = mix(deep, refl.rgb, fresnel);

  // Specular glints: Blinn-Phong sparkle from point/spot lights on wave crests.
  for( int i=0;i<LS.LightsUsed;i++ ){
    if( int(LS.Light[i].Position.w)==1 ) continue; // skip directional
    vec3 Lv = LS.Light[i].Position.xyz - bbVertex_WorldPosition;
    float dist = length(Lv);
    vec3 L = Lv / dist;
    float att = 1.0;
    float range = LS.Light[i].Params.x;
    if( range > 0.0 ){
      att = clamp(1.0 - dist/range, 0.0, 1.0);
      att = att * att;
    }
    vec3 H = normalize(L - I);
    float spec = pow(max(dot(N, H), 0.0), 200.0);
    col += clamp(LS.Light[i].Color.rgb, 0.0, 1.0) * spec * att * 2.0;
  }

  return vec4(col, refl.a);
}

vec4 Blend( vec4 t0,vec4 t1,int i ){
  switch( RS.Texture[i].Blend ){
  default:
  case BLEND_REPLACE:  return t0;
  case BLEND_ALPHA:    return mix(t0, t1, t1.a);
  case BLEND_MULTIPLY: return t0*t1;
  case BLEND_ADD:      return t0+t1;
  }
}

// Per-fragment tangent frame from screen-space derivatives (Christian Schueler,
// http://www.thetenthplanet.de/archives/1180). Lets a tangent-space normal map
// perturb the normal WITHOUT per-vertex tangents: T/B are derived from how the
// view-space position p and the uv change between neighbouring pixels. N is the
// (geometric) eye-space normal, the resulting frame maps tangent space -> eye.
mat3 cotangentFrame( vec3 N,vec3 p,vec2 uv ){
  vec3 dp1 = dFdx( p ),  dp2 = dFdy( p );
  vec2 du1 = dFdx( uv ), du2 = dFdy( uv );
  vec3 dp2perp = cross( dp2,N );
  vec3 dp1perp = cross( N,dp1 );
  vec3 T = dp2perp*du1.x + dp1perp*du2.x;
  vec3 B = dp2perp*du1.y + dp1perp*du2.y;
  float invmax = inversesqrt( max( dot(T,T),dot(B,B) ) );
  return mat3( T*invmax,B*invmax,N );
}

// Perspective shadow map test for the shadow-casting light. Returns 1 = lit,
// 0 = fully shadowed, with 3x3 PCF for soft edges. Fragments outside the light
// frustum (or beyond its far plane) are treated as lit, so the rest of the scene
// is unaffected. worldPos = fragment position in Blitz world space.
float shadowFactor( vec3 worldPos ){
  vec4 lp = bbLightViewProj * vec4( worldPos,1.0 );
  if( lp.w <= 0.0 ) return 1.0;                 // behind the light
  vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;        // NDC -> [0,1]
  if( proj.z >= 1.0 ) return 1.0;               // beyond far plane
  if( proj.x < 0.0 || proj.x > 1.0 ||
      proj.y < 0.0 || proj.y > 1.0 ) return 1.0; // outside the cone
  float bias = 0.0015;
  vec2 texel = 1.0 / vec2( textureSize( bbShadowMap,0 ) );
  float sh = 0.0;
  for( int y=-1;y<=1;y++ )
    for( int x=-1;x<=1;x++ )
      sh += texture( bbShadowMap, vec3( proj.xy + vec2(x,y)*texel, proj.z - bias ) );
  return sh / 9.0;
}

void main() {
  // Per-pixel lighting (moved here from the vertex shader). Uses the
  // interpolated view-space normal and position; identical light model.
  vec4 litColor;
  if( RS.FullBright==0 ){
    vec3 N = normalize( bbVertex_Normal );

    // Normal (bump) mapping: a texture whose blend is BLEND_DOT3 is treated as a
    // tangent-space normal map (OpenGL convention, +Y up). Sample it, build a
    // tangent frame from derivatives and perturb N BEFORE lighting, so diffuse +
    // specular react to the relief. Unrolled (constant sampler index) because ES
    // forbids dynamic sampler indexing; the guards are on uniforms (dynamically
    // uniform), so the texture fetch is well-defined. BUMP_STRENGTH scales the xy.
    const float BUMP_STRENGTH = 1.0;
    #define ApplyNormalMap(i) if( i<RS.TexturesUsed && RS.Texture[i].Blend==BLEND_DOT3 && RS.Texture[i].CubeMap==0 ){ vec3 nt=texture(bbTexture[i],bbVertex_TexCoord[i]).xyz*2.0-1.0; nt.xy*=BUMP_STRENGTH; N=normalize( cotangentFrame(N,bbVertex_ViewPos,bbVertex_TexCoord[i])*nt ); }
    ApplyNormalMap(0) ApplyNormalMap(1) ApplyNormalMap(2) ApplyNormalMap(3)
    ApplyNormalMap(4) ApplyNormalMap(5) ApplyNormalMap(6) ApplyNormalMap(7)

    vec4 Diffuse = vec4( 0.0 ), Specular = vec4( 0.0 );
    for( int i=0;i<LS.LightsUsed;i++ ){
      float nDotVP,nDotHV,pf;
      float attenuation = 1.0;
      vec3 LightDir;
      int lightType = int(LS.Light[i].Position.w);
      float lightRange = LS.Light[i].Params.x;
      if( lightType == 1 ){
        LightDir = normalize( mat3( bbViewMatrix ) * LS.Light[i].Direction.xyz );
      } else {
        vec3 lightViewPos = (bbViewMatrix * vec4(LS.Light[i].Position.xyz, 1.0)).xyz;
        vec3 lightToFrag = bbVertex_ViewPos - lightViewPos;
        float dist = length(lightToFrag);
        LightDir = -normalize(lightToFrag);
        if( lightRange > 0.0 ){
          attenuation = clamp(1.0 - (dist / lightRange), 0.0, 1.0);
          attenuation = attenuation * attenuation;
        }
      }
      vec3 halfVector = normalize( LightDir + vec3( 0.0, 0.0, -1.0 ));
      nDotVP = max( 0.0, dot( N, LightDir ) );
      pf = 0.0;
      if( RS.BrushShininess > 0.0 ){
        nDotHV = max( 0.0, dot( N, halfVector ));
        pf = pow( nDotHV, 128.0 * RS.BrushShininess ) * RS.BrushShininess * float(nDotVP != 0.0);
      }
      vec4 lightColor = clamp(LS.Light[i].Color, 0.0, 1.0);
      // shadow: only the shadow-casting light is modulated; others stay lit.
      // bbShadowLight defaults to -1 (engine) so this is a no-op until enabled.
      float sh = ( i == bbShadowLight ) ? shadowFactor( bbVertex_WorldPosition ) : 1.0;
      Diffuse  += lightColor * nDotVP * attenuation * sh;
      Specular += lightColor * pf * attenuation * sh;
    }
    litColor = RS.Ambient * bbVertex_Color + Diffuse * bbVertex_Color + Specular * vec4( 1.0 );
    litColor = clamp( litColor, 0.0, 1.0 );
    litColor.a = bbVertex_Color.a;
  } else {
    litColor = bbVertex_Color;
  }
  bbFragColor = litColor;

  // TODO: ES doesn't allow dynamic indexing of uniforms
  // so this (should) force the various compilers to unroll.
  // A BLEND_DOT3 slot is a normal map (consumed above to perturb N), so skip it
  // here - otherwise its raw RGB would tint the surface blue.
  #define ProcessTexture(i) if( i<RS.TexturesUsed && RS.Texture[i].Blend!=BLEND_DOT3 ) bbFragColor=Blend( bbFragColor,RS.Texture[i].CubeMap==0?Sample2D(bbTexture[i],i):SampleCube(bbTextureCube[i],RS.Texture[i].CubeMap),i );

  // For cubemaps, use direct sampling without blend
  if( 0<RS.TexturesUsed && RS.Texture[0].CubeMap!=0 ){
    bbFragColor = SampleCube(bbTextureCube[0],RS.Texture[0].CubeMap);
  } else {
    ProcessTexture(0);
  }
  ProcessTexture(1);
  ProcessTexture(2);
  ProcessTexture(3);
  ProcessTexture(4);
  ProcessTexture(5);
  ProcessTexture(6);
  ProcessTexture(7);

  if( RS.FogMode>0 ){
    vec4 fogColor=vec4( RS.FogColor.rgb,bbFragColor.a );
    bbFragColor=mix( bbFragColor,fogColor,bbVertex_FogFactor );
  }

  if( RS.AlphaTest==1 && bbFragColor.a < 0.5 ){
    discard;
  }
}
#endif
