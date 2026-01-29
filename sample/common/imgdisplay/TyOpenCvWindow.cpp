/************************************************
 * @File Name:       ../sample/common/imgdisplay/TyOpenCvWindow.cpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2025-12-15 17:49:42
 * @Modified Time:   2025-12-16 15:07:29
 ***********************************************/
#include "TYImageShow.h"
#include "MatViewer.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <mutex>
#include <memory>
#include <map>

//Use Static to avoid multi-definition issue when compile
int TYCreateDisplayWindow(const char* windowName, int width, int height) {
  cv::namedWindow(windowName);
  return 0;
}

static inline int parseImage(cv::Mat &image, int width, int height, 
                           TYPixFmt pixelFormat, const void* imageData) {
  switch (pixelFormat) {
    case TYPixelFormatRGB8:
    {
      //Cast const to normal, as we do not change original datas
      cv::Mat org(height, width, CV_8UC3, const_cast<void*>(imageData));
      cv::cvtColor(org, image, cv::COLOR_RGB2BGR);
      break;
    }
    case TYPixelFormatBGR8:
    {
      cv::Mat org(height, width, CV_8UC3, const_cast<void*>(imageData));
      image = org;
      break;
    }
    case TYPixelFormatMono8:
    {
      cv::Mat org(height, width, CV_8U, const_cast<void*>(imageData));
      image = org;
      break;
    }
    case TYPixelFormatMono16:
    case TYPixelFormatCoord3D_C16:
    {
      cv::Mat org(height, width, CV_16U, const_cast<void*>(imageData));
      image = org;
      break;
    }
    default:
        std::cerr << "Error: Unsupported pixel format: 0x" << std::hex 
                  << pixelFormat << std::dec << std::endl;
        return -2;

  }
  return 0;
}

int TYDisplayImage(const char* windowName, int width, int height, 
                           TYPixFmt pixelFormat, const void* imageData, 
                           const float depth_scale_unit) {
  cv::Mat mat;
  int ret = parseImage(mat, width, height, pixelFormat, imageData);
  if (ret < 0) {
    return ret;
  }
  
  static std::mutex imshowMutex;
  static std::map<std::string, std::shared_ptr<DepthViewer>> depthViewer;
  std::lock_guard<std::mutex> lock(imshowMutex);
  if (pixelFormat == TYPixelFormatCoord3D_C16) {
    auto it = depthViewer.find(windowName);
    if(it == depthViewer.end()) {
      auto viewer = std::make_shared<DepthViewer>(windowName);
      it = depthViewer.emplace(windowName, viewer).first;
    }
    it->second->depth_scale_unit = depth_scale_unit;
    it->second->show(mat);
  } else {
    cv::imshow(windowName, mat);
  }
  return 0;
}

int TYWaitKeyEvents(void) {
  return cv::waitKey(1);
}

int TYDestroyWindow(const char* windowName) {
  cv::destroyWindow(windowName);
  return 0;
}

int TYDestroyAllWindows(void) {
  cv::destroyAllWindows();
  return 0;
}

