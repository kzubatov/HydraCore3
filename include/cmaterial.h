#ifndef RTC_MATERIAL
#define RTC_MATERIAL

#include "cglobals.h"

struct BsdfSample
{
  float3 color;
  float3 direction;
  float  pdf; 
  uint   flags;
};

struct BsdfEval
{
  float3 color;
  float  pdf; 
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////

enum GLTF_COMPOMENT { GLTF_COMPONENT_LAMBERT   = 1, 
                      GLTF_COMPONENT_COAT      = 2,
                      GLTF_COMPONENT_METAL     = 4,
                      GLTF_METAL_PERF_MIRROR   = 8, 
                      GLTF_COMPONENT_ORENNAYAR = 16 }; // bit fields

enum MATERIAL_TYPES { MAT_TYPE_GLTF          = 1,
                      MAT_TYPE_GLASS         = 2,
                      MAT_TYPE_LIGHT_SOURCE  = 0xEFFFFFFF };

enum MATERIAL_EVENT {
  RAY_EVENT_S         = 1,  ///< Indicates Specular reflection or refraction  (additionally check for RAY_EVENT_T)
  RAY_EVENT_D         = 2,  ///< Indicates Diffuse  reflection or translucent (additionally check for RAY_EVENT_T)
  RAY_EVENT_G         = 4,  ///< Indicates Glossy   reflection or refraction  (additionally check for RAY_EVENT_T)
  RAY_EVENT_T         = 8,  ///< Indicates Transparensy or reftacrion. 
  RAY_EVENT_V         = 16, ///< Indicates Volume scattering, not used for a while
  RAY_EVENT_TOUT      = 32, ///< Indicates Transparensy Outside of water or glass or e.t.c. (old RAY_IS_INSIDE_TRANSPARENT_OBJECT = 128)
  RAY_EVENT_TNINGLASS = 64,
};

////////////////////////////////
// Indexes for materials
// 
// Custom for all materials
static constexpr uint UINT_MTYPE                  = 0;  ///< one of 'MATERIAL_TYPES'
static constexpr uint UINT_CFLAGS                 = 1;  ///< combination of some matertial flags, for GLTF is a combination of 'GLTF_COMPOMENT' bits
static constexpr uint UINT_LIGHTID                = 2;  ///< identifier of light if this material is light 

// GLTF
// The BRDF of the metallic-roughness material is a linear interpolation of a metallic BRDF and a dielectric BRDF. 
// The BRDFs **share** the parameters for roughness and base color.
// colors
static constexpr uint GLTF_COLOR_BASE             = 0;  ///< color for both lambert and emissive lights; baseColor.w store emission
static constexpr uint GLTF_COLOR_COAT             = 1;  ///< in our implementation we allow different color for coating (fresnel) and diffuse
static constexpr uint GLTF_COLOR_METAL            = 2;  ///< in our implementation we allow different color for metals and diffuse
                                                        
// custom                                               
static constexpr uint GLTF_FLOAT_MI_FDR_INT       = 3;  ///< ScalarFloat m_fdr_int;
static constexpr uint GLTF_FLOAT_MI_FDR_EXT       = 4;  ///< ScalarFloat m_fdr_ext;
static constexpr uint GLTF_FLOAT_MI_SSW           = 5;  ///< Float m_specular_sampling_weight;
static constexpr uint GLTF_FLOAT_ALPHA            = 6;  ///< blend factor between dielectric and metal reflection : alpha*baseColor + (1.0f-alpha)*baseColor
static constexpr uint GLTF_FLOAT_GLOSINESS        = 7;  ///< material glosiness or intensity for lights, take color from baseColor
static constexpr uint GLTF_FLOAT_IOR              = 8;  ///< index of refraction for reflection Fresnel
static constexpr uint GLTF_FLOAT_ROUGH_ORENNAYAR  = 9;  ///< roughness for Oren-Nayar
static constexpr uint GLTF_UINT_TEXID0            = 10; ///< texture id

// GLASS
// colors
static constexpr uint GLASS_COLOR_REFLECT         = 0;    
static constexpr uint GLASS_COLOR_TRANSP          = 1;  

// custom 
static constexpr uint GLASS_FLOAT_GLOSS_REFLECT   = 3;
static constexpr uint GLASS_FLOAT_GLOSS_TRANSP    = 4;
static constexpr uint GLASS_FLOAT_IOR             = 5;


// The size is taken according to the largest indexes
static constexpr uint COLOR_DATA_SIZE             = 6;
static constexpr uint CUSTOM_DATA_SIZE            = 11;

struct Material
{
  float4 colors[COLOR_DATA_SIZE]; ///< colors data

  float4 row0[1];     ///< texture matrix
  float4 row1[1];     ///< texture matrix
      
