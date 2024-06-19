#include <nan.h>
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <shellscalingapi.h>
#pragma comment(lib, "Gdiplus.lib")

using namespace Gdiplus;
using namespace v8;

class CaptureWorker : public Nan::AsyncWorker {
public:
  CaptureWorker(int index, double scale, ULONG quality, Nan::Callback* callback) : Nan::AsyncWorker(callback), index(index), scale(scale), quality(quality) {}
  ~CaptureWorker() {}

  void Execute() {
    SetDpiAware();

    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    HDC hScreenDC;
    int x = 0, y = 0, width = 0, height = 0;
    if (index == -1) {
      width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
      x = GetSystemMetrics(SM_XVIRTUALSCREEN);
      y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    } else {
      DISPLAY_DEVICE dd;
      dd.cb = sizeof(dd);
      EnumDisplayDevices(NULL, index, &dd, 0);
      DEVMODE dm;
      dm.dmSize = sizeof(dm);
      EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm);
      x = dm.dmPosition.x;
      y = dm.dmPosition.y;
      width = dm.dmPelsWidth;
      height = dm.dmPelsHeight;
    }

    hScreenDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, x, y, SRCCOPY);
    Bitmap* bmp = new Bitmap(hBitmap, NULL);

    int newWidth = static_cast<int>(width * scale);
    int newHeight = static_cast<int>(height * scale);
    buffer = BitmapToJpegBuffer(bmp, quality, newWidth, newHeight);

    delete bmp;
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    DeleteDC(hScreenDC);

    GdiplusShutdown(gdiplusToken);
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    Local<Value> argv[] = {
      Nan::Null(),
      Nan::CopyBuffer(reinterpret_cast<char*>(buffer.data()), buffer.size()).ToLocalChecked()
    };
    callback->Call(2, argv);
  }

private:
  int index;
  double scale;
  ULONG quality;
  std::vector<BYTE> buffer;

  BOOL SetDpiAware() {
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    if (osvi.dwMajorVersion >= 6) {
      HMODULE hShcore = LoadLibrary(TEXT("Shcore.dll"));
      if (hShcore) {
        typedef HRESULT(WINAPI *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        SetProcessDpiAwarenessFunc SetProcessDpiAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (SetProcessDpiAwareness)
          SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);
        FreeLibrary(hShcore);
      } else {
        HMODULE hUser32 = LoadLibrary(TEXT("User32.dll"));
        if (hUser32) {
          typedef BOOL(WINAPI *SetProcessDPIAwareFunc)(void);
          SetProcessDPIAwareFunc SetProcessDPIAware = (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
          if (SetProcessDPIAware)
            SetProcessDPIAware();
          FreeLibrary(hUser32);
        }
      }
    }
    return TRUE;
  }

  int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return -1;
    }

    std::vector<BYTE> buffer(size);
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(&buffer[0]);
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
      if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
        *pClsid = pImageCodecInfo[j].Clsid;
        return j;
      }
    }
    return -1;
  }

  std::vector<BYTE> BitmapToJpegBuffer(Gdiplus::Bitmap* bmp, ULONG quality, int scaledWidth, int scaledHeight) {
    std::vector<BYTE> buffer;
    IStream* stream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    CLSID clsid;

    if (GetEncoderClsid(L"image/jpeg", &clsid) == -1)
      return buffer;

    Gdiplus::Bitmap* scaledBmp = new Gdiplus::Bitmap(scaledWidth, scaledHeight, bmp->GetPixelFormat());
    Gdiplus::Graphics graphics(scaledBmp);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(bmp, 0, 0, scaledWidth, scaledHeight);

    EncoderParameters encoderParameters;
    encoderParameters.Count = 1;
    encoderParameters.Parameter[0].Guid = EncoderQuality;
    encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParameters.Parameter[0].NumberOfValues = 1;
    encoderParameters.Parameter[0].Value = &quality;

    scaledBmp->Save(stream, &clsid, &encoderParameters);

    LARGE_INTEGER li = {0};
    ULARGE_INTEGER uli = {0};
    stream->Seek(li, STREAM_SEEK_SET, &uli);
    STATSTG stg = {0};
    stream->Stat(&stg, STATFLAG_NONAME);
    buffer.resize(stg.cbSize.LowPart);
    stream->Read(&buffer[0], stg.cbSize.LowPart, NULL);
    stream->Release();
    delete scaledBmp;
    return buffer;
  }
};

NAN_METHOD(Capture) {
  int index = Nan::To<int>(info[0]).FromJust();
  double scale = Nan::To<double>(info[1]).FromJust();
  int quality = Nan::To<int>(info[2]).FromJust();
  Nan::Callback *callback = new Nan::Callback(info[3].As<Function>());
  Nan::AsyncQueueWorker(new CaptureWorker(index, scale, quality, callback));
}

NAN_MODULE_INIT(Initialize) {
  Nan::SetMethod(target, "capture", Capture);
}

NODE_MODULE(capture, Initialize)
