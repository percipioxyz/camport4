#include "common.hpp"

// The distance(mm) range defined by the macro is related to the camera model
#define MIN_DEPTH			(400)
#define MAX_DEPTH			(4000)

// The rect size used for rgbd registration in color image.
// Located in the center of the color image.
#define DEFAULT_RECT_WIDTH 80
#define DEFAULT_RECT_HEIGHT 60

struct CallbackData {
  int             index;
  TY_DEV_HANDLE   hDevice;

  float           scale_unit;

  TY_CAMERA_CALIB_INFO depth_calib;
  TY_CAMERA_CALIB_INFO color_calib;
};

struct Rect {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

struct Point2D {
  int32_t x;
  int32_t y;
};

static void printRectDebug(const std::vector<Point2D>& dst_pos) {
    std::cout << "dst_pos = {";
    for (size_t i = 0; i < dst_pos.size(); ++i) {
        std::cout << "{" << dst_pos[i].x << ", " << dst_pos[i].y << "}";
        if (i < dst_pos.size() - 1) std::cout << ", ";
    }
    std::cout << "}" << std::endl;
}

static void printRectDebug(const Rect& rect) {
    Point2D top_left = {rect.x, rect.y};
    Point2D top_right = {rect.x + rect.w, rect.y};
    Point2D bottom_right = {rect.x + rect.w, rect.y + rect.h};
    Point2D bottom_left = {rect.x, rect.y + rect.h};
    
    std::vector<Point2D> points = {top_left, top_right, bottom_right, bottom_left};
    std::cout << "src_pos = {";
    for (size_t i = 0; i < points.size(); ++i) {
        std::cout << "{" << points[i].x << ", " << points[i].y << "}";
        if (i < points.size() - 1) std::cout << ", ";
    }
    std::cout << "}" << std::endl;
}

static void doRectRegister(const TY_CAMERA_CALIB_INFO& depth_calib
                      , const TY_CAMERA_CALIB_INFO& color_calib
                      , const uint16_t* depth
                      , const int32_t depthWidth
                      , const int32_t depthHeight
                      , float f_scale_unit
                      , const int32_t colorWidth
                      , const int32_t colorHeight
                      , const Rect& src
                      , std::vector<Point2D>& point_array)
{
  std::vector<TY_PIXEL_COLOR_DESC> src_rgb_data(4);
  std::vector<TY_PIXEL_COLOR_DESC> dst_rgb_data(4);

  src_rgb_data[0].x = src.x;              src_rgb_data[0].y = src.y;
  src_rgb_data[1].x = src.x + src.w;      src_rgb_data[1].y = src.y;
  src_rgb_data[2].x = src.x + src.w;      src_rgb_data[2].y = src.y + src.h;
  src_rgb_data[3].x = src.x;              src_rgb_data[3].y = src.y + src.h;

  ASSERT_OK(
    TYMapRGBPixelsToDepthCoordinate(
      &depth_calib,
      depthWidth, depthHeight, depth,
      &color_calib,
      colorWidth, colorHeight, &src_rgb_data[0], src_rgb_data.size(),
      MIN_DEPTH, MAX_DEPTH,
      &dst_rgb_data[0], f_scale_unit
    )
  );
  
  int size = dst_rgb_data.size();
  point_array.resize(size);
  for (int i = 0; i < size; i++) {
    point_array[i].x = dst_rgb_data[i].x * f_scale_unit;
    point_array[i].y = dst_rgb_data[i].y * f_scale_unit;
  }
}

void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
  CallbackData* pData = (CallbackData*)userdata;
  LOGD("=== Get frame %d", ++pData->index);
#if 1
  TY_IMAGE_DATA* depthImage = nullptr;
  TY_IMAGE_DATA* colorImage = nullptr;
  for (int i = 0; i < frame->validCount; i++) {
      if (frame->image[i].status != TY_STATUS_OK) continue;
    
      if (frame->image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
          depthImage = &frame->image[i];
      }
      else if (frame->image[i].componentID == TY_COMPONENT_RGB_CAM) {
          colorImage = &frame->image[i];
      }
  }

