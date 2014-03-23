/* 
 *  Copyright (C) 2011 Team XBMC
 *  http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef HAS_DS_PLAYER
#include "madVRAllocatorPresenter.h"
#include "windowing/WindowingFactory.h"
#include <moreuuids.h>
#include "RendererSettings.h"
#include "Application.h"
#include "cores/VideoRenderers/RenderManager.h"
#include "../DSPlayer.h"
#include "ApplicationMessenger.h"
//
// CmadVRAllocatorPresenter
//

CmadVRAllocatorPresenter::CmadVRAllocatorPresenter(HWND hWnd, HRESULT& hr, CStdString &_Error)
  : ISubPicAllocatorPresenterImpl(hWnd, hr)
  , m_ScreenSize(0, 0)
{
  if(FAILED(hr)) {
    _Error += L"ISubPicAllocatorPresenterImpl failed\n";
    return;
  }
  m_bIsFullscreen = false;
  hr = S_OK;
}

CmadVRAllocatorPresenter::~CmadVRAllocatorPresenter()
{
  /*if(m_pSRCB) {
    ((CSubRenderCallback*)(ISubRenderCallback2*)m_pSRCB)->SetDXRAP(NULL);
  }*/

  m_pSubPicQueue = NULL;
  m_pAllocator = NULL;
  m_pDXR = NULL;
}

STDMETHODIMP CmadVRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  if(riid != IID_IUnknown && m_pDXR) {
    if(SUCCEEDED(m_pDXR->QueryInterface(riid, ppv))) {
      return S_OK;
    }
  }

  return __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CmadVRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
  if (!pD3DDev)
  {
    // release all resources
    m_pSubPicQueue = NULL;
    m_pAllocator = NULL;
    return S_OK;
  }

  Com::SmartSize size;
  
  //from mpc-hc
  switch(0)//GetRenderersSettings().nSPCMaxRes) 
  {
    case 0:
    default:
      size = m_ScreenSize;
      break;
    case 1:
      size.SetSize(1024, 768);
      break;
    case 2:
      size.SetSize(800, 600);
      break;
    case 3:
      size.SetSize(640, 480);
      break;
    case 4:
      size.SetSize(512, 384);
      break;
    case 5:
      size.SetSize(384, 288);
      break;
    case 6:
      size.SetSize(2560, 1600);
      break;
    case 7:
      size.SetSize(1920, 1080);
      break;
    case 8:
      size.SetSize(1320, 900);
      break;
    case 9:
      size.SetSize(1280, 720);
      break;
  }

  if(m_pAllocator) {
    m_pAllocator->ChangeDevice(pD3DDev);
  } else {
    m_pAllocator = DNew CDX9SubPicAllocator(pD3DDev, size,true);
    if(!m_pAllocator) {
      return E_FAIL;
    }
  }

  HRESULT hr = S_OK;
  
  if (g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead > 0)
    m_pSubPicQueue = new CSubPicQueue(g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead, g_dsSettings.pRendererSettings->subtitlesSettings.disableAnimations, m_pAllocator, &hr);
  else
    m_pSubPicQueue = new CSubPicQueueNoThread(m_pAllocator, &hr);
  m_pSubPicQueue = g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead > 0
           ? (ISubPicQueue*)DNew CSubPicQueue(g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead > 0, true, m_pAllocator, &hr)
           : (ISubPicQueue*)DNew CSubPicQueueNoThread(m_pAllocator, &hr);
  if(!m_pSubPicQueue || FAILED(hr)) {
    return E_FAIL;
  }

  if(m_SubPicProvider) {
    m_pSubPicQueue->SetSubPicProvider(m_SubPicProvider);
  }

  return S_OK;
}

