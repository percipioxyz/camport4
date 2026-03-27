#include <thread>
#include <chrono>
#include <cstring>
#include "Frame.hpp"
#include "TYImageProc.h"
#include "common.hpp"

namespace percipio_layer {

// TYImage
TYImage::TYImage()
{
    std::memset(&image_data, 0, sizeof(image_data));
}

TYImage::TYImage(const TY_IMAGE_DATA& image, float f_scale_unit)
{
    TYImageInfo image_info = ty_image_info(image);
    uint32_t destSize = 0;
    
    TYDecodeError decodeErr = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_AUTO);
    if (decodeErr == TY_DECODE_SUCCESS && destSize > 0) {
        image_data.buffer = std::malloc(destSize);
        if (image_data.buffer) {
            TYDecodeResult retInfo;
            TYDecodeError decodeStatus = TYDecodeImage(&image_info, TY_OUTPUT_FORMAT_AUTO, image_data.buffer, destSize, &retInfo);
            
            if (decodeStatus == TY_DECODE_SUCCESS) {
                m_isOwner = true;
                image_data.timestamp = image.timestamp;
                image_data.imageIndex = image.imageIndex;
                image_data.status = image.status;
                image_data.componentID = image.componentID;
                image_data.size = static_cast<int32_t>(retInfo.dataSize);
                image_data.width = retInfo.width;
                image_data.height = retInfo.height;
                image_data.pixelFormat = retInfo.format;
            } else {
                std::free(image_data.buffer);
                std::memset(&image_data, 0, sizeof(image_data));
            }
        }
    }
    
    if (!image_data.buffer) {
        // If decoding fails, directly copy the original data.
        m_isOwner = true;
        image_data.timestamp = image.timestamp;
        image_data.imageIndex = image.imageIndex;
        image_data.status = image.status;
        image_data.componentID = image.componentID;
        image_data.size = image.size;
        image_data.width = image.width;
        image_data.height = image.height;
        image_data.pixelFormat = image.pixelFormat;
        
        if (image.size > 0 && image.buffer) {
            image_data.buffer = std::malloc(image.size);
            if (image_data.buffer) {
                std::memcpy(image_data.buffer, image.buffer, image.size);
            } else {
                std::memset(&image_data, 0, sizeof(image_data));
                m_isOwner = false;
            }
        }
    }
    
    scale_unit = f_scale_unit;
}

TYImage::TYImage(const TYImage& src)
{
    copyFrom(src);
}

TYImage::TYImage(int32_t width, int32_t height, TY_COMPONENT_ID compID, 
                TYPixFmt format, int32_t size)
{
    image_data.size = size;
    image_data.width = width;
    image_data.height = height;
    image_data.componentID = compID;
    image_data.pixelFormat = format;
    
    if (size > 0) {
        image_data.buffer = std::malloc(size);
        m_isOwner = (image_data.buffer != nullptr);
        if (image_data.buffer) {
            std::memset(image_data.buffer, 0, size);
        }
    }
}

void TYImage::copyFrom(const TYImage& src)
{
    cleanup();
    
    image_data.timestamp = src.image_data.timestamp;
    image_data.imageIndex = src.image_data.imageIndex;
    image_data.status = src.image_data.status;
    image_data.componentID = src.image_data.componentID;
    image_data.size = src.image_data.size;
    image_data.width = src.image_data.width;
    image_data.height = src.image_data.height;
    image_data.pixelFormat = src.image_data.pixelFormat;
    scale_unit = src.scale_unit;
    
    if (src.image_data.size > 0 && src.image_data.buffer) {
        m_isOwner = true;
        image_data.buffer = std::malloc(src.image_data.size);
        if (image_data.buffer) {
            std::memcpy(image_data.buffer, src.image_data.buffer, src.image_data.size);
        } else {
            std::memset(&image_data, 0, sizeof(image_data));
            m_isOwner = false;
        }
    } else {
        m_isOwner = false;
    }
}

TYImage& TYImage::operator=(const TYImage& src)
{
    if (this != &src) {
        copyFrom(src);
    }
    return *this;
}

void TYImage::cleanup() noexcept
{
    if (m_isOwner && image_data.buffer) {
        std::free(image_data.buffer);
    }
    std::memset(&image_data, 0, sizeof(image_data));
    m_isOwner = false;
}

TYImage::~TYImage()
{
    cleanup();
}

