// dx2d version 0.1.1
// (c) 2025 mithrilly

#pragma once
#define DIRECTINPUT_VERSION 0x0800
#define DIRECTSOUND_VERSION 0x0800
#include <Windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dinput.h>
#include <dsound.h>
#include <wincodec.h>
#include <mmsystem.h>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dsound.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"WinMM.Lib")

constexpr int graph_max = 1024;


inline LRESULT CALLBACK wndproc(HWND procwinh, UINT procmes, WPARAM procwp, LPARAM proclp) {
	switch (procmes) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(procwinh, procmes, procwp, proclp);
}

class dx2d_core {
	//memo: change to comptr
	HWND hwin = NULL;
	MSG mes = { NULL };
	ID2D1Factory* d2fac = nullptr;
	ID2D1HwndRenderTarget* d2render = nullptr;
	IDirectInput8W* difac = nullptr;
	IDirectInputDevice8W* didev = nullptr;
	IDirectSound8* dsfac = nullptr;

	BYTE keystate[256] = { NULL };

	ID2D1Bitmap* d2bmp[graph_max] = { nullptr };

	template<class T>void srelease(T** pclass) {
		if (*pclass) {
			(*pclass)->Release();
			*pclass = nullptr;
		}
	}
public:
	int initialize(HINSTANCE getinst, LPCWSTR getwinname, int getx, int gety, int getwidth, int getheight) {
		if (FAILED(CoInitialize(NULL)))return 1;

		WNDCLASSW winc;
		winc.style = CS_HREDRAW | CS_VREDRAW;
		winc.lpfnWndProc = wndproc;
		winc.cbClsExtra = 0;
		winc.cbWndExtra = 0;
		winc.hInstance = getinst;
		winc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
		winc.hCursor = LoadCursorW(NULL, IDC_ARROW);
		winc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		winc.lpszMenuName = NULL;
		winc.lpszClassName = getwinname;
		if (!RegisterClassW(&winc))return 1;
		hwin = CreateWindowW(getwinname, getwinname, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, getx, gety, getwidth, getheight, NULL, NULL, getinst, NULL);
		if (!hwin)return 1;

		RECT rwin, rcli;
		GetWindowRect(hwin, &rwin);
		GetClientRect(hwin, &rcli);
		int newwidth = (rwin.right - rwin.left) - (rcli.right - rcli.left) + getwidth;
		int newheight = (rwin.bottom - rwin.top) - (rcli.bottom - rcli.top) + getheight;
		SetWindowPos(hwin, NULL, 0, 0, newwidth, newheight, SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);

		HRESULT hres = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2fac);
		if (SUCCEEDED(hres))hres = d2fac->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwin, D2D1::SizeU(640, 480)), &d2render);
		if (FAILED(hres)) {
			srelease(&d2render);
			srelease(&d2fac);
			return 1;
		}

		hres = DirectInput8Create(getinst, DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&difac, NULL);
		difac->CreateDevice(GUID_SysKeyboard, &didev, NULL);
		didev->SetDataFormat(&c_dfDIKeyboard);
		didev->SetCooperativeLevel(hwin, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

		hres = DirectSoundCreate8(&DSDEVID_DefaultPlayback, &dsfac, NULL);
		dsfac->SetCooperativeLevel(hwin, DSSCL_PRIORITY);

		d2render->BeginDraw();
		return 0;
	}
	int release() {
		for (int i = 0; i < graph_max; i++) {
			srelease(&d2bmp[i]);
		}

		d2render->EndDraw();

		srelease(&dsfac);

		didev->Unacquire();
		srelease(&didev);
		srelease(&difac);

		srelease(&d2render);
		srelease(&d2fac);
		CoUninitialize();
		return 0;
	}
	int procmsg() {
		if (PeekMessageW(&mes, NULL, 0, 0, PM_NOREMOVE)) {
			switch (GetMessageW(&mes, NULL, 0, 0)) {
			case 0:
			case -1:
				return 0;
			}
			DispatchMessageW(&mes);
		}
		HRESULT hres = didev->GetDeviceState(256, keystate);
		if (FAILED(hres)) {
			didev->Acquire();
			didev->GetDeviceState(256, keystate);
		}
		return 1;
	}
	int screenflip() {
		d2render->EndDraw();
		d2render->BeginDraw();
		d2render->Clear(D2D1::ColorF(0, 0, 0));
		return 0;
	}
	int getbmpusable() {
		for (int buff = 0; buff < graph_max; buff++) {
			if (!d2bmp[buff])return buff;
		}
		return -1;
	}
	int loadgraph(LPCWSTR getpath) {
		int buff = getbmpusable();
		if (buff == -1)return -1;
		IWICImagingFactory* wicfac = nullptr;
		IWICBitmapDecoder* wicdec = nullptr;
		IWICBitmapFrameDecode* wicframe = nullptr;
		IWICFormatConverter* wicconv = nullptr;
		HRESULT hres = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicfac));
		if (SUCCEEDED(hres))hres = wicfac->CreateDecoderFromFilename(getpath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &wicdec);
		if (SUCCEEDED(hres))hres = wicdec->GetFrame(0, &wicframe);
		if (SUCCEEDED(hres))hres = wicfac->CreateFormatConverter(&wicconv);
		if (SUCCEEDED(hres))hres = wicconv->Initialize(wicframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeMedianCut);
		if (SUCCEEDED(hres))hres = d2render->CreateBitmapFromWicBitmap(wicconv, &d2bmp[buff]);
		srelease(&wicconv);
		srelease(&wicframe);
		srelease(&wicdec);
		srelease(&wicfac);
		if (FAILED(hres))return -1;
		return buff;
	}
	int deletegraph(int grhandle) {
		srelease(&d2bmp[grhandle]);
		return 0;
	}
	int drawgraph(int grhandle, float getx, float gety, float getext, float getangle, float getalpha) {
		if (!d2bmp[grhandle])return 1;
		D2D1_SIZE_F szbmp = d2bmp[grhandle]->GetSize();
		D2D1_RECT_F rectbmp = { 0.f };
		rectbmp.left = getx - szbmp.width * getext / 2.f;
		rectbmp.top = gety - szbmp.height * getext / 2.f;
		rectbmp.right = getx + szbmp.width * getext / 2.f;
		rectbmp.bottom = gety + szbmp.height * getext / 2.f;
		d2render->SetTransform(D2D1::Matrix3x2F::Rotation(getangle, D2D1::Point2F(getx, gety)));
		d2render->DrawBitmap(d2bmp[grhandle], rectbmp, getalpha);
		return 0;
	}
	int loadsound(LPCWSTR getpath) {
		wchar_t filepath[1024] = { '\0' };
		for (int i = 0; getpath[i] != '\0' || i < 1024; i++) {
			filepath[i] = getpath[i];
		}

		HMMIO hmm = NULL;
		MMIOINFO mminfo;
		MMCKINFO riff, fmt, data;
		WAVEFORMATEX wavefmt;
		memset(&mminfo, 0, sizeof(MMIOINFO));
		if (mmioOpenW((LPWSTR)getpath, &mminfo, MMIO_READ))return -1;
		if (!hmm)return -1;
		riff.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmm, &riff, NULL, MMIO_FINDRIFF);
		fmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmm, &fmt, &riff, MMIO_FINDCHUNK);
		mmioRead(hmm, (HPSTR)&wavefmt, fmt.cksize);
		mmioAscend(hmm, &fmt, 0);
		data.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmm, &data, &riff, MMIO_FINDCHUNK);
		char* wavedata = new char[data.cksize];
		mmioRead(hmm, (HPSTR)wavedata, data.cksize);
		mmioClose(hmm, 0);
		DSBUFFERDESC dsdesc;
		IDirectSoundBuffer* dstmp;
		IDirectSoundBuffer8* dsbuf;
		ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
		dsdesc.dwSize = sizeof(DSBUFFERDESC);
		dsdesc.dwFlags = DSBCAPS_CTRLVOLUME;
		dsdesc.dwBufferBytes = fmt.cksize;
		dsdesc.dwReserved = 0;
		dsdesc.lpwfxFormat = &wavefmt;
		dsdesc.guid3DAlgorithm = DS3DALG_DEFAULT;

		dsfac->CreateSoundBuffer(&dsdesc, &dstmp, 0);
		dstmp->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)&dsbuf);
		dstmp->Release();
	}
	int getinput(UCHAR inputcode) {
		if (keystate[inputcode]) {
			return 1;
		}
		return 0;
	}
};


