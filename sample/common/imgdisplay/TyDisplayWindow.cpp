#define NOMINMAX
#include "TYImageShow.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <queue>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#define cimg_display 2          // use the Microsoft GDI32 framework.
#else
#define cimg_display 1          // use the X-Window framework (X11).
#endif

//#define cimg_use_jpeg         //Optional: if JPEG support is needed
//#define cimg_use_png          //Optional: if PNG support is needed
#include "CImg.h"

using namespace cimg_library;

#define MAX_DEPTH 0x10000


struct float3
{
    float x, y, z;

    float length() const { return sqrt(x * x + y * y + z * z); }

    float3 normalize() const
    {
        return (length() > 0) ? float3{ x / length(), y / length(), z / length() } : *this;
    }
};

inline float3 cross(const float3& a, const float3& b)
{
    return { a.y * b.z - b.y * a.z, a.x * b.z - b.x * a.z, a.x * b.y - a.y * b.x };
}

inline float3 operator*(const float3& a, float t)
{
    return { a.x * t, a.y * t, a.z * t };
}

inline float3 operator/(const float3& a, float t)
{
    return { a.x / t, a.y / t, a.z / t };
}

inline float3 operator+(const float3& a, const float3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline float3 operator-(const float3& a, const float3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline float3 lerp(const float3& a, const float3& b, float t)
{
    return b * t + a * (1 - t);
}

class ty_color_map
{
public:
    explicit ty_color_map(std::map<float, float3> map, int steps = 4000) : _map(map)
    {
        initialize(steps);
    }

    explicit ty_color_map(const std::vector<float3>& values, int steps = 4000)
    {
        for (size_t i = 0; i < values.size(); i++)
        {
            _map[(float)i / (values.size() - 1)] = values[i];
        }
        initialize(steps);
    }

    ty_color_map() {}

    float3 get(float value) const
    {
        if (_max == _min) return *_data;
        auto t = (value - _min) / (_max - _min);
        t = std::min(std::max(t, 0.f), 1.f);
        return _data[(int)(t * (_size - 1))];
    }

    float min_key() const { return _min; }
    float max_key() const { return _max; }

private:
    float3 calc(float value) const
    {
        if (_map.size() == 0) return { value, value, value };
        // if we have exactly this value in the map, just return it
        if (_map.find(value) != _map.end()) return _map.at(value);
        // if we are beyond the limits, return the first/last element
        if (value < _map.begin()->first)   return _map.begin()->second;
        if (value > _map.rbegin()->first)  return _map.rbegin()->second;

        auto lower = _map.lower_bound(value) == _map.begin() ? _map.begin() : --(_map.lower_bound(value));
        auto upper = _map.upper_bound(value);

        auto t = (value - lower->first) / (upper->first - lower->first);
        auto c1 = lower->second;
        auto c2 = upper->second;
        return lerp(c1, c2, t);
    }

    void initialize(int steps)
    {
        if (_map.size() == 0) return;

        _min = _map.begin()->first;
        _max = _map.rbegin()->first;

        _cache.resize(steps + 1);
        for (int i = 0; i <= steps; i++)
        {
            auto t = (float)i / steps;
            auto x = _min + t * (_max - _min);
            _cache[i] = calc(x);
        }

        // Save size and data to avoid STL checks penalties in DEBUG
        _size = _cache.size();
        _data = _cache.data();
    }

    std::map<float, float3> _map;
    std::vector<float3> _cache;
    float _min, _max;
    size_t _size; float3* _data;
};

static const ty_color_map ty_jet({
    {255,   0,   0},    // Red
    {255, 100,   0},    // Orange-red  
    {255, 255,   0},    // Yellow
    {0,   255, 255},    // Cyan
    {0,     0, 255}     // Blue
});

static void update_histogram(int* hist, const uint16_t* depth_data, int pixel_count)
{
    memset(hist, 0, MAX_DEPTH * sizeof(int));
    
    for (int i = 0; i < pixel_count; ++i)
    {
        uint16_t depth = depth_data[i];
        if (depth > 0 && depth < 0xFFFF)
        {
            ++hist[depth];
        }
    }
    
    int sum = 0;
    for (int i = 1; i < MAX_DEPTH; ++i)
    {
        sum += hist[i];
        hist[i] = sum;
    }
}

static void depthToRGB(const uint16_t* depth_frame, int width, int height, 
                      const int* histogram, uint8_t* rgb_frame)
{
    const int pixel_count = width * height;
    const int total_pixels = histogram[MAX_DEPTH - 1];
    
    if (total_pixels == 0)
    {
        memset(rgb_frame, 0, pixel_count * 3);
        return;
    }
    
    const float inv_total = 1.0f / total_pixels;
    
    for (int i = 0; i < pixel_count; ++i)
    {
        const uint16_t depth = depth_frame[i];
        uint8_t* rgb = rgb_frame + i * 3;
        
        if (depth > 0 && depth < 0xFFFF)
        {
            float normalized = histogram[depth] * inv_total;
            const float3 color = ty_jet.get(normalized);
            
            rgb[0] = (uint8_t)color.x; // B
            rgb[1] = (uint8_t)color.y; // G  
            rgb[2] = (uint8_t)color.z; // R
        }
        else
        {
            rgb[0] = rgb[1] = rgb[2] = 0;
        }
    }
}

static void depthToRGBDirect(const uint16_t* depth_frame, int width, int height, uint8_t* rgb_frame)
{
    static std::vector<int> histogram(MAX_DEPTH);
    
    const int pixel_count = width * height;
    update_histogram(&histogram[0], depth_frame, pixel_count);
    depthToRGB(depth_frame, width, height, &histogram[0], rgb_frame);
}

//Global variables: store CImg display windows
static std::map<std::string, CImgDisplay*> g_CImgWindows;
static std::queue<unsigned char> g_KeyQueue;

//Create display window
TY_SHOW_API TYCreateDisplayWindow(const char* windowName, int width, int height)
{
    if (width <= 0) width = 800;
    if (height <= 0) height = 600;
    
    // Check if window already exists
    auto it = g_CImgWindows.find(windowName);
    if (it != g_CImgWindows.end())
    {
        //Window exists, adjust size
        CImgDisplay* display = it->second;
        if (display->width() != width || display->height() != height)
        {
            display->resize(width, height);
        }
        return 0;
    }
    
    //Create new window
    try
    {
        CImgDisplay* display = new CImgDisplay(width, height, windowName, 0);
        g_CImgWindows[windowName] = display;
        
        return 0;
    }
    catch (const CImgException& e)
    {
        std::cerr << "Error creating CImg display window: " << e.what() << std::endl;
        return -1;
    }
}

//Helper function to display image with mouse position information
static void displayWithMouseInfo(CImgDisplay* display, CImg<unsigned char>& img, const void* imageData, TYPixFmt pixelFormat, float f_scale_unit = 1.f)
{
    //Get mouse position relative to the image
    int mouse_x = display->mouse_x();
    int mouse_y = display->mouse_y();
    
    //Create a copy of the image for displaying (so we don't modify the original)
    CImg<unsigned char> displayImg = img;
    
    //Check if mouse is within image bounds
    if (mouse_x >= 0 && mouse_x < img.width() && mouse_y >= 0 && mouse_y < img.height())
    {
        //Create information string based on pixel format
        std::string infoStr;
        
        switch (pixelFormat)
        {
            case TYPixelFormatRGB8:
            case TYPixelFormatBGR8:
            {
                //Get RGB values at mouse position
                unsigned char r = img(mouse_x, mouse_y, 0, 0);
                unsigned char g = img(mouse_x, mouse_y, 0, 1);
                unsigned char b = img(mouse_x, mouse_y, 0, 2);
                
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "Pos: (%d, %d)  RGB: (%d, %d, %d)", 
                        mouse_x, mouse_y, r, g, b);
                infoStr = buffer;
                break;
            }
            
            case TYPixelFormatMono8:
            {
                // Get grayscale value at mouse position
                unsigned char gray = img(mouse_x, mouse_y);
                
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "Pos: (%d, %d)  Gray8: %d", 
                        mouse_x, mouse_y, gray);
                infoStr = buffer;
                break;
            }
            case TYPixelFormatMono16:
            {
                const uint16_t* gray_ptr = (const uint16_t*)imageData;
                uint16_t gray = gray_ptr[img.width() * mouse_y + mouse_x];

                char buffer[128];
                snprintf(buffer, sizeof(buffer), "Pos: (%d, %d) Gray16: %d",
                        mouse_x, mouse_y, gray);
                infoStr = buffer;
                break;
            }
            case TYPixelFormatCoord3D_C16:
            {
                // For depth images, show position and maybe depth value
                const uint16_t* depth_ptr = (const uint16_t*)imageData;
                float depth = depth_ptr[img.width() * mouse_y + mouse_x] * f_scale_unit;

                char buffer[128];
                snprintf(buffer, sizeof(buffer), "Pos: (%d, %d) Depth: %.2f",
                        mouse_x, mouse_y, depth);
                infoStr = buffer;
                break;
            }
            
            default:
                infoStr = "Pos: (" + std::to_string(mouse_x) + ", " + 
                          std::to_string(mouse_y) + ")";
                break;
        }
        
        // Draw a white background rectangle for text
        const unsigned char white[] = { 255, 255, 255 };
        const unsigned char black[] = { 0, 0, 0 };
        
        // Determine text position (top-left corner)
        int textX = 10;
        int textY = 10;
        
        // Draw background rectangle
        int textWidth = infoStr.length() * 8;  // Approximate width
        int textHeight = 20;  // Approximate height
        
        // Draw rectangle with some transparency
        for (int y = textY - 5; y < textY + textHeight; ++y)
        {
            for (int x = textX - 5; x < textX + textWidth + 5; ++x)
            {
                if (x >= 0 && x < displayImg.width() && y >= 0 && y < displayImg.height())
                {
                    // Semi-transparent black background
                    if (displayImg.spectrum() == 1)  // Grayscale
                    {
                        displayImg(x, y) = 0;  // Black
                    }
                    else if (displayImg.spectrum() == 3)  // RGB
                    {
                        displayImg(x, y, 0, 0) = 0;  // R
                        displayImg(x, y, 0, 1) = 0;  // G
                        displayImg(x, y, 0, 2) = 0;  // B
                    }
                }
            }
        }
        
        // Draw the text
        if (displayImg.spectrum() == 1)  // Grayscale
        {
            displayImg.draw_text(textX, textY, infoStr.c_str(), white, black, 1, 15);
        }
        else  // RGB
        {
            displayImg.draw_text(textX, textY, infoStr.c_str(), white, black, 1, 15);
        }
        
        // Optionally draw a crosshair at mouse position
        const unsigned char red[] = { 255, 0, 0 };
        const unsigned char green[] = { 0, 255, 0 };
        
        // Draw horizontal line
        for (int x = std::max(0, mouse_x - 10); x < std::min(img.width(), mouse_x + 11); ++x)
        {
            if (x >= 0 && x < img.width() && mouse_y >= 0 && mouse_y < img.height())
            {
                if (img.spectrum() == 1)
                {
                    displayImg(x, mouse_y) = 255;  // White for grayscale
                }
                else
                {
                    displayImg(x, mouse_y, 0, 0) = 255;  // Red
                    displayImg(x, mouse_y, 0, 1) = 0;
                    displayImg(x, mouse_y, 0, 2) = 0;
                }
            }
        }
        
        // Draw vertical line
        for (int y = std::max(0, mouse_y - 10); y < std::min(img.height(), mouse_y + 11); ++y)
        {
            if (mouse_x >= 0 && mouse_x < img.width() && y >= 0 && y < img.height())
            {
                if (img.spectrum() == 1)
                {
                    displayImg(mouse_x, y) = 255;  // White for grayscale
                }
                else
                {
                    displayImg(mouse_x, y, 0, 0) = 255;  // Red
                    displayImg(mouse_x, y, 0, 1) = 0;
                    displayImg(mouse_x, y, 0, 2) = 0;
                }
            }
        }
    }
    else
    {
        // Mouse is outside image, just show basic info
        std::string infoStr = "Mouse outside image";
        
        // Draw background and text
        const unsigned char white[] = { 255, 255, 255 };
        const unsigned char black[] = { 0, 0, 0 };
        
        int textX = 10;
        int textY = 10;
        
        // Draw background rectangle
        int textWidth = infoStr.length() * 8;
        int textHeight = 20;
        
        for (int y = textY - 5; y < textY + textHeight; ++y)
        {
            for (int x = textX - 5; x < textX + textWidth + 5; ++x)
            {
                if (x >= 0 && x < displayImg.width() && y >= 0 && y < displayImg.height())
                {
                    if (displayImg.spectrum() == 1)
                    {
                        displayImg(x, y) = 0;
                    }
                    else if (displayImg.spectrum() == 3)
                    {
                        displayImg(x, y, 0, 0) = 0;
                        displayImg(x, y, 0, 1) = 0;
                        displayImg(x, y, 0, 2) = 0;
                    }
                }
            }
        }
        
        // Draw the text
        if (displayImg.spectrum() == 1)
        {
            displayImg.draw_text(textX, textY, infoStr.c_str(), white, black, 1, 15);
        }
        else
        {
            displayImg.draw_text(textX, textY, infoStr.c_str(), white, black, 1, 15);
        }
    }
    
    // Display the modified image
    display->display(displayImg);
}