// TYFrame 
TYFrame::TYFrame(const TY_FRAME_DATA& frame)
{
    userBuffer.resize(frame.bufferSize);
    if (frame.bufferSize > 0 && frame.userBuffer) {
        std::memcpy(userBuffer.data(), frame.userBuffer, frame.bufferSize);
    }

    for (int i = 0; i < frame.validCount; i++) {
        if (frame.image[i].status != TY_STATUS_OK) continue;
        
        TY_COMPONENT_ID compID = frame.image[i].componentID;
        if (compID == TY_COMPONENT_DEPTH_CAM || compID == TY_COMPONENT_RGB_CAM ||
            compID == TY_COMPONENT_IR_CAM_LEFT || compID == TY_COMPONENT_IR_CAM_RIGHT) {
            
            TY_IMAGE_DATA img = frame.image[i];
            
            if (img.buffer && frame.userBuffer) {
                size_t offset = reinterpret_cast<uintptr_t>(img.buffer) - 
                              reinterpret_cast<uintptr_t>(frame.userBuffer);
                if (offset < frame.bufferSize) {
                    img.buffer = userBuffer.data() + offset;
                } else {
                    img.buffer = nullptr;
                }
            }
            
            if (img.buffer && img.size > 0) {
                _images[compID] = std::make_shared<TYImage>(img);
            }
        }
    }
}

// ImageDisplay 
ImageDisplay::ImageDisplay()
{
    m_thread = std::thread(&ImageDisplay::displayThread, this);
}

ImageDisplay::~ImageDisplay()
{
    stop();
}

void ImageDisplay::stop()
{
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

int ImageDisplay::updateWindow(const std::string& win, std::shared_ptr<TYImage> img)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (img && img->isValid()) {
        displays[win] = DisplayItem(img, true);
    }
    
    int key = m_key.exchange(0);
    return key;
}

void ImageDisplay::closeWindow(const std::string& win)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    displays.erase(win);
    windowsToDestroy.insert(win);
}

void ImageDisplay::destroyAllWindows()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    displays.clear();
    windowsToDestroy.clear();
}

void ImageDisplay::displayThread()
{
    const int displayIntervalMs = 33; //about 30FPS
    
    while (m_running) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& display : displays) {
                const std::string& win = display.first;
                DisplayItem& item = display.second;
                if (item.updated && item.image && item.image->isValid()) {
                    TYDisplayImage(win.c_str(), 
                                 item.image->width(), 
                                 item.image->height(),
                                 item.image->pixelFormat(), 
                                 item.image->buffer(), 
                                 item.image->DepthScaleUnit());
                    item.updated = false;
                }
            }
            
            if (!windowsToDestroy.empty()) {
                windowsToDestroy.clear();
            }
        }
        
        int key = TYWaitKeyEvents();
        if (key > 0) {
            m_key = key;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(displayIntervalMs));
    }
}

// ImageProcesser
ImageProcesser::ImageProcesser(const std::string& win, 
                             const TY_CAMERA_CALIB_INFO* calib_data)
    : win_name(win), has_win(false)
{
    if (calib_data) {
        _calib_data = std::make_shared<TY_CAMERA_CALIB_INFO>(*calib_data);
    }
}

int ImageProcesser::parse(const std::shared_ptr<TYImage>& image)
{
    std::lock_guard<std::mutex> lock(image_mutex);
    
    if (!image || !image->isValid()) {
        return -1;
    }
    
    TYPixFmt format = image->pixelFormat();
    
    if (format == TYPixelFormatCoord3D_ABC16) {
        int32_t pixelCount = image->width() * image->height();
        std::vector<int16_t> depth_data(pixelCount);
        
        const int16_t* src = static_cast<const int16_t*>(image->buffer());
        if (src) {
            for (int i = 0; i < pixelCount; ++i) {
                depth_data[i] = src[i * 3 + 2];
            }
            
            _image = std::make_shared<TYImage>(image->width(), image->height(),
                                              image->componentID(), TYPixelFormatCoord3D_C16,
                                              static_cast<int32_t>(depth_data.size() * sizeof(int16_t)));
            if (_image->buffer()) {
                std::memcpy(_image->buffer(), depth_data.data(), depth_data.size() * sizeof(int16_t));
            }
        }
    } else {
        _image = std::make_shared<TYImage>(*image);
    }
    
    return 0;
}

TY_STATUS ImageProcesser::doUndistortion()
{
    std::lock_guard<std::mutex> lock(image_mutex);
    
    if (!_calib_data || !_image || !_image->isValid()) {
        return TY_STATUS_ERROR;
    }
    
    std::vector<uint8_t> undistort_image(_image->size());
    
    TY_IMAGE_DATA dst;
    dst.width = _image->width(),
    dst.height = _image->height(),
    dst.size = _image->size(),
    dst.pixelFormat = _image->pixelFormat(),
    dst.buffer = undistort_image.data();
    
    TY_STATUS status = TYUndistortImage(_calib_data.get(), _image->image(), nullptr, &dst);
    if (status == TY_STATUS_OK) {
        _image = std::make_shared<TYImage>(dst);
    }
    
    return status;
}

