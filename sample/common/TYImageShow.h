#ifndef TY_IMAGE_SHOW_H_
#define TY_IMAGE_SHOW_H_

#include "TYDefs.h"
#define TY_SHOW_API int

/// @brief Create a display window for image visualization.
/// 
/// This function creates a window with the specified name and dimensions.
/// If a window with the same name already exists, its size will be adjusted
/// to match the new dimensions.
/// 
/// @param  [in] windowName      Window name/title for identification. Must be unique.
/// @param  [in] width           Window width in pixels. If <= 0, defaults to 800.
/// @param  [in] height          Window height in pixels. If <= 0, defaults to 600.
/// 
/// @return Returns 0 on success, or -1 if window creation fails.
TY_SHOW_API TYCreateDisplayWindow(const char* windowName, int width, int height);

/// @brief Display an image in the specified window.
/// 
/// This function displays various image formats including RGB, grayscale, and depth images.
/// For depth images (TYPixelFormatCoord3D_C16), the depth_scale_unit parameter allows
/// conversion of raw depth values to real-world distances.
/// 
/// The window will show mouse position and pixel value information when the mouse is
/// over the image. For depth images, the actual distance in millimeters is calculated
/// as: distance = depth_value * depth_scale_unit.
/// 
/// @param  [in] windowName        Name of the window where the image will be displayed.
///                                 If the window doesn't exist, it will be created.
/// @param  [in] width             Width of the image in pixels.
/// @param  [in] height            Height of the image in pixels.
/// @param  [in] pixelFormat       Pixel format of the input image data.
///                                 Supported formats:
///                                 - TYPixelFormatRGB8: 8-bit RGB format (R,G,B)
///                                 - TYPixelFormatBGR8: 8-bit BGR format (B,G,R)
///                                 - TYPixelFormatMono8: 8-bit grayscale
///                                 - TYPixelFormatMono16: 16-bit grayscale
///                                 - TYPixelFormatCoord3D_C16: 16-bit depth map (colorized with jet colormap)
/// @param  [in] imageData         Pointer to the image data buffer.
///                                 The buffer size should be at least:
///                                 - width * height * 3 bytes for RGB/BGR formats
///                                 - width * height bytes for 8-bit grayscale
///                                 - width * height * 2 bytes for 16-bit grayscale/depth
/// @param  [in] depth_scale_unit  Depth scaling factor (millimeters per depth unit).
///                                 This parameter is only relevant for depth images
///                                 (TYPixelFormatCoord3D_C16). It converts the raw 16-bit
///                                 depth values to real-world distances.
///                                 
///                                 Example:
///                                 - depth_scale_unit = 0.25f: Each depth unit = 0.25mm
///                                 - depth_scale_unit = 0.1f: Each depth unit = 0.1mm
///                                 - depth_scale_unit = 1.0f: Each depth unit = 1.0mm (default)
///                                 
///                                 The actual distance (mm) = depth_value * depth_scale_unit
///                                 
///                                 Note: A depth value of 0xFFFF (65535) typically represents
///                                 an invalid or out-of-range measurement and is displayed as black.
///                                 
///                                 Default value: 1.0f
/// 
/// @return Returns 0 on success, -1 if window creation/display fails, or -2 for unsupported pixel format.
TY_SHOW_API TYDisplayImage(const char* windowName, int width, int height, 
                           TYPixFmt pixelFormat, const void* imageData, 
                           const float depth_scale_unit = 1.f);

/// @brief Wait for and process keyboard and window events.
/// 
/// This function should be called periodically to process window events,
/// including keyboard input and window close events. It processes events
/// for all open windows and returns any keyboard input.
/// 
/// On Windows, this function also sets up a global keyboard hook to capture
/// keyboard events even when windows don't have focus, ensuring consistent
/// key event handling across all windows.
/// 
/// Note: Key events are queued and this function returns the oldest key
/// from the queue. ASCII letters are converted to lowercase.
/// 
/// @return ASCII code of the pressed key (lowercase for letters), 
///         or -1 if no key was pressed.
TY_SHOW_API TYWaitKeyEvents(void);

/// @brief Destroy a specific display window.
/// 
/// This function closes and destroys the window with the specified name.
/// If the window doesn't exist, the function returns success (0).
/// 
/// @param  [in] windowName      Name of the window to destroy.
/// 
/// @return Always returns 0 (success).
TY_SHOW_API TYDestroyWindow(const char* windowName);

/// @brief Destroy all display windows and cleanup resources.
/// 
/// This function closes and destroys all open display windows,
/// clears the keyboard event queue, and releases all associated resources.
/// It should be called at program shutdown to ensure proper cleanup.
/// 
/// @return Always returns 0 (success).
TY_SHOW_API TYDestroyAllWindows(void);

#endif