// A structure for our custom vertex type
struct CUSTOMVERTEX5
{
    FLOAT x, y, z, rhw; // The transformed position for the vertex
    DWORD color;        // The vertex color
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX5 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

void CmadVRAllocatorPresenter::D3dTest()
{
  m_pVB = NULL;
  // Initialize three Vertices for rendering a triangle
    CUSTOMVERTEX5 Vertices[] =
    {
        { 150.0f,  50.0f, 0.5f, 1.0f, 0xffff0000, }, // x, y, z, rhw, color
        { 250.0f, 250.0f, 0.5f, 1.0f, 0xff00ff00, },
        {  50.0f, 250.0f, 0.5f, 1.0f, 0xff00ffff, },
    };

    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom Vertices. We also
    // specify the FVF, so the vertex buffer knows what data it contains.
    if( FAILED( m_pD3DDevice->CreateVertexBuffer( 3 * sizeof( CUSTOMVERTEX5 ),
                                                  0, D3DFVF_CUSTOMVERTEX5,
                                                  D3DPOOL_DEFAULT, &m_pVB, NULL ) ) )
    {
        return;
    }

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Vertices. This mechanism is required becuase vertex
    // buffers may be in device memory.
    VOID* pVertices;
    if( FAILED( m_pVB->Lock( 0, sizeof( Vertices ), ( void** )&pVertices, 0 ) ) )
        return;
    memcpy( pVertices, Vertices, sizeof( Vertices ) );
    m_pVB->Unlock();

    return;
  
}

HRESULT CmadVRAllocatorPresenter::Render(
  REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf,
  int left, int top, int right, int bottom, int width, int height)
{
  if (!m_bIsFullscreen)
  {
    //CRetakeLock<CExclusiveLock> lock(m_sharedSection, false);
    //lock.Leave();
    m_bIsFullscreen = g_application.SwitchToFullScreen();
    D3dTest();
    //CApplicationMessenger::Get().SwitchToFullscreen();
    //lock.Enter();
  //g_renderManager.Configure(m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy, m_fps,
  //            g_dsSettings.pRendererSettings->bAllowFullscreen ? CONF_FLAGS_FULLSCREEN : 0, 
	//		  RENDER_FMT_NONE, 0, 0);
  //m_bIsFullscreen = true;
  }
  
  //g_application.RenderMadVr();
  m_rtNow = rtStart;
  //AlphaBltSubPic(Com::SmartSize(width, height));
  
  m_pD3DDevice->SetStreamSource( 0, m_pVB, 0, sizeof( CUSTOMVERTEX5 ) );
        m_pD3DDevice->SetFVF( D3DFVF_CUSTOMVERTEX5 );
        m_pD3DDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );
  return S_OK;
  /*if (!g_renderManager.IsConfigured())
  {
    g_renderManager.Configure(m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy, m_fps,
              CONF_FLAGS_FULLSCREEN, 0);
  }*/
  //m_drawingIsDone.Reset();
  int64_t start = CurrentHostCounter();
  
  
  m_readyToDraw.Set();
  int64_t end1 = CurrentHostCounter();
  m_readyToStartBack.WaitMSec(200);
  int64_t end2 = CurrentHostCounter();
  //g_application.NewFrame();
  //m_drawingIsDone.Wait(); // Wait until the drawing is done
  __super::SetPosition(Com::SmartRect(0, 0, width, height), Com::SmartRect(left, top, right, bottom)); // needed? should be already set by the player
  SetTime(rtStart);
  if(atpf > 0 && m_pSubPicQueue) {
    m_pSubPicQueue->SetFPS(10000000.0 / atpf);
  }
  AlphaBltSubPic(Com::SmartSize(width, height));
  /*this to see the time it take to render*/
#if 0
  CLog::Log(LOGINFO, "%s Rendering. Setting %.2fms Wait() took %.2fms", __FUNCTION__, 1000.f * (end1 - start) / CurrentHostFrequency(), 1000.f * (end2 - end1) / CurrentHostFrequency());
#endif
  return S_OK;
}

// ISubPicAllocatorPresenter

STDMETHODIMP CmadVRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
  CheckPointer(ppRenderer, E_POINTER);

  if(m_pDXR) {
    return E_UNEXPECTED;
  }
  HRESULT hr = m_pDXR.CoCreateInstance(CLSID_madVR, GetOwner());
  if(!m_pDXR) {
    return E_FAIL;
  }
  
  Com::SmartQIPtr<ISubRender> pSR = m_pDXR;
  Com::SmartQIPtr<IMadVRDirect3D9Manager> pMadVrD3d = m_pDXR;
  m_pD3DDevice =g_Windowing.Get3DDevice();
  hr = pMadVrD3d->UseTheseDevices(m_pD3DDevice,m_pD3DDevice,m_pD3DDevice);
  hr = pMadVrD3d->ConfigureDisplayModeChanger(FALSE, FALSE);
  //Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR;
  //hr = pVW->put_Owner((OAHWND)g_hWnd);
  //hr = pVW->put_Left(0);
  //hr = pVW->put_Top(0);

  if(!pSR) {
    m_pDXR = NULL;
    return E_FAIL;
  }

  m_pSRCB = DNew CSubRenderCallback(this);
  if(FAILED(pSR->SetCallback(m_pSRCB))) {
    m_pDXR = NULL;
    return E_FAIL;
  }
  Com::SmartQIPtr<IBaseFilter> pBF = this;
  if(FAILED(hr))
      *ppRenderer = NULL;
    else
      *ppRenderer = pBF.Detach();
  /*(*ppRenderer = this)->AddRef();*/

  MONITORINFO mi;
  mi.cbSize = sizeof(MONITORINFO);
  if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
    m_ScreenSize.SetSize(mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top);
  }

  CGraphFilters::Get()->SetMadVrCallback(this);

  return S_OK;
}

STDMETHODIMP_(void) CmadVRAllocatorPresenter::SetPosition(RECT w, RECT v)
{
  if(Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    pBV->SetDefaultSourcePosition();
    pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
  }

  if(Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR) {
    pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
  }
}

STDMETHODIMP_(SIZE) CmadVRAllocatorPresenter::GetVideoSize(bool fCorrectAR)
{
  SIZE size = {0, 0};

  if(!fCorrectAR) {
    if(Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
      pBV->GetVideoSize(&size.cx, &size.cy);
    }
  } else {
    if(Com::SmartQIPtr<IBasicVideo2> pBV2 = m_pDXR) {
      pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
    }
  }

  return size;
}

STDMETHODIMP CmadVRAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
  HRESULT hr = E_NOTIMPL;
  if(Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
  }
  return hr;
}

STDMETHODIMP_(bool) CmadVRAllocatorPresenter::Paint(bool fAll)
{
  return false;
}

STDMETHODIMP CmadVRAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
  return E_NOTIMPL;
}

#endif