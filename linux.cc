#include <nan.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <jpeglib.h>
#include <cstring>
#include <vector>
#include <iostream>

using namespace v8;

class CaptureWorker : public Nan::AsyncWorker {
 public:
  CaptureWorker(int screenIndex, double scale, int quality, Nan::Callback *callback) : Nan::AsyncWorker(callback), screenIndex(screenIndex), scale(scale), quality(quality) {}

  void Execute() {
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
      SetErrorMessage("Failed to open X display");
      return;
    }

    Screen *screen = DefaultScreenOfDisplay(display);
    int screenCount = ScreenCount(display);
    int x = 0, y = 0, width = 0, height = 0;

    if (screenIndex >= 0 && screenIndex < screenCount) {
      screen = ScreenOfDisplay(display, screenIndex);
      width = screen->width;
      height = screen->height;
    } else {
      for (int i = 0; i < screenCount; ++i) {
        Screen *s = ScreenOfDisplay(display, i);
        width += s->width;
        height = std::max(height, s->height);
      }
    }

    XImage *image = XGetImage(display, RootWindowOfScreen(screen), x, y, width, height, AllPlanes, ZPixmap);
    std::cout << "Image depth: " << image->depth << " bits_per_pixel: " << image->bits_per_pixel << std::endl;

    if (!image) {
      XCloseDisplay(display);
      SetErrorMessage("Failed to capture screen image");
      return;
    }

    unsigned long output_size;
    jpegData = EncodeToJPEG(image, width, height, &output_size);

    XDestroyImage(image);
    XCloseDisplay(display);

    if (!jpegData) {
      SetErrorMessage("JPEG encoding failed");
      return;
    }
    resultSize = output_size;
  }

  void HandleOKCallback() {
    Nan::HandleScope scope;
    Local<Value> argv[] = {
        Nan::Null(),
        Nan::CopyBuffer((char *)jpegData, resultSize).ToLocalChecked()};
    callback->Call(2, argv, async_resource);
    delete[] jpegData;
  }

 private:
  int screenIndex;
  double scale;
  int quality;
  unsigned char *jpegData = nullptr;
  size_t resultSize = 0;

  unsigned char *EncodeToJPEG(XImage *img, int width, int height, unsigned long *output_size) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char *outbuffer = nullptr;
    unsigned long outsize = 0;
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

    cinfo.image_width = img->width;
    cinfo.image_height = img->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 75, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = img->bytes_per_line;

    std::vector<unsigned char> row_buffer(img->width * 3);

    while (cinfo.next_scanline < cinfo.image_height) {
      for (int x = 0; x < img->width; x++) {
        long pixel = XGetPixel(img, x, cinfo.next_scanline);
        row_buffer[x * 3 + 0] = (pixel & img->red_mask) >> 16;
        row_buffer[x * 3 + 1] = (pixel & img->green_mask) >> 8;
        row_buffer[x * 3 + 2] = (pixel & img->blue_mask);
      }

      JSAMPLE *row_pointer = (JSAMPLE *)row_buffer.data();
      jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    *output_size = outsize;
    return outbuffer;
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