int ImageProcesser::flush()
{
    std::lock_guard<std::mutex> lock(image_mutex);
    
    if (!_image || !_image->isValid()) {
        return -1;
    }
    has_win = true;
    return ImageDisplay::getInstance().updateWindow(win_name, _image);
}

void ImageProcesser::clear()
{
    std::lock_guard<std::mutex> lock(image_mutex);
    
    _image.reset();
    if(has_win) {
        ImageDisplay::getInstance().closeWindow(win_name);
    }
    has_win = false;
}

// TYFrameParser 
TYFrameParser::TYFrameParser(uint32_t max_queue_size)
    : max_queue_size(max_queue_size)
{
    stream[TY_COMPONENT_DEPTH_CAM] = std::make_shared<ImageProcesser>("depth");
    stream[TY_COMPONENT_IR_CAM_LEFT] = std::make_shared<ImageProcesser>("Left-IR");
    stream[TY_COMPONENT_IR_CAM_RIGHT] = std::make_shared<ImageProcesser>("Right-IR");
    stream[TY_COMPONENT_RGB_CAM] = std::make_shared<ImageProcesser>("color");
    
    processThread = std::thread(&TYFrameParser::processThreadFunc, this);
}

TYFrameParser::~TYFrameParser()
{
    stop();
}

void TYFrameParser::stop()
{
    isRunning = false;
    cv.notify_all();
    if (processThread.joinable()) {
        processThread.join();
    }
    
    for (auto& s : stream) {
        TY_COMPONENT_ID id = s.first;
        std::shared_ptr<ImageProcesser>& processor = s.second;
        if (processor) {
            processor->clear();
        }
    }
}

int TYFrameParser::setImageProcesser(TY_COMPONENT_ID id, std::shared_ptr<ImageProcesser> proc)
{
    stream[id] = std::move(proc);
    return 0;
}

int TYFrameParser::doProcess(const std::shared_ptr<TYFrame>& frame)
{
    if (!frame) return -1;
    
    std::vector<TY_COMPONENT_ID> components = {
        TY_COMPONENT_IR_CAM_LEFT,
        TY_COMPONENT_IR_CAM_RIGHT,
        TY_COMPONENT_RGB_CAM,
        TY_COMPONENT_DEPTH_CAM
    };
    
    for (auto compID : components) {
        auto it = stream.find(compID);
        if (it != stream.end() && it->second) {
            std::shared_ptr<TYImage> image;
            
            switch (compID) {
                case TY_COMPONENT_DEPTH_CAM: image = frame->depthImage(); break;
                case TY_COMPONENT_RGB_CAM: image = frame->colorImage(); break;
                case TY_COMPONENT_IR_CAM_LEFT: image = frame->leftIRImage(); break;
                case TY_COMPONENT_IR_CAM_RIGHT: image = frame->rightIRImage(); break;
                default: continue;
            }
            
            if (image) {
                it->second->parse(image);
            }
        }
    }
    
    return 0;
}

void TYFrameParser::update(const std::shared_ptr<TYFrame>& frame)
{
    if (!frame) return;
    
    std::lock_guard<std::mutex> lock(queue_mutex);
    imageQueueSizeCheck();
    images.push(frame);
    cv.notify_one();
}

void TYFrameParser::processThreadFunc()
{
    while (isRunning) {
        std::shared_ptr<TYFrame> frame;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait_for(lock, std::chrono::milliseconds(33), [this]() { 
                return !isRunning || !images.empty(); 
            });
            
            if (!isRunning && images.empty()) break;
            
            if (!images.empty()) {
                frame = images.front();
                images.pop();
            }
        }
        
        if (frame) {
            doProcess(frame);
            
            for (auto& s : stream) {
                TY_COMPONENT_ID compID = s.first;
                std::shared_ptr<ImageProcesser>& processor = s.second;
                if (processor) {
                    int ret = processor->flush();
                    if (ret > 0 && func_keyboard_event) {
                        func_keyboard_event(ret, user_data);
                    }
                }
            }
        }
    }
}

void TYFrameParser::imageQueueSizeCheck()
{
    while (images.size() > max_queue_size * 2) {
        images.pop();
    }
}

} // namespace percipio_layer
