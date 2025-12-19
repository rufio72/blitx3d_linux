precision highp float;
precision highp int;

uniform mat4 bbProjMatrix;
uniform mat4 bbViewMatrix;
uniform mat4 bbWorldMatrix;
uniform mat3 bbNormalMatrix; // Optimization #4
uniform vec3 bbCameraPos;    // Camera world position for reflections

uniform sampler2D bbTexture[8];
uniform samplerCube bbTextureCube[8];

#define FOG_NONE   0
#define FOG_LINEAR 1

struct BBLightData {
  mat4 TForm;
  vec4 Color;
  vec4 Position;  // xyz = world position, w = type (1=directional, 2=point, 3=spot)
  vec4 Params;    // x = range, y = inner_cone, z = outer_cone, w = unused
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

mat4 rotationMatrix(vec3 axis, float angle){
  axis = normalize(axis);
  float s = sin(angle);
  float c = cos(angle);
  float oc = 1.0 - c;

  return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
              oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
              oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
              0.0,                                0.0,                                0.0,                                1.0);
}

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

void main() {
  mat4 bbModelViewMatrix = bbViewMatrix * bbWorldMatrix;
  mat4 bbModelViewProjectionMatrix=bbProjMatrix * bbModelViewMatrix;
  // Optimization #4: bbNormalMatrix is now passed as uniform from CPU

  bbVertex_Position = bbPosition;

  // Calculate world space position and normal for cubemap reflections
  vec4 worldPos = bbWorldMatrix * vec4(bbPosition, 1.0);
  bbVertex_WorldPosition = worldPos.xyz;
  bbVertex_WorldNormal = normalize(mat3(bbWorldMatrix) * bbNormal);

  gl_Position = bbModelViewProjectionMatrix * vec4(bbPosition, 1.0);

  vec3 EyeNormal = bbNormalMatrix * bbNormal;

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

  if( RS.FullBright==0 ){
    vec4 Diffuse=vec4( 0.0 ),Specular=vec4( 0.0 );

    // Get vertex position in view space for point light calculations
    vec3 vertexViewPos = (bbModelViewMatrix * vec4(bbPosition, 1.0)).xyz;

    for( int i=0;i<LS.LightsUsed;i++ ){
      float nDotVP,nDotHV,pf;
      float attenuation = 1.0;
      vec3 LightDir;

      int lightType = int(LS.Light[i].Position.w);
      float lightRange = LS.Light[i].Params.x;

      if( lightType == 1 ){
        // Directional light
        LightDir = normalize( mat3( bbViewMatrix*LS.Light[i].TForm*rotationMatrix( vec3(1.0,0.0,0.0), 1.5708 ) )*vec3(0.0,1.0,0.0) );
      } else {
        // Point light (type 2) or Spot light (type 3)
        vec3 lightWorldPos = LS.Light[i].Position.xyz;
        vec3 lightViewPos = (bbViewMatrix * vec4(lightWorldPos, 1.0)).xyz;
        vec3 lightToVertex = vertexViewPos - lightViewPos;
        float distance = length(lightToVertex);
        LightDir = -normalize(lightToVertex);

        // Attenuation based on range
        if( lightRange > 0.0 ){
          attenuation = clamp(1.0 - (distance / lightRange), 0.0, 1.0);
          attenuation = attenuation * attenuation; // Quadratic falloff
        }
      }

      vec3 halfVector = normalize( LightDir + vec3( 0.0, 0.0, -1.0 ));

      nDotVP = max( 0.0, dot( bbVertex_Normal, LightDir ) );
      nDotHV = max( 0.0, dot( bbVertex_Normal, vec3( halfVector )));
      pf = pow( nDotHV, 100.0 ) * float(nDotVP != 0.0);

      // Limit light contribution to prevent over-saturation
      vec4 lightColor = clamp(LS.Light[i].Color, 0.0, 1.0);
      Diffuse  += lightColor * nDotVP * attenuation;
      Specular += lightColor * pf * attenuation;
    }

    bbVertex_Color = RS.Ambient * bbMaterialColor +
                     Diffuse    * bbMaterialColor +
                     Specular   * vec4( 1.0 );
    bbVertex_Color = clamp( bbVertex_Color, 0.0, 1.0 );
    bbVertex_Color.a = bbMaterialColor.a; // TODO: is this right?
  }else{
    bbVertex_Color = bbMaterialColor;
  }

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

vec4 SampleCube( samplerCube tex ){
  // View direction: from camera to fragment (in world space)
  vec3 I = normalize(bbVertex_WorldPosition - bbCameraPos);

  // Normal in world space
  vec3 N = normalize(bbVertex_WorldNormal);

  // Reflection vector
  vec3 R = reflect(I, N);

  // BUG: Il riflesso mostra direzione sbagliata - vedi DEBUG_CUBEMAP_REPORT.txt
  R.z = -R.z;

  return texture(tex, R);
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

void main() {
  bbFragColor=bbVertex_Color;

  // TODO: ES doesn't allow dynamic indexing of uniforms
  // so this (should) force the various compilers to unroll.
  #define ProcessTexture(i) if( i<RS.TexturesUsed ) bbFragColor=Blend( bbFragColor,RS.Texture[i].CubeMap!=1?Sample2D(bbTexture[i],i):SampleCube(bbTextureCube[i]),i );

  // For cubemaps, use direct sampling without blend
  if( 0<RS.TexturesUsed && RS.Texture[0].CubeMap==1 ){
    bbFragColor = SampleCube(bbTextureCube[0]);
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