  if (depthImage != nullptr && colorImage != nullptr) {
      Rect roi_src = {
          (colorImage->width - DEFAULT_RECT_WIDTH) / 2, 
          (colorImage->height - DEFAULT_RECT_HEIGHT) / 2, 
          DEFAULT_RECT_WIDTH, 
          DEFAULT_RECT_HEIGHT
      };

      printRectDebug(roi_src);

      std::vector<Point2D> dst_pos(4);
      doRectRegister(pData->depth_calib, pData->color_calib, 
                (const uint16_t*)depthImage->buffer, depthImage->width, depthImage->height, pData->scale_unit,
                colorImage->width, colorImage->height, 
                roi_src, dst_pos);
      
      printRectDebug(dst_pos);
  }
#else
  cv::Mat depth, color;
  parseFrame(*frame, &depth, 0, 0, &color);
  if (!depth.empty() && !color.empty()) {
    // do undistortion
    int32_t image_size;   
    TYPixFmt color_fmt;
    if(color.type() == CV_16U) {
      image_size = color.size().area() * 2;
      color_fmt = TYPixelFormatMono16;
    }
    else {
      image_size = color.size().area() * 3;
      color_fmt = TYPixelFormatRGB8;
    }

    cv::Mat  undistort_color;
    if(color_fmt == TYPixelFormatMono16)
      undistort_color = cv::Mat(color.size(), CV_16U);
    else
      undistort_color = cv::Mat(color.size(), CV_8UC3);

    TY_IMAGE_DATA src;
    src.width = color.cols;
    src.height = color.rows;
    src.size = image_size;
    src.pixelFormat = color_fmt;
    src.buffer = color.data;

    TY_IMAGE_DATA dst;
    dst.width = color.cols;
    dst.height = color.rows;
    dst.size = image_size;
    dst.buffer = undistort_color.data;
    dst.pixelFormat = color_fmt;
    ASSERT_OK(TYUndistortImage(&pData->color_calib, &src, NULL, &dst));

    //use default roi area
    cv::Rect roi_src = cv::Rect(
      (undistort_color.cols - DEFAULT_RECT_WIDTH) / 2, 
      (undistort_color.rows - DEFAULT_RECT_HEIGHT) / 2, 
      DEFAULT_RECT_WIDTH, DEFAULT_RECT_HEIGHT);

    std::vector<cv::Point> dst_pos(4);
    doRectRegister(pData->depth_calib, pData->color_calib, depth, pData->scale_unit, undistort_color, roi_src, dst_pos);
	
    cv::rectangle(undistort_color, roi_src, cv::Scalar(100, 100, 100), 2);
    cv::imshow("undistort color", undistort_color);

    cv::Mat depth_display = pData->render->Compute(depth);

    int m_valid_pixels_cnt = 0;
    for (int i = 0; i < dst_pos.size(); i++) {
      if ((dst_pos[i].x >= 0) && (dst_pos[i].y >= 0))
        m_valid_pixels_cnt++;
    }
    if (m_valid_pixels_cnt == dst_pos.size()) {
      cv::line(depth_display, dst_pos[0], dst_pos[1], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[1], dst_pos[2], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[2], dst_pos[3], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[3], dst_pos[0], cv::Scalar(200, 0, 0), 1);
    }
    else {
      LOGW("Some pixels map failed.\n");
    }
    cv::imshow("Depth", depth_display);
  }
#endif
  LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
  ASSERT_OK(TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize));
}

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
  if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
    LOGD("=== Event Callback: Device Offline!");
    // Note: 
      //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
  }
  else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
    LOGD("=== Event Callback: License Error!");
  }
}

int main(int argc, char* argv[])
{
  std::string ID, IP;
  TY_INTERFACE_HANDLE hIface = NULL;
  TY_DEV_HANDLE hDevice = NULL;

  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "-id") == 0){
      ID = argv[++i];
    } else if(strcmp(argv[i], "-ip") == 0) {
      IP = argv[++i];
    }else if(strcmp(argv[i], "-h") == 0){
      LOGI("Usage: SimpleView_PixelsRegistration [-h] [-id <ID>]");
      return 0;
    }
  }

  LOGD("=== Init lib");
  ASSERT_OK( TYInitLib() );
  TY_VERSION_INFO ver;
  ASSERT_OK( TYLibVersion(&ver) );
  LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

  std::vector<TY_DEVICE_BASE_INFO> selected;
  ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
  ASSERT(selected.size() > 0);
  TY_DEVICE_BASE_INFO& selectedDev = selected[0];

  ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
  ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

  TY_COMPONENT_ID allComps;
  ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
  if(!(allComps & TY_COMPONENT_RGB_CAM)){
    LOGE("=== Has no RGB camera, cant do registration");
    return -1;
  }

  ASSERT_OK( TYDisableComponents(hDevice, allComps) );
  
  LOGD("=== Configure components");
  TY_COMPONENT_ID componentIDs = TY_COMPONENT_DEPTH_CAM | TY_COMPONENT_RGB_CAM;
  ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

  bool hasUndistortSwitch, hasDistortionCoef;
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, &hasUndistortSwitch) );
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_DISTORTION, &hasDistortionCoef) );
  if (hasUndistortSwitch) {
    ASSERT_OK( TYSetBool(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, true) );
  }

  LOGD("=== Prepare image buffer");
  uint32_t frameSize;
  ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
  LOGD("     - Get size of framebuffer, %d", frameSize);
  LOGD("     - Allocate & enqueue buffers");
  char* frameBuffer[2];
  frameBuffer[0] = new char[frameSize];
  frameBuffer[1] = new char[frameSize];
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
  ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
  ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

  LOGD("=== Register event callback");
  ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

  bool hasTriggerParam = false;
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTriggerParam) );
  if (hasTriggerParam) {
    LOGD("=== Disable trigger mode");
    TY_TRIGGER_PARAM_EX trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
  }

  CallbackData cb_data;
  cb_data.index = 0;
  cb_data.hDevice = hDevice;

  float scale_unit = 1.;
  TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
  cb_data.scale_unit = scale_unit;

  LOGD("=== Read depth calib info");
  ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
      , &cb_data.depth_calib, sizeof(cb_data.depth_calib)) );

  LOGD("=== Read color calib info");
  ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA
      , &cb_data.color_calib, sizeof(cb_data.color_calib)) );

  LOGD("=== Start capture");
  ASSERT_OK( TYStartCapture(hDevice) );

  LOGD("=== Wait for callback");
  bool exit_main = false;

  while(!exit_main){
    TY_FRAME_DATA frame;
    int err = TYFetchFrame(hDevice, &frame, -1);
    if( err != TY_STATUS_OK ) {
      LOGE("Fetch frame error %d: %s", err, TYErrorString(err));
      break;
    }

    handleFrame(&frame, &cb_data);
/*
    int key = TYWaitKeyEvents();
    switch(key & 0xff){
      case 0xff:
        break;
      case 'q':
        exit_main = true;
        break;
      default:
        LOGD("Pressed key %d", key);
    }
*/
  }

  ASSERT_OK( TYStopCapture(hDevice) );
  ASSERT_OK( TYCloseDevice(hDevice) );
  ASSERT_OK( TYCloseInterface(hIface) );
  ASSERT_OK( TYDeinitLib() );
  delete frameBuffer[0];
  delete frameBuffer[1];

  LOGD("=== Main done!");
  return 0;
}