//Display image
TY_SHOW_API TYDisplayImage(const char* windowName, int width, int height, 
                            TYPixFmt pixelFormat, const void* imageData, const float scale)
{
    // Check if window exists, create if not
    auto it = g_CImgWindows.find(windowName);
    if (it == g_CImgWindows.end())
    {
        TYCreateDisplayWindow(windowName, width, height);
        it = g_CImgWindows.find(windowName);
        if (it == g_CImgWindows.end())
        {
            std::cerr << "Error: Failed to create window '" << windowName << "'" << std::endl;
            return -1;
        }
    }
    
    CImgDisplay* display = it->second;
    
    try
    {
        // Process different image types based on pixel format
        switch (pixelFormat)
        {
            case TYPixelFormatRGB8:
            {
                // RGB8 format: 3 bytes per pixel (R,G,B)
                CImg<unsigned char> img(width, height, 1, 3);
                const unsigned char* src = (const unsigned char*)imageData;
                
                // Copy data, note CImg is row-major
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        int src_idx = (y * width + x) * 3;
                        int dst_idx = y * width + x;
                        
                        img(x, y, 0, 0) = src[src_idx + 0];   // R
                        img(x, y, 0, 1) = src[src_idx + 1];   // G
                        img(x, y, 0, 2) = src[src_idx + 2];   // B
                    }
                }
                
                // Display image with mouse position information
                displayWithMouseInfo(display, img, imageData, pixelFormat);
                break;
            }
            
            case TYPixelFormatBGR8:
            {
                // BGR8 format: need to convert to RGB
                CImg<unsigned char> img(width, height, 1, 3);
                const unsigned char* src = (const unsigned char*)imageData;
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        int src_idx = (y * width + x) * 3;
                        int dst_idx = y * width + x;
                        
                        img(x, y, 0, 0) = src[src_idx + 2];   // B -> R
                        img(x, y, 0, 1) = src[src_idx + 1];   // G -> G
                        img(x, y, 0, 2) = src[src_idx + 0];   // R -> B
                    }
                }
                
                // Display image with mouse position information
                displayWithMouseInfo(display, img, imageData, pixelFormat);
                break;
            }
            
            case TYPixelFormatMono8:
            {
                // Single channel 8-bit grayscale image
                CImg<unsigned char> img(width, height, 1, 1);
                const unsigned char* src = (const unsigned char*)imageData;
                
                // Direct copy data
                std::memcpy(img.data(), src, width * height);
                
                // Display image with mouse position information
                displayWithMouseInfo(display, img, imageData, pixelFormat);
                break;
            }
            
            case TYPixelFormatMono16:
            {
                // Single channel 16-bit grayscale image: need to scale to 8-bit
                CImg<unsigned char> img(width, height, 1, 1);
                const uint16_t* src = (const uint16_t*)imageData;
                
                // Scale and convert to 8-bit
                for (int i = 0; i < width * height; ++i)
                {
                    img[i] = (unsigned char)((src[i] >> 8));
                }
                
                // Display image with mouse position information
                displayWithMouseInfo(display, img, imageData, pixelFormat);
                break;
            }
            
            case TYPixelFormatCoord3D_C16:
            {
                // Depth map: convert to colored RGB display
                CImg<unsigned char> img(width, height, 1, 3);
                uint16_t* depth_data = (uint16_t*)imageData;

                for(size_t index = 0; index < width * height; index++) {
                    if(depth_data[index] == 0xffff) {
                        depth_data[index] = 0;
                    }
                }
                
                // Use existing depth to RGB conversion function
                std::vector<uint8_t> rgb_data(width * height * 3);
                depthToRGBDirect(depth_data, width, height, rgb_data.data());
                
                // Copy to CImg image
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        int idx = (y * width + x) * 3;
                        img(x, y, 0, 0) = rgb_data[idx + 0];   // R
                        img(x, y, 0, 1) = rgb_data[idx + 1];   // G
                        img(x, y, 0, 2) = rgb_data[idx + 2];   // B
                    }
                }
                
                // Display image with mouse position information
                displayWithMouseInfo(display, img, imageData, pixelFormat, scale);
                break;
            }
            
            default:
                std::cerr << "Error: Unsupported pixel format: 0x" << std::hex 
                          << pixelFormat << std::dec << " for window '" 
                          << windowName << "'" << std::endl;
                return -2;
        }
        return 0;
    }
    catch (const CImgException& e)
    {
        std::cerr << "Error displaying image in window '" << windowName 
                  << "': " << e.what() << std::endl;
        return -1;
    }
}

