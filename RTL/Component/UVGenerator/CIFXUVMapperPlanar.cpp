//***************************************************************************
//
//  Copyright (c) 1999 - 2006 Intel Corporation
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//***************************************************************************

/**
	@file	CIFXUVMapperPlanar.h

			Class implementation file for the planar texture coordinate generator
			class.
*/

#include "CIFXUVMapperPlanar.h"

CIFXUVMapperPlanar::CIFXUVMapperPlanar() : CIFXUVMapperNone(TRUE)
{
}

CIFXUVMapperPlanar::~CIFXUVMapperPlanar()
{
}

// Factory method
IFXRESULT IFXAPI_CALLTYPE CIFXUVMapperPlanar_Factory( IFXREFIID interfaceId, void** ppInterface )
{
  IFXRESULT result;

  if ( ppInterface )
  {
    // It doesn't exist, so try to create it.  Note:  The component
    // class sets up gs_pSingleton upon construction and NULLs it
    // upon destruction.
    CIFXUVMapperPlanar  *pComponent = new CIFXUVMapperPlanar;

    if ( pComponent )
    {
      // Perform a temporary AddRef for our usage of the component.
      pComponent->AddRef();

      // Attempt to obtain a pointer to the requested interface.
      result = pComponent->QueryInterface( interfaceId, ppInterface );

      // Perform a Release since our usage of the component is now
      // complete.  Note:  If the QI fails, this will cause the
      // component to be destroyed.
      pComponent->Release();
    }
    else
      result = IFX_E_OUT_OF_MEMORY;
  }
  else
    result = IFX_E_INVALID_POINTER;

  return result;
}

BOOL CIFXUVMapperPlanar::NeedToMap(IFXMesh& rMesh, IFXUVMapParameters* pParams)
{
  BOOL bRet = FALSE;

  if(pParams)
  {
    IFXMeshAttributes eRenderTCs = rMesh.GetRenderTexCoordsInUse();

    if(!(eRenderTCs[IFX_MESH_RENDER_TC0 + pParams->uTextureLayer]))
    {
      eRenderTCs |= IFX_MESH_RENDER_TC0 + pParams->uTextureLayer;
      rMesh.SetRenderTexCoordsInUse(eRenderTCs);
    }

    IFXUVMapParameters& rUVMapperParams = rMesh.GetUVMapParameters(pParams->uTextureLayer);
    IFXInterleavedData* pData = 0;
    rMesh.GetMeshData( IFX_MESH_RENDER_TC0 + pParams->uTextureLayer, pData );

    if(!pData)
    {
      IFXCreateComponent(CID_IFXInterleavedData, IID_IFXInterleavedData, (void**)&pData);

      if(pData)
      {
        rMesh.SetMeshData(IFX_MESH_RENDER_TC0 + pParams->uTextureLayer, pData);
      }
      else
      {
        return FALSE;
      }
    }

    if(pData->GetNumVertices() < rMesh.GetMaxNumVertices())
    {
      bRet = TRUE;
    }
    else if(! (pParams->mWrapTransformMatrix == rUVMapperParams.mWrapTransformMatrix) )
    {
      bRet = TRUE;
    }
    else if(! (IFX_UV_PLANAR == rUVMapperParams.eWrapMode) )
    {
      bRet = TRUE;
    }
    else if(! (pParams->eOrientation == rUVMapperParams.eOrientation))
    {
      bRet = TRUE;
    }

    IFXRELEASE(pData);
  }

  return bRet;
}

IFXRESULT CIFXUVMapperPlanar::Map(  IFXMesh& rMesh,
                  IFXUVMapParameters* pParams,
                  IFXMatrix4x4* pModelMatrix,
                  IFXMatrix4x4* pViewMatrix,
                  const IFXLightSet* pLightSet)
{
  IFXRESULT rc = IFX_OK;

  if(IFXSUCCESS(rc))
  {
    IFXVector3Iter v3iVertex;
    IFXVector2Iter v2iTexture;

    IFXVector3* pvVertex;
    IFXVector2* pvTexCoord;
    IFXVector3  vMin, vMax;
    U32     uNumVertex;
    U32     uIndex;

    // calculate the transformation matrix
    IFXVector3 vTransformedPoint;

    // Invert as to effect the wrap space
    IFXMatrix4x4 invertedWrap;
    invertedWrap.Invert3x4(pParams->mWrapTransformMatrix);

    // get mesh parameters/iterators
    uNumVertex=rMesh.GetNumVertices();
    switch(pParams->eOrientation)
    {
    case IFX_UV_VERTEX:
      rMesh.GetPositionIter(v3iVertex);
      break;
    case IFX_UV_NORMAL:
      rMesh.GetNormalIter(v3iVertex);
      break;
    default:
      rc = IFX_E_UNSUPPORTED;
    }

    if(IFXSUCCESS(rc))
    {
      // Set Initial conditions
      pvVertex = v3iVertex.Index(0);
      invertedWrap.TransformVector(*pvVertex, vTransformedPoint);
      vMin = vTransformedPoint;
      vMax = vTransformedPoint;

      // Calculate bounding box
      U32 i;
      for( i = 0; i < uNumVertex; i++)
      {
        pvVertex = v3iVertex.Next();
        invertedWrap.TransformVector(*pvVertex, vTransformedPoint);

        if(vTransformedPoint.X() < vMin.X())
          vMin.X() = vTransformedPoint.X();
        if(vTransformedPoint.Y() < vMin.Y())
          vMin.Y() = vTransformedPoint.Y();

        if(vTransformedPoint.X() > vMax.X())
          vMax.X() = vTransformedPoint.X();
        if(vTransformedPoint.Y() > vMax.Y())
          vMax.Y() = vTransformedPoint.Y();
      }

      v3iVertex.PointAt(0);
    }

    if(IFXSUCCESS(rc))
    {
      IFXInterleavedData* pData = 0;
      rMesh.GetMeshData(IFX_MESH_RENDER_TC0 + pParams->uTextureLayer, pData);

      if(pData->GetNumVertices() < rMesh.GetMaxNumVertices())
      {
        U32 uVectorSize = sizeof(IFXVector2);
        pData->Allocate(1, &uVectorSize, rMesh.GetMaxNumVertices());
      }
      pData->GetVectorIter(0, v2iTexture);

      IFXVector3 boxSize;
      boxSize.Subtract(vMax, vMin);

      if(boxSize.X())
        boxSize.X() = 0.98f / boxSize.X();
      if(boxSize.Y())
        boxSize.Y() = 0.98f / boxSize.Y();

      // walk the mesh and calculate coordinate
      for(uIndex=0; uIndex<uNumVertex; uIndex++)
      {
        pvVertex=v3iVertex.Next();
        pvTexCoord=v2iTexture.Next();

        // transform the point via given transform
        invertedWrap.TransformVector( *pvVertex, vTransformedPoint);

        // apply mapping
        pvTexCoord->U() = (vTransformedPoint.X() - vMin.X()) * boxSize.X() + 0.01f;
        pvTexCoord->V() = (vTransformedPoint.Y() - vMin.Y()) * boxSize.Y() + 0.01f;
      }

      IFXRELEASE(pData);
    }
  }

  if(IFXSUCCESS(rc))
  {
    rMesh.GetUVMapParameters(pParams->uTextureLayer) = *pParams;
    rMesh.UpdateVersionWord(IFX_MESH_RENDER_TC0 + pParams->uTextureLayer);
  }

  return rc;
}
