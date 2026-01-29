/************************************************
 * @File Name:       TyDummyWindow.cpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2025-12-16 15:13:37
 * @Modified Time:   2025-12-22 14:14:28
 ***********************************************/
#include "TYImageShow.h"
int TYCreateDisplayWindow(const char* windowName, int width, int height) {
  return 0;
}
int TYDisplayImage(const char* windowName, int width, int height, 
                           TYPixFmt pixelFormat, const void* imageData, 
                           const float depth_scale_unit) {
  return 0;
}

int TYWaitKeyEvents(void) {
  return -1;
}

int TYDestroyWindow(const char* windowName) {
  return 0;
}

int TYDestroyAllWindows(void) {
  return 0;
}
