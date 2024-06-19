// mac.cc
#include <nan.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreServices/CoreServices.h>
#include <iostream>

using namespace v8;

CFDataRef CreateJPEGDataFromCGImage(CGImageRef image, CGFloat compressionQuality) {
  CFMutableDataRef jpegData = CFDataCreateMutable(kCFAllocatorDefault, 0);
  CGImageDestinationRef destination = CGImageDestinationCreateWithData(jpegData, kUTTypeJPEG, 1, NULL);
  CFNumberRef qualityRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &compressionQuality);
  CFDictionaryRef options = CFDictionaryCreate(NULL, (const void**)&kCGImageDestinationLossyCompressionQuality, (const void**)&qualityRef, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CGImageDestinationAddImage(destination, image, options);
  CFRelease(options);
  CFRelease(qualityRef);

  if (CGImageDestinationFinalize(destination)) {
    CFRelease(destination);
    return jpegData;
  }

  CFRelease(destination);
  CFRelease(jpegData);
  return NULL;
}

class CaptureWorker : public Nan::AsyncWorker {
public:
  CaptureWorker(Nan::Callback *callback, int screenIndex, double scale, double quality) : Nan::AsyncWorker(callback), screenIndex(screenIndex), scale(scale), quality(quality) {}

  void Execute() {
    CGImageRef image = nullptr;
    if (screenIndex >= 0) {
      uint32_t displayCount;
      CGGetActiveDisplayList(0, NULL, &displayCount);
      CGDirectDisplayID* displays = new CGDirectDisplayID[displayCount];
      CGGetActiveDisplayList(displayCount, displays, &displayCount);

      if (screenIndex < static_cast<int>(displayCount)) {
          image = CGDisplayCreateImage(displays[screenIndex]);
      } else {
        delete[] displays;
        SetErrorMessage("Invalid screen index");
        return;
      }
      delete[] displays;
    } else {
      image = CGWindowListCreateImage(CGRectInfinite, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
    }

    if (image) {
      size_t width = CGImageGetWidth(image) * scale;
      size_t height = CGImageGetHeight(image) * scale;

      if (scale != 1.0) {
        CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, 0, CGImageGetColorSpace(image), kCGImageAlphaPremultipliedLast);
        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
        CGImageRef scaledImage = CGBitmapContextCreateImage(context);
        CGContextRelease(context);
        CGImageRelease(image);
        image = scaledImage;
      }

      CFDataRef jpegData = CreateJPEGDataFromCGImage(image, quality);
      CGImageRelease(image);

      if (jpegData) {
        jpegBuffer.assign(CFDataGetBytePtr(jpegData), CFDataGetBytePtr(jpegData) + CFDataGetLength(jpegData));
        CFRelease(jpegData);
      } else {
        SetErrorMessage("Failed to convert to JPEG.");
      }
    } else {
      SetErrorMessage("Failed to capture screen.");
    }
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = {
      Nan::Null(),
      Nan::CopyBuffer(jpegBuffer.data(), jpegBuffer.size()).ToLocalChecked()
    };
    callback->Call(2, argv, async_resource);
  }

private:
  int screenIndex;
  double scale;
  double quality;
  std::vector<char> jpegBuffer;
};

NAN_METHOD(Capture) {
  int index = Nan::To<int>(info[0]).FromJust();
  double scale = Nan::To<double>(info[1]).FromJust();
  int qualityInput = Nan::To<int>(info[2]).FromJust();
  double quality = qualityInput / 100.0;
  Nan::Callback *callback = new Nan::Callback(info[3].As<Function>());

  Nan::AsyncQueueWorker(new CaptureWorker(callback, index, scale, quality));
}

NAN_MODULE_INIT(Initialize) {
  Nan::SetMethod(target, "capture", Capture);
}

NODE_MODULE(capture, Initialize)