inline dx2d_core* pdxcore() {
	static dx2d_core* stpdxcore = new dx2d_core;
	return stpdxcore;
}

inline int dx2d_init(HINSTANCE getinst) {
	int res = pdxcore()->initialize(getinst, L"dx2d", CW_USEDEFAULT, 0, 640, 480);
	if (res)delete pdxcore();
	return res;
}

inline int dx2d_end() {
	int res = pdxcore()->release();
	delete pdxcore();
	return res;
}

inline int procmsg() {
	return pdxcore()->procmsg();
}

inline int screenflip() {
	return pdxcore()->screenflip();
}

inline int loadgraph(LPCWSTR getpath) {
	return pdxcore()->loadgraph(getpath);
}

inline int deletegraph(int grhandle) {
	return pdxcore()->deletegraph(grhandle);
}

inline int drawgraphf(int grhandle, float getx, float gety, float getext = 1.f, float getangle = 0.f, float getalpha = 1.f) {
	return pdxcore()->drawgraph(grhandle, getx, gety, getext, getangle, getalpha);
}

inline int drawgraph(int grhandle, double getx, double gety, double getext = 1.0, double getangle = 0.0, double getalpha = 1.0) {
	return pdxcore()->drawgraph(grhandle, (float)getx, (float)gety, (float)getext, (float)getangle, (float)getalpha);
}

//not yet
inline int loadsound(LPCWSTR getpath) {
	return pdxcore()->loadsound(getpath);
}

//ex. DIK_RIGHT : right arrow key
inline int getinput(UCHAR inputcode) {
	return pdxcore()->getinput(inputcode);
}