#ifdef _WIN32
static LRESULT CALLBACK KeyboardMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            BYTE keyboardState[256];
            GetKeyboardState(keyboardState);

            WCHAR buffer[2];
            int result = ToUnicode(p->vkCode, p->scanCode, keyboardState, buffer, 2, 0);

            if (result > 0)
            {
                char asciiChar = (char)buffer[0];
                if (asciiChar > 0)
                {
                    g_KeyQueue.push((unsigned char)asciiChar);
                }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
#endif

int TYWaitKeyEvents(void)
{
    for (auto it = g_CImgWindows.begin(); it != g_CImgWindows.end();)
    {
        CImgDisplay* display = it->second;
        
        if (display->is_closed())
        {
            delete display;
            it = g_CImgWindows.erase(it);
            continue; 
        }
        
        if (display->is_key())
        {
            unsigned char key = display->key();
            if (key >= 'A' && key <= 'Z')
            {
                key = key + 32;
            }
            g_KeyQueue.push(key);
        }
        
        display->wait(0);
        
        ++it;
    }
    
#ifdef _WIN32
    static HHOOK keyboardHook = NULL;
    if (!g_CImgWindows.empty() && keyboardHook == NULL)
    {
        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardMsgProc, NULL, 0);
    }
    else if (g_CImgWindows.empty() && keyboardHook != NULL)
    {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
    
    //handle Windows message
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
    
    // return key value
    if (!g_KeyQueue.empty())
    {
        unsigned char key = g_KeyQueue.front();
        g_KeyQueue.pop();
        return (int)key;
    }
    
    return -1;  // No key pressed
}

TY_SHOW_API TYDestroyWindow(const char* windowName)
{
    auto it = g_CImgWindows.find(windowName);
    if (it != g_CImgWindows.end())
    {
        delete it->second;
        g_CImgWindows.erase(it);
        return 0;
    }
    else
    {
        return 0;
    }
}

TY_SHOW_API TYDestroyAllWindows(void)
{
    for (auto& pair : g_CImgWindows)
    {
        delete pair.second;
    }
    g_CImgWindows.clear();
    
    while (!g_KeyQueue.empty())
    {
        g_KeyQueue.pop();
    }
    
#ifdef _WIN32
    // clear keyboard hooks
    static HHOOK keyboardHook = NULL;
    if (keyboardHook != NULL)
    {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
#endif
    
    return 0;
}
