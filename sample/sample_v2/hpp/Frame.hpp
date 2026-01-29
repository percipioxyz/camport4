#pragma once

#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <algorithm>

#include "common.hpp"

namespace percipio_layer {

class TYImage
{
public:
    TYImage();
    explicit TYImage(const TY_IMAGE_DATA& image, float f_scale_unit = 1.0f);
    explicit TYImage(const TYImage& src);
    TYImage(int32_t width, int32_t height, TY_COMPONENT_ID compID, 
            TYPixFmt format, int32_t size);
    
    TYImage& operator=(const TYImage& src);
    
    ~TYImage();

    int32_t size() const noexcept { return image_data.size; }
    int32_t width() const noexcept { return image_data.width; }
    int32_t height() const noexcept { return image_data.height; }
    int32_t bpp() const noexcept { 
        return (image_data.width * image_data.height > 0) ? 
               8 * image_data.size / (image_data.width * image_data.height) : 0; 
    }
    
    void* buffer() const noexcept { return image_data.buffer; }
    int32_t status() const noexcept { return image_data.status; }
    uint64_t timestamp() const noexcept { return image_data.timestamp; }
    int32_t imageIndex() const noexcept { return image_data.imageIndex; }

    TYPixFmt pixelFormat() const noexcept { return image_data.pixelFormat; }
    TY_COMPONENT_ID componentID() const noexcept { return image_data.componentID; }

    float DepthScaleUnit() const noexcept { return scale_unit; }
    
    const TY_IMAGE_DATA* image() const noexcept { return &image_data; }
    
    bool isValid() const noexcept { return image_data.buffer != nullptr && image_data.size > 0; }

private:
    bool m_isOwner = false;
    TY_IMAGE_DATA image_data{};
    float scale_unit = 1.0f;
    
    void cleanup() noexcept;
    void copyFrom(const TYImage& src);
};

class TYFrame
{
public:
    ~TYFrame() = default;
    
    TYFrame(const TYFrame&) = delete;
    TYFrame& operator=(const TYFrame&) = delete;
    
    TYFrame(TYFrame&&) = delete;
    TYFrame& operator=(TYFrame&&) = delete;
    
    explicit TYFrame(const TY_FRAME_DATA& frame);

    std::shared_ptr<TYImage> depthImage() { return getImage(TY_COMPONENT_DEPTH_CAM); }
    std::shared_ptr<TYImage> colorImage() { return getImage(TY_COMPONENT_RGB_CAM); }
    std::shared_ptr<TYImage> leftIRImage() { return getImage(TY_COMPONENT_IR_CAM_LEFT); }
    std::shared_ptr<TYImage> rightIRImage() { return getImage(TY_COMPONENT_IR_CAM_RIGHT); }
    
    bool hasImage(TY_COMPONENT_ID compID) const {
        return _images.find(compID) != _images.end();
    }

private:
    std::vector<uint8_t> userBuffer;
    std::map<TY_COMPONENT_ID, std::shared_ptr<TYImage>> _images;
    
    std::shared_ptr<TYImage> getImage(TY_COMPONENT_ID compID) {
        auto it = _images.find(compID);
        return (it != _images.end()) ? it->second : nullptr;
    }
};

class ImageDisplay
{
public:
    static ImageDisplay& getInstance() {
        static ImageDisplay instance;
        return instance;
    }
    
    ImageDisplay(const ImageDisplay&) = delete;
    ImageDisplay& operator=(const ImageDisplay&) = delete;
    
    int updateWindow(const std::string& win, std::shared_ptr<TYImage> img);
    void closeWindow(const std::string& win);
    void destroyAllWindows();
    
    void stop();

private:
    ImageDisplay();
    ~ImageDisplay();
    
    std::atomic<bool> m_running{true};
    std::thread m_thread;
    
    std::atomic<int> m_key{0};
    mutable std::mutex m_mutex;
    
    struct DisplayItem {
        std::shared_ptr<TYImage> image;
        bool updated = false;
        
        DisplayItem() = default;
        DisplayItem(std::shared_ptr<TYImage> img, bool upd) : image(img), updated(upd) {}
    };
    
    std::map<std::string, DisplayItem> displays;
    std::set<std::string> windowsToDestroy;
    
    void displayThread();
};

class ImageProcesser
{
public:
    explicit ImageProcesser(const std::string& win, 
                          const TY_CAMERA_CALIB_INFO* calib_data = nullptr);
    ~ImageProcesser() { clear(); }
    
    ImageProcesser(const ImageProcesser&) = delete;
    ImageProcesser& operator=(const ImageProcesser&) = delete;
    
    virtual int parse(const std::shared_ptr<TYImage>& image);
    TY_STATUS doUndistortion();
    int flush();
    void clear();
    
    const std::shared_ptr<TYImage>& image() const { return _image; }
    const std::string& win() const { return win_name; }
    
    bool isValid() const { return _image != nullptr; }

protected:
    std::shared_ptr<TYImage> _image;

private:
    std::string win_name;
    std::shared_ptr<TY_CAMERA_CALIB_INFO> _calib_data;
    bool has_win;
    mutable std::mutex image_mutex;
};

using TYFrameKeyBoardEventCallback = std::function<void(int, void*)>;
using ImageProcesserMap = std::map<TY_COMPONENT_ID, std::shared_ptr<ImageProcesser>>;

class TYFrameParser
{
public:
    explicit TYFrameParser(uint32_t max_queue_size = 4);
    ~TYFrameParser();
    
    TYFrameParser(const TYFrameParser&) = delete;
    TYFrameParser& operator=(const TYFrameParser&) = delete;

    void RegisterKeyBoardEventCallback(TYFrameKeyBoardEventCallback cb, void* data) {
        user_data = data;
        func_keyboard_event = std::move(cb);
    }

    int setImageProcesser(TY_COMPONENT_ID id, std::shared_ptr<ImageProcesser> proc);
    virtual int doProcess(const std::shared_ptr<TYFrame>& frame);
    void update(const std::shared_ptr<TYFrame>& frame);
    
    void stop();

protected:
    ImageProcesserMap stream;

private:
    mutable std::mutex queue_mutex;
    std::condition_variable cv;
    uint32_t max_queue_size;
    
    std::atomic<bool> isRunning{true};
    std::thread processThread;
    
    void* user_data = nullptr;
    TYFrameKeyBoardEventCallback func_keyboard_event;
    std::queue<std::shared_ptr<TYFrame>> images;
    
    void processThreadFunc();
    void imageQueueSizeCheck();
};

} // namespace percipio_layer
