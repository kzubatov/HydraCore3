#pragma once
#include "CamPluginAPI.h"
#include "LiteMath.h"

using LiteMath::float4x4;
using LiteMath::float4;
using LiteMath::float3;

class CamPinHole : public ICamRaysAPI
{
public:
  CamPinHole();
  virtual ~CamPinHole();

  void SetParameters(int a_width, int a_height, const CamParameters& a_params) override;
  void MakeRaysBlock(float* out_rayPosAndNear4f, float* out_rayDirAndFar4f, size_t in_blockSize, int passId)    override;
  void AddSamplesContributionBlock(float* out_color4f, const float* colors4f, size_t in_blockSize, 
                                   uint32_t a_width, uint32_t a_height, int passId);


  void MakeEyeRay   (int tid, float4* out_rayPosAndNear4f, float4* out_rayDirAndFar4f);
  void ContribSample(int tid, const float4* in_color, float4* out_color);

protected:

  void kernel_MakeEyeRay   (int tid, float4* out_rayPosAndNear4f, float4* out_rayDirAndFar4f);
  void kernel_ContribSample(int tid, const float4* in_color, float4* out_color);

  float4x4 m_proj;
  float4x4 m_worldView;
  float4x4 m_projInv;
  float4x4 m_worldViewInv;

  int m_width;
  int m_height;
};