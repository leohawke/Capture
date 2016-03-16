#include "d3d8.h" 

#include "CaptureHookCommon.h"
#include "DXGI_D3D10_D3D11.h"

#include <Windows.h>
#include <Shlobj.h>


//TODO Support D3D8EX

#pragma warning(disable:4244)


h3d::SWAPFORMAT D3D8ConvertFormat(D3DFORMAT);

typedef LPDIRECT3D8(WINAPI* D3D8CREATEPROC)(UINT);

#define D3D8ENDSCENE_OFFSET 42
h3d::APIHook d3d8end_scene;
HRESULT __stdcall EndScne(IDirect3DDevice8*);

//Todo Support It
//Don't Support D3D8 Game In Xp And Vista
bool h3d::BeginD3D8CaptureHook() {

	wchar_t sD3D8Path[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, sD3D8Path);
	wcscat_s(sD3D8Path, MAX_PATH, L"\\D3D8.dll");

	HMODULE hD3D8Dll = GetModuleHandle(sD3D8Path);
	if (hD3D8Dll) {
		PTR addr = reinterpret_cast<PTR>(hD3D8Dll);
		PTR end_sceneoffset = 0;

		UINT version = GetOSVersion();

		if (version == 7)
			end_sceneoffset = 0x44530;
		if (version >= 8)
			end_sceneoffset = 0x33540;

		if (end_sceneoffset) {
			d3d8end_scene.Do((WINAPIPROC)(addr + end_sceneoffset), (WINAPIPROC)EndScne);
			return true;
		}
	}

	return false;
}

void D3D8CaptureSetup(IDirect3DDevice8* device);

HRESULT __stdcall EndScne(IDirect3DDevice8* device) {
	//EnterCriticalSection
	d3d8end_scene.UnDo();
#ifdef _DEBUG
	//logstream << "D3D8EndScne called " << std::endl;
#endif

	if (!target_acquired && capture_run) {
		D3D8CaptureSetup(device);
		target_acquired = true;
	}

	HRESULT hr = device->EndScene();
	d3d8end_scene.ReDo();

	//LeaveCriticalSection
	return hr;
}


#define D3D8PRESENT_OFFSET 15
h3d::APIHook d3d8present;
HRESULT __stdcall D3D8Present(IDirect3DDevice8 *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);

#define D3D8RESET_OFFSET 16
h3d::APIHook d3d8reset;
HRESULT __stdcall D3D8Reset(IDirect3DDevice8 *device, D3DPRESENT_PARAMETERS* params);


h3d::CaptureInfo D3D8_captureinfo;
DWORD D3D8_format;
void D3D8CaptureSetup(IDirect3DDevice8* device) {
	IDirect3DSurface8* back_buffer = NULL;
	//ͨ����������Ϣ����ȡ�����Ϣ
	if (SUCCEEDED(device->GetRenderTarget(&back_buffer))) {
		D3DSURFACE_DESC desc;
		if (SUCCEEDED(back_buffer->GetDesc(&desc))) {
			//D3D8_captureinfo.format = d3dpp.BackBufferFormat;
			//todo format check

			if (D3D8_format != desc.Format ||
				D3D8_captureinfo.oWidth != desc.Width || D3D8_captureinfo.oHeight != desc.Height) {
				//todo printf prsents info

				D3D8_format = desc.Format;
				D3D8_captureinfo.oWidth = desc.Width;
				D3D8_captureinfo.oHeight = desc.Height;
				D3D8_captureinfo.Reserved1 = D3D8ConvertFormat(desc.Format);
			}
		}
		else
			logstream << "Don't have enough infomation to hook(failed to get present params)" << std::endl;

		//todo D3D8EX/PresentEx
		d3d8present.Do(h3d::GetVirtual(device, D3D8PRESENT_OFFSET), (h3d::WINAPIPROC)D3D8Present);
		d3d8reset.Do(h3d::GetVirtual(device, D3D8RESET_OFFSET), (h3d::WINAPIPROC)D3D8Reset);

		back_buffer->Release();
		logstream << "D3D8Capture Succesfully Set Up D3D8 HOOKS" << std::endl;
	}
	else
		logstream << "D3D8Capture Faild to get GetRenderTarget to initialize hooks" << std::endl;
}

h3d::SWAPFORMAT D3D8ConvertFormat(D3DFORMAT format) {
	//https://msdn.microsoft.com/en-us/library/windows/desktop/bb172558(v=vs.85).aspx#BackBuffer_or_Display_Formats
	switch (format) {
	case D3DFMT_A8R8G8B8: return h3d::BGRA8;
	case D3DFMT_X8R8G8B8: return h3d::BGRX8;//�����ʹ��FFMPEG������Ҫ�����ʽ
	case D3DFMT_A1R5G5B5: return h3d::B5G6R5A1;
	case D3DFMT_X1R5G5B5:return h3d::B5G6R5X1;
	case D3DFMT_R5G6B5: return h3d::B5G6R5;
	}
	return h3d::SWAPFORMAT_UNKNOWN;
}

void D3D8Capture(IDirect3DDevice8* device);

HRESULT __stdcall D3D8Present(IDirect3DDevice8 *device, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) {
	d3d8present.UnDo();

#ifdef _DEBUG
	//logstream << "D3D8Present called" << std::endl;
#endif

	//todo some code protect fps or other things 

	//todo get the surface image
	D3D8Capture(device);

	HRESULT hr = device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	d3d8present.ReDo();

	return hr;
}

void D3D8Flush();

HRESULT __stdcall D3D8Reset(IDirect3DDevice8 *device, D3DPRESENT_PARAMETERS* params) {

#ifdef _DEBUG
	//logstream << "D3D8Rest called" << std::endl;
#endif

	D3D8Flush();

	if (!target_acquired && capture_run) {
		D3D8CaptureSetup(device);//Don't worry that hookapi will deal with second hook do(undo first)
		target_acquired = true;
	}

	//todo clear data/build data
	d3d8reset.UnDo();
	HRESULT hr = device->Reset(params);
	d3d8reset.ReDo();

	return hr;
}

namespace {
	bool has_textured = false;
}

bool CreateCPUCapture(IDirect3DDevice8* device);

#define NUM_BACKBUFF 3

namespace {
	//CPU copy stuff
	IDirect3DSurface8 * D3D8_textures[NUM_BACKBUFF] = { 0,0,0 };
	bool locked_textures[NUM_BACKBUFF] = { 0,0,0 };
	UINT tex_pitch = 0;

	CRITICAL_SECTION data_mutexs[NUM_BACKBUFF];

	void* copy_data = NULL;
	UINT copy_index = 0;
	HANDLE copy_event = NULL;
	HANDLE copy_thread = NULL;
	bool copy_thread_run = true;
	h3d::MemoryInfo* copy_info = NULL;
	h3d::byte* tex_addrsss[2] = { NULL,NULL };
	static DWORD copy_wait = 0;

	DWORD curr_capture = 0;
}

void D3D8Capture(IDirect3DDevice8* device) {
	h3d::EventProcess();

	if (!has_textured && capture_run) {
		if (!CreateCPUCapture(device))
			D3D8Flush();
		else
			has_textured = true;
	}

	KeepAliveProcess(D3D8Flush());

	if (has_textured && capture_run) {
		//todo fps maintain

		IDirect3DSurface8* back_buffer;
		if (FAILED(device->GetRenderTarget(&back_buffer)))
			return;
		//copy texture data only when GetRenderTargetData completes
		for (UINT i = 0; i != NUM_BACKBUFF; ++i) {
			if (!locked_textures[i]) {
				if (FAILED(device->CopyRects(back_buffer, NULL, 0, D3D8_textures[i], NULL)))
					return;

				D3DLOCKED_RECT lr;
				if (SUCCEEDED(D3D8_textures[i]->LockRect(&lr, NULL, D3DLOCK_READONLY))) {
					locked_textures[i] = true;
					copy_data = lr.pBits;
					copy_index = i;
					SetEvent(copy_event);
				}
			}
		}

		back_buffer->Release();

		DWORD next_capture = (curr_capture == NUM_BACKBUFF - 1) ? 0 : (curr_capture + 1);
		if (locked_textures[next_capture]) {
			EnterCriticalSection(&data_mutexs[next_capture]);
			locked_textures[next_capture] = false;
			D3D8_textures[next_capture]->UnlockRect();
			LeaveCriticalSection(&data_mutexs[next_capture]);
		}

		curr_capture = next_capture;
	}
	else if (has_textured)
		D3D8Flush();
}

DWORD CopyD3D8TextureThread(LPVOID lpUseless);