  float data[CUSTOM_DATA_SIZE]; ///< float, uint and custom data. Read uint: uint x = as_uint(data[INDEX]), write: data[INDEX] = as_float(x)
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lambert BRDF
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float3 lambertSample(float2 rands, float3 v, float3 n)
{
   return MapSampleToCosineDistribution(rands.x, rands.y, n, n, 1.0f);
}

static inline float lambertEvalPDF(float3 l, float3 v, float3 n) 
{ 
  return std::abs(dot(l, n)) * INV_PI;
}

static inline float lambertEvalBSDF(float3 l, float3 v, float3 n)
{
  return INV_PI;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// PBRT routine
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float cosPhiPBRT(const float3 w, const float sintheta)
{
  if (sintheta == 0.0f)
    return 1.0f;
  else
    return clamp(w.x / sintheta, -1.0f, 1.0f);
}

static inline float sinPhiPBRT(const float3 w, const float sintheta)
{
  if (sintheta == 0.0f)
    return 0.0f;
  else
    return clamp(w.y / sintheta, -1.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Oren-Nayar BRDF from PBRT
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float orennayarFunc(const float3 a_l, const float3 a_v, const float3 a_n, const float a_roughness)
{
  const float cosTheta_wi = dot(a_l, a_n);
  const float cosTheta_wo = dot(a_v, a_n);

  const float sinTheta_wi = sqrt(fmax(0.0f, 1.0f - cosTheta_wi * cosTheta_wi));
  const float sinTheta_wo = sqrt(fmax(0.0f, 1.0f - cosTheta_wo * cosTheta_wo));

  const float sigma  = a_roughness * M_PI * 0.5f; //Radians(sig)
  const float sigma2 = sigma * sigma;
  const float A      = 1.0f - (sigma2 / (2.0f * (sigma2 + 0.33f)));
  const float B      = 0.45f * sigma2 / (sigma2 + 0.09f);

  ///////////////////////////////////////////////////////////////////////////// to PBRT coordinate system
  // wo = a_v = -ray_dir
  // wi = a_l = newDir
  //
  float3 nx, ny, nz = a_n;
  CoordinateSystem(nz, &nx, &ny);

  ///////////////////////////////////////////////////////////////////////////// to PBRT coordinate system

  // Compute cosine term of Oren-Nayar model
  float maxcos = 0.0F;

  if (sinTheta_wi > 1e-4 && sinTheta_wo > 1e-4)
  {
    const float3 wo     = float3(-dot(a_v, nx), -dot(a_v, ny), -dot(a_v, nz));
    const float3 wi     = float3(-dot(a_l, nx), -dot(a_l, ny), -dot(a_l, nz));
    const float sinphii = sinPhiPBRT(wi, sinTheta_wi);
    const float cosphii = cosPhiPBRT(wi, sinTheta_wi);
    const float sinphio = sinPhiPBRT(wo, sinTheta_wo);
    const float cosphio = cosPhiPBRT(wo, sinTheta_wo);
    const float dcos    = cosphii * cosphio + sinphii * sinphio;
    maxcos              = fmax(0.0F, dcos);
  }

  // Compute sine and tangent terms of Oren-Nayar model
  float sinalpha = 0.0F, tanbeta = 0.0F;

  if (fabs(cosTheta_wi) > fabs(cosTheta_wo))
  {
    sinalpha = sinTheta_wo;
    tanbeta  = sinTheta_wi / fmax(fabs(cosTheta_wi), DEPSILON);
  }
  else
  {
    sinalpha = sinTheta_wi;
    tanbeta  = sinTheta_wo / fmax(fabs(cosTheta_wo), DEPSILON);
  }

  return (A + B * maxcos * sinalpha * tanbeta);
}


static inline float orennayarEvalPDF(const float3 l, const float3 n)
{
  return fabs(dot(l, n)) * INV_PI;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float3 SphericalDirectionPBRT(const float sintheta, const float costheta, const float phi) 
{ 
  return float3(sintheta * cos(phi), sintheta * sin(phi), costheta); 
}

static inline float GGX_Distribution(const float cosThetaNH, const float alpha)
{
  const float alpha2 = alpha * alpha;
  const float NH_sqr = clamp(cosThetaNH * cosThetaNH, 0.0f, 1.0f);
  const float den    = NH_sqr * alpha2 + (1.0f - NH_sqr);
  return alpha2 / std::max((float)(M_PI) * den * den, 1e-6f);
}

static inline float GGX_GeomShadMask(const float cosThetaN, const float alpha)
{
  // Height - Correlated G.
  //const float tanNV      = sqrt(1.0f - cosThetaN * cosThetaN) / cosThetaN;
  //const float a          = 1.0f / (alpha * tanNV);
  //const float lambda     = (-1.0f + sqrt(1.0f + 1.0f / (a*a))) / 2.0f;
  //const float G          = 1.0f / (1.0f + lambda);

  // Optimized and equal to the commented-out formulas on top.
  const float cosTheta_sqr = clamp(cosThetaN * cosThetaN, 0.0f, 1.0f);
  const float tan2         = (1.0f - cosTheta_sqr) / std::max(cosTheta_sqr, 1e-6f);
  const float GP           = 2.0f / (1.0f + std::sqrt(1.0f + alpha * alpha * tan2));
  return GP;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float3 ggxSample(float2 rands, float3 v, float3 n, float roughness)
{
  const float roughSqr = roughness * roughness;
    
  float3 nx, ny, nz = n;
  CoordinateSystem(nz, &nx, &ny);
    
  const float3 wo       = float3(dot(v, nx), dot(v, ny), dot(v, nz));
  const float phi       = rands.x * M_TWOPI;
  const float cosTheta  = clamp(std::sqrt((1.0f - rands.y) / (1.0f + roughSqr * roughSqr * rands.y - rands.y)), 0.0f, 1.0f);
  const float sinTheta  = std::sqrt(1.0f - cosTheta * cosTheta);
  const float3 wh       = SphericalDirectionPBRT(sinTheta, cosTheta, phi);
    
  const float3 wi = 2.0f * dot(wo, wh) * wh - wo;      // Compute incident direction by reflecting about wm  
  return normalize(wi.x * nx + wi.y * ny + wi.z * nz); // back to normal coordinate system
}

static inline float ggxEvalPDF(float3 l, float3 v, float3 n, float roughness) 
{ 
  const float dotNV = dot(n, v);
  const float dotNL = dot(n, l);
  if (dotNV < 1e-6f || dotNL < 1e-6f)
    return 1.0f;

  const float  roughSqr  = roughness * roughness;
    
  const float3 h    = normalize(v + l); // half vector.
  const float dotNH = dot(n, h);
  const float dotHV = dot(h, v);
  const float D     = GGX_Distribution(dotNH, roughSqr);
  return  D * dotNH / (4.0f * std::max(dotHV,1e-6f));
}

static inline float ggxEvalBSDF(float3 l, float3 v, float3 n, float roughness)
{
  if(std::abs(dot(l, n)) < 1e-5f)
    return 0.0f; 
 
  const float dotNV = dot(n, v);  
  const float dotNL = dot(n, l);
  if (dotNV < 1e-6f || dotNL < 1e-6f)
    return 0.0f; 

  const float  roughSqr = roughness * roughness;
  const float3 h    = normalize(v + l); // half vector.
  const float dotNH = dot(n, h);
  const float D     = GGX_Distribution(dotNH, roughSqr);
  const float G     = GGX_GeomShadMask(dotNV, roughSqr)*GGX_GeomShadMask(dotNL, roughSqr);      

  return (D * G / std::max(4.0f * dotNV * dotNL, 1e-6f));  // Pass single-scattering
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline float FrDielectricPBRT(float cosThetaI, float etaI, float etaT) 
{
  cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
  // Potentially swap indices of refraction
  bool entering = cosThetaI > 0.f;
  if (!entering) 
  {
    const float tmp = etaI;
    etaI = etaT;
    etaT = tmp;
    cosThetaI = std::abs(cosThetaI);
  }

  // Compute _cosThetaT_ using Snell's law
  float sinThetaI = std::sqrt(std::max(0.0f, 1.0f - cosThetaI * cosThetaI));
  float sinThetaT = etaI / etaT * sinThetaI;

  // Handle total internal reflection
  if (sinThetaT >= 1.0f) 
    return 1.0f;

  const float cosThetaT = std::sqrt(std::max(0.0f, 1.0f - sinThetaT * sinThetaT));
  const float Rparl     = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
  const float Rperp     = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
  return 0.5f*(Rparl * Rparl + Rperp * Rperp);
}

//static inline float fresnelConductor(float cosTheta, float eta, float roughness)
//{
//  float tmp = (eta*eta + roughness*roughness) * (cosTheta * cosTheta);
//  float rParl2 = (tmp - (eta * (2.0f * cosTheta)) + 1.0f) / (tmp + (eta * (2.0f * cosTheta)) + 1.0f);
//  float tmpF = eta*eta + roughness*roughness;
//  float rPerp2 = (tmpF - (eta * (2.0f * cosTheta)) + (cosTheta*cosTheta)) / (tmpF + (eta * (2.0f * cosTheta)) + (cosTheta*cosTheta));
//  return 0.5f*(rParl2 + rPerp2);
//}

static inline float fresnelSlick(float VdotH)
{
  const float tmp = 1.0f - std::abs(VdotH);
  return (tmp*tmp)*(tmp*tmp)*tmp;
}

static inline float3 hydraFresnelCond(float3 f0, float VdotH, float ior, float roughness) 
{
  if(ior == 0.0f) // fresnel reflactance is disabled
    return f0;

  return f0 + (float3(1.0f,1.0f,1.0f) - f0) * fresnelSlick(VdotH); // return bsdf * (f0 + (1 - f0) * (1 - abs(VdotH))^5)
}


#endif