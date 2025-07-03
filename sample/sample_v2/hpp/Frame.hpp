#pragma once

#include <memory>
#include <set>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "common.hpp"

namespace percipio_layer {

class TYImage
{
  public:
    TYImage();
    TYImage(const TY_IMAGE_DATA& image);
    TYImage(const TYImage& src);
    TYImage(int32_t width, int32_t height, TY_COMPONENT_ID compID, TYPixFmt format, int32_t size);

    ~TYImage();

    int32_t  size()       const { return image_data.size;   }
    int32_t  width()      const { return image_data.width;  }
    int32_t  height()     const { return image_data.height; }
    int32_t  bpp()        const { return 8 * image_data.size / (image_data.width * image_data.height); }
    void*    buffer()     const { return image_data.buffer; }
    int32_t  status()     const { return image_data.status; }
    uint64_t timestamp()  const { return image_data.timestamp; }
    int32_t  imageIndex() const { return image_data.imageIndex; }

    bool     resize(int w, int h);

    TYPixFmt pixelFormat() const { return image_data.pixelFormat; }
    TY_COMPONENT_ID componentID() const { return image_data.componentID; }

    const TY_IMAGE_DATA* image() const { return &image_data; }

  private:
    bool m_isOwner = false;
    TY_IMAGE_DATA image_data;
};

class TYFrame
{
  public:
    ~TYFrame();
    void operator=(TYFrame const&) = delete;
    TYFrame(TYFrame const&) = delete;
    TYFrame(const TY_FRAME_DATA& frame);
 
    std::shared_ptr<TYImage> depthImage()        { return _images[TY_COMPONENT_DEPTH_CAM];}
    std::shared_ptr<TYImage> colorImage()        { return _images[TY_COMPONENT_RGB_CAM];}
    std::shared_ptr<TYImage> leftIRImage()       { return _images[TY_COMPONENT_IR_CAM_LEFT];}
    std::shared_ptr<TYImage> rightIRImage()      { return _images[TY_COMPONENT_IR_CAM_RIGHT];}

  private:
    int32_t               bufferSize = 0;
    std::vector<uint8_t>  userBuffer;

    typedef std::map<TY_COMPONENT_ID, std::shared_ptr<TYImage>> ty_image;
    ty_image              _images;
};

#ifdef OPENCV_DEPENDENCIES
class ImageDisplay
{
  public:
    ImageDisplay();
    ~ImageDisplay();

    int updateWindow(const char* win, const cv::Mat& img);
    void CloseWindow(const char* win);

  private:
    std::atomic<bool> m_running;
    std::thread m_thread;
    void displayThread();

    int m_key;

    std::set<std::string> destroy_win;

    std::mutex      _lock;
    typedef std::map<std::string, cv::Mat> ty_display;
    ty_display displays;
};
#endif

class ImageProcesser
{
  public:
    ImageProcesser(const char* win,  const TY_CAMERA_CALIB_INFO* calib_data = nullptr);
    ~ImageProcesser() {clear();}

    virtual int parse(const std::shared_ptr<TYImage>& image);
    int DepthImageRender();
    TY_STATUS doUndistortion();
    int flush();
    void clear();

    const std::shared_ptr<TYImage>& image() const { return _image; }
    const std::string& win() { return win_name; }

  protected:
    std::shared_ptr<TYImage> _image;

#ifdef OPENCV_DEPENDENCIES
    ImageDisplay* disp_ptr;
#endif

  private:
    std::string win_name;
    std::shared_ptr<TY_CAMERA_CALIB_INFO> _calib_data;

#ifdef OPENCV_DEPENDENCIES
    DepthRender render;
#endif
};


typedef void (*TYFrameKeyBoardEventCallback) (int, void*);
typedef std::map<TY_COMPONENT_ID, std::shared_ptr<ImageProcesser>> ty_stream;
class TYFrameParser
{
  public:
    TYFrameParser(uint32_t max_queue_size = 4);
    ~TYFrameParser();

    void RegisterKeyBoardEventCallback(TYFrameKeyBoardEventCallback cb, void* data) {
      user_data = data;
      func_keyboard_event = cb;
    }

    int setImageProcesser(TY_COMPONENT_ID id, std::shared_ptr<ImageProcesser> proc);
    virtual int doProcess(const std::shared_ptr<TYFrame>& frame);
    void update(const std::shared_ptr<TYFrame>& frame);

protected:
    ty_stream stream;
  private:
    std::mutex      _queue_lock;
    uint32_t        _max_queue_size;

    bool            isRuning;
    std::thread     processThread_;

    void* user_data;
    TYFrameKeyBoardEventCallback     func_keyboard_event;
    std::queue<std::shared_ptr<TYFrame>> images;

    inline void ImageQueueSizeCheck();
    inline void display();
};
}