bool CreateCPUCapture(IDirect3DDevice8* device) {

	for (UINT i = 0; i != NUM_BACKBUFF; ++i) {
		if (FAILED(device->CreateImageSurface(D3D8_captureinfo.oWidth, D3D8_captureinfo.oHeight,
			(D3DFORMAT)D3D8_format, &D3D8_textures[i]))) {
			logstream << "CPU Texture IDirect3DDevice8::CreateOffscreenPlainSurface " << i << "th failed" << std::endl;
			return false;
		}
		InitializeCriticalSection(&data_mutexs[i]);
	}

	//try lock get pitch

	D3DLOCKED_RECT lr;
	if (FAILED(D3D8_textures[NUM_BACKBUFF - 1]->LockRect(&lr, NULL, D3DLOCK_READONLY))) {
		logstream << "CPU Texture Lock Failed,can't get pitch" << std::endl;
	}
	tex_pitch = lr.Pitch;
	D3D8_textures[NUM_BACKBUFF - 1]->UnlockRect();



	copy_thread_run = true;
	if (copy_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyD3D8TextureThread, NULL, 0, NULL)) {
		if (!(copy_event = CreateEvent(NULL, FALSE, FALSE, NULL))) {
			logstream << "Create CopyEvent Failed" << std::endl;
			return false;
		}
	}
	else
		return false;


	D3D8_captureinfo.Reserved2 = h3d::CtorSharedMemCPUCapture(tex_pitch*D3D8_captureinfo.oHeight, D3D8_captureinfo.Reserved3, copy_info, tex_addrsss);

	if (!D3D8_captureinfo.Reserved2) {
		logstream << "Create Shared Memory Failed" << std::endl;
		return false;
	}

	has_textured = true;
	D3D8_captureinfo.Reserved4 = tex_pitch;
	memcpy(ptr_capture_info, &D3D8_captureinfo, sizeof(h3d::CaptureInfo));
	SetEvent(hReadyEvent);
	logstream << "Allthing has ready,HOOK Success[Sent Event to CaptureApp]" << std::endl;
	return true;
}

DWORD CopyD3D8TextureThread(LPVOID lpUseless) {
	logstream << "Begin CopyD3D8TextureThread" << std::endl;

	HANDLE hEvent = NULL;
	if (!DuplicateHandle(GetCurrentProcess(), copy_event, GetCurrentProcess(), &hEvent, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		logstream << "CopyD3D8TextureThread DuplicateHandle Failed" << std::endl;
	}

	bool address_index = false;

	//�õ��¼�����ʱ��ʼ����
	while ((WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) && copy_thread_run) {
		bool next_address_index = !address_index;

		DWORD local_copy_index = copy_index;
		LPVOID local_copy_data = copy_data;

		if (local_copy_index < NUM_BACKBUFF && local_copy_data != NULL) {
			EnterCriticalSection(&data_mutexs[local_copy_index]);

			int last_rendered = -1;

			if (WaitForSingleObject(texture_mutex[address_index], 0) == WAIT_OBJECT_0)
				last_rendered = address_index;
			else if (WaitForSingleObject(texture_mutex[next_address_index], 0) == WAIT_OBJECT_0)
				last_rendered = next_address_index;


			if (last_rendered != -1) {
				memcpy(tex_addrsss[last_rendered], local_copy_data, tex_pitch*D3D8_captureinfo.oHeight);
				ReleaseMutex(texture_mutex[last_rendered]);
				copy_info->Reserved3 = last_rendered;
			}

			LeaveCriticalSection(&data_mutexs[local_copy_index]);
		}

		address_index = next_address_index;
	}

	CloseHandle(hEvent);
	logstream << "Exit CopyD3D8TextureThread" << std::endl;
	return 0;
}


#define SR(var) if(var) {var->Release();var = NULL;}

void D3D8Flush() {
	has_textured = false;

	if (copy_thread) {
		copy_thread_run = false;
		SetEvent(copy_event);
		//ǿ���ս��߳�
		if (WaitForSingleObject(copy_thread, 500) != WAIT_OBJECT_0)
			TerminateThread(copy_thread, 0);

		CloseHandle(copy_thread);
		CloseHandle(copy_data);

		copy_thread = NULL;
		copy_event = NULL;
	}

	for (int i = 0; i != NUM_BACKBUFF; ++i) {
		if (locked_textures[i]) {
			EnterCriticalSection(&data_mutexs[i]);

			D3D8_textures[i]->UnlockRect();
			locked_textures[i] = false;

			LeaveCriticalSection(&data_mutexs[i]);
		}
		SR(D3D8_textures[i]);

		DeleteCriticalSection(&data_mutexs[i]);
	}

	h3d::DestroySharedMem();
	copy_data = NULL;
	copy_info = NULL;
	copy_wait = 0;
	copy_index = 0;


	curr_capture = 0;

	logstream << "Flush D3D8 Capture" << std::endl;
}


void h3d::EndD3D8CaptureHook() {
	d3d8end_scene.UnDo();
	d3d8reset.UnDo();
	d3d8present.UnDo();
	D3D8Flush();
}

