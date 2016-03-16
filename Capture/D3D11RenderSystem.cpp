#include "D3D11RenderSystem.hpp"
#include "WinPlatform.h"
#include <stdexcept>

#ifndef _USING_V110_SDK71_
#include <dxgi1_2.h>
#endif

typedef HRESULT(WINAPI*DXGICREATEPROC)(REFIID riid, void ** ppFactory);
typedef HRESULT(WINAPI*D3D11PROC)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, D3D_FEATURE_LEVEL*, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** ppImmediateContext);


ID3D10Device1* pDevice = NULL;


//vs2008 xp hook
//�������ӵ�D3D11.lib��dxgi.lib ֻ�ܲ�ȡ��̬������dll
//��֧��vista sp1��
bool h3d::D3D11Engine::Construct(HWND hwnd)
{
	factory = new D3D11Factory();
	if (!factory)
		return false;

	HMODULE hDXGIDll = LoadLibraryW(L"dxgi.dll");
	if (!hDXGIDll)
		return false;

	int version = GetOSVersion();
	//7 CreateDXGIFactory1
	//8 CreateDXGIFactory2
#ifndef _USING_V110_SDK71_
	REFIID iid = version >= 8 ? __uuidof(IDXGIFactory2) : __uuidof(IDXGIFactory1);
#else
	REFIID iid = __uuidof(IUnknown);//xp��Ӧ��ִ�гɹ�
#endif

	DXGICREATEPROC CreateDXGI = (DXGICREATEPROC)GetProcAddress(hDXGIDll, "CreateDXGIFactory1");

	IDXGIFactory1* pFactory = NULL;
	if (FAILED(CreateDXGI(iid, reinterpret_cast<void**>(&pFactory))))
		return false;

	//����ʹ�õ�һ���Կ�
	IDXGIAdapter1* pAdapter = NULL;
	if (FAILED(pFactory->EnumAdapters1(0, &pAdapter)))
		return false;

	//TODO log�Կ���Ϣ��NVDIA hack

	RECT rect;
	GetWindowRect(hwnd, &rect);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	ZeroMemory(&swap_desc, sizeof(swap_desc));
	swap_desc.BufferCount = 2;
	swap_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_desc.BufferDesc.Width = rect.right - rect.left;
	swap_desc.BufferDesc.Height = rect.bottom - rect.top;
	swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_desc.OutputWindow = hwnd;
	swap_desc.SampleDesc.Count = 1;
	swap_desc.SampleDesc.Quality = 0;
	swap_desc.Windowed = TRUE;
	swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap_desc.Flags = 0;


	HMODULE hD3D11Dll = LoadLibraryW(L"d3d11.dll");
	if (!hD3D11Dll)
		return false;

	D3D11PROC create = (D3D11PROC)GetProcAddress(hD3D11Dll, "D3D11CreateDeviceAndSwapChain");
	if (!create)
		return false;

	D3D_FEATURE_LEVEL desired_levels[] = { D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2 ,D3D_FEATURE_LEVEL_9_1 };
	D3D_FEATURE_LEVEL support_level;
	D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_UNKNOWN;

	UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif


	//Warning: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476083(v=vs.85).aspx
	//if pAdapter = NULL,driver_type = D3D_DRIVER_TYPE_NULL;create will be success,but the device can't OpenSharedResource
	//NULL������debug����ȾAPI/OpenSharedResource ����ʧ��
	//if pAdapter != NULL,driver_type != D3D_DRIVER_TYPE_UNKNOWN;create will be failed
	//�Ƽ�ʹ�� pAdapter = NULL,driver_type = D3D_DRIVER_TYPE_HARDWARE
	//����ö���Կ� pAdapter,driver_type = D3D_DRIVER_TYPE_UNKNOWN
	if (FAILED(
		(*create)(pAdapter, driver_type, NULL,Flags, desired_levels, 6, D3D11_SDK_VERSION, &swap_desc, &swap_chain, &factory->device, &support_level, &context)
		))
		return false;

	pAdapter->Release();
	pFactory->Release();

	return true;
}

#define SR(var) if(var) {var->Release();var = NULL;}

void h3d::D3D11Engine::Destroy()
{
	SR(context);
	SR(swap_chain);
	SR(factory->device);
	delete factory;
}

h3d::D3D11Engine::D3D11Engine()
	:factory(NULL),context(NULL),swap_chain(NULL)
{
}

h3d::D3D11Factory & h3d::D3D11Engine::GetFactory()
{
	return *factory;
}

void h3d::D3D11Engine::CopyTexture(D3D11Texture * dst, D3D11Texture * src)
{
	context->CopyResource(dst->texture, src->texture);
}

h3d::D3D11Texture::MappedData h3d::D3D11Engine::Map(D3D11Texture * tex)
{
	D3D11_MAPPED_SUBRESOURCE subRes;
	if (FAILED(context->Map(tex->texture, 0, D3D11_MAP_READ, 0, &subRes)))
		throw std::runtime_error("D3D11Texture2D Map Failed");
	return {reinterpret_cast<byte*>(subRes.pData),subRes.RowPitch };
}

void h3d::D3D11Engine::UnMap(D3D11Texture * tex)
{
	context->Unmap(tex->texture, 0);
}

h3d::D3D11Factory::D3D11Factory()
	:device(NULL)
{
}

h3d::D3D11Texture * h3d::D3D11Factory::CreateTexture(SDst Width, SDst Height, unsigned __int64 Handle)
{
	HANDLE hShare = reinterpret_cast<HANDLE>(Handle);

	ID3D11Resource* share_res = NULL;

	if (SUCCEEDED(device->OpenSharedResource(hShare, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&share_res)))) {
		ID3D11Texture2D* share_tex = NULL;
		if (SUCCEEDED(share_res->QueryInterface(&share_tex))) {
			share_res->Release();
			 return new D3D11Texture(share_tex);
		}
	}
	return nullptr;
}

DXGI_FORMAT ConvertFormat(h3d::SWAPFORMAT format) {
	switch (format) {
	case h3d::R10G10B10A2:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case h3d::BGRA8:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case h3d::BGRX8:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case h3d::B5G6R5A1: case h3d::B5G6R5X1:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case h3d::B5G6R5:
		return DXGI_FORMAT_B5G6R5_UNORM;
	}
	return DXGI_FORMAT_UNKNOWN;
}

#pragma warning(disable:4244)
h3d::D3D11Texture * h3d::D3D11Factory::CreateTexture(SDst Width, SDst Height, SWAPFORMAT Format)
{
	CD3D11_TEXTURE2D_DESC texDesc(ConvertFormat(Format),Width, Height, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);

	ID3D11Texture2D* texture = NULL;
	if (SUCCEEDED(device->CreateTexture2D(&texDesc, NULL, &texture)))
		return new D3D11Texture(texture);

	return nullptr;
}

H3D_API h3d::D3D11Engine & h3d::GetEngine()
{
	static D3D11Engine mInstance;
	return mInstance;
}