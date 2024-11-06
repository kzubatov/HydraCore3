#pragma once
#include "cglobals.h"
#include "crandom.h"
#include "cmaterial.h"

static inline void gltfSampleAndEval(const Material* a_materials, float4 rands, float3 v, 
                                     float3 n, float2 tc, float4 baseColor, float4 fourParams, BsdfSample* pRes)
{
  // PLEASE! use 'a_materials[0].' for a while ... , not a_materials-> and not *(a_materials).
  const uint   cflags     = a_materials[0].cflags;
  const float  metalness  = cflags == GLTF_COMPONENT_METAL ? 1.0f : a_materials[0].data[GLTF_FLOAT_ALPHA]*fourParams.y;
  const float  coatValue  = a_materials[0].data[GLTF_FLOAT_REFL_COAT]*fourParams.z;                 
  const float  fresnelIOR = a_materials[0].data[GLTF_FLOAT_IOR];
  
  const float3 lambertDir   = lambertSample(float2(rands.x, rands.y), v, n);
  const float  lambertPdf   = lambertEvalPDF(lambertDir, v, n);
  const float  lambertVal   = lambertEvalBSDF(lambertDir, v, n);

  // (1) select between metal and dielectric via rands.z
  //
  float pdfSelect = 1.0f;
  if(rands.z < metalness) // select metall
  {
    pdfSelect         *= metalness;
    pRes->dir          = float3(0.0);
    pRes->val          = float4(0.0);
    pRes->pdf          = 0.f;
    pRes->flags        = RAY_FLAG_IS_DEAD;
  }
  else                // select dielectric
  {
    pdfSelect *= 1.0f - metalness;
    
    // (2) now select between specular and diffise via rands.w
    //
    const float f_i           = FrDielectricPBRT(std::abs(dot(v,n)), 1.0f, fresnelIOR); 
    const float prob_specular = 0.5f*coatValue;
    const float prob_diffuse  = 1.0f-prob_specular;
    
    if(rands.w < prob_specular) // specular
    {
      pdfSelect      *= prob_specular;
      pRes->dir       = float3(0.0);
      pRes->val       = float4(0.0);
      pRes->pdf       = 0.f;
      pRes->flags     = RAY_FLAG_IS_DEAD;
    } 
    else
    {
      pdfSelect      *= prob_diffuse; // lambert
      pRes->dir       = lambertDir;
      pRes->val       = lambertVal * baseColor * (1.0f - metalness);
      pRes->pdf       = lambertPdf;
      pRes->flags     = RAY_FLAG_HAS_NON_SPEC;
            
      if(coatValue > 0.0f && fresnelIOR > 0.0f) // Plastic, account for retroreflection between surface and coating layer
      {
        const float m_fdr_int = a_materials[0].data[GLTF_FLOAT_MI_FDR_INT];
        const float f_o       = FrDielectricPBRT(std::abs(dot(lambertDir, n)), 1.0f, fresnelIOR);
        pRes->val            *= lerp(1.0f, (1.0f - f_i) * (1.0f - f_o) / (fresnelIOR * fresnelIOR * (1.0f - m_fdr_int)), coatValue);
      }
    }
  }   
  pRes->pdf *= pdfSelect;
}


static void gltfEval(const Material* a_materials, float3 l, float3 v, float3 n, float2 tc, 
                     float4 baseColor, float4 fourParams, BsdfEval* res)
{
  const uint   cflags     = a_materials[0].cflags;
  const float  metalness  = cflags == GLTF_COMPONENT_METAL ? 1.0f : a_materials[0].data[GLTF_FLOAT_ALPHA]*fourParams.y;
  const float  coatValue  = a_materials[0].data[GLTF_FLOAT_REFL_COAT]*fourParams.z;      
  const float  fresnelIOR = a_materials[0].data[GLTF_FLOAT_IOR];
      
  float lambertVal       = lambertEvalBSDF(l, v, n);
  const float lambertPdf = lambertEvalPDF (l, v, n);
  float f_i              = 1.0f;
      
  if(coatValue > 0.0f && metalness < 1.0f && fresnelIOR > 0.0f) // Plastic, account for retroreflection between surface and coating layer
  {
    f_i                   = FrDielectricPBRT(std::abs(dot(v,n)), 1.0f, fresnelIOR);
    const float f_o       = FrDielectricPBRT(std::abs(dot(l,n)), 1.0f, fresnelIOR);  
    const float m_fdr_int = a_materials[0].data[GLTF_FLOAT_MI_FDR_INT];
    const float coeff     = lerp(1.0f, (1.f - f_i) * (1.f - f_o) / (fresnelIOR*fresnelIOR*(1.f - m_fdr_int)), coatValue);
    lambertVal           *= coeff;
  }
  
  // const float4 fConductor    = hydraFresnelCond(metalCol, VdotH, fresnelIOR, roughness); // (1) eval metal component      
  // const float4 specularColor = ggxVal*fConductor;                                        // eval metal specular component

  const float prob_specular = 0.5f*coatValue;
  const float prob_diffuse  = 1.0f-prob_specular;

  const float4 dielectricVal = lambertVal * baseColor;// + ggxVal * coatCol * f_i * coatValue;
  const float  dielectricPdf = lambertPdf * prob_diffuse;// + ggxPdf*prob_specular; 

  res->val = (1.0f - metalness) * dielectricVal; // (3) accumulate final color and pdf
  res->pdf = (1.0f - metalness) * dielectricPdf; // (3) accumulate final color and pdf
}