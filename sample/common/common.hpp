#ifndef SAMPLE_COMMON_COMMON_HPP_
#define SAMPLE_COMMON_COMMON_HPP_

#include "Utils.hpp"

#include <fstream>
#include <iterator>

#include <memory>
#include <iostream>
#include <typeinfo>

#ifdef OPENCV_DEPENDENCIES
#include <opencv2/opencv.hpp>
#include "DepthRender.hpp"
#include "MatViewer.hpp"
#include "DepthInpainter.hpp"
#endif

#include "TYThread.hpp"
#include "CommandLineParser.hpp"
#include "CommandLineFeatureHelper.hpp"

static inline int decodeCsiRaw10(unsigned char* src, unsigned char* dst, int width, int height)
{
    if(width & 0x3) {
        return -1;
    }
    int raw10_line_size = 5 * width / 4;
    for(size_t i = 0, j = 0; i < raw10_line_size * height; i+=5, j+=4)
    {
        //[A9 - A2] | [B9 - B2] | [C9 - C2] | [D9 - D2] | [D1D0-C1C0-B1B0-A1A0]
        dst[j + 0] = src[i + 0];
        dst[j + 1] = src[i + 1];
        dst[j + 2] = src[i + 2];
        dst[j + 3] = src[i + 3];
    }
    return 0;
}

static inline int decodeCsiRaw12(unsigned char* src, unsigned char* dst, int width, int height)
{
    if(width & 0x1) {
        return -1;
    }
    int raw12_line_size = 3 * width / 2;
    for(size_t i = 0, j = 0; i < raw12_line_size * height; i+=3, j+=2)
    {
        //[A11 - A4] | [B11 - B4] | [B3B2B1B0-A3A2A1A0]
        dst[j + 0] = src[i + 0];
        dst[j + 1] = src[i + 1];
    }
    return 0;
}

static inline int decodeCsiRaw14(unsigned char* src, unsigned char* dst, int width, int height)
{
    if(width & 0x3) {
        return -1;
    }
    int raw14_line_size = 7 * width / 4;
    for(size_t i = 0, j = 0; i < raw14_line_size * height; i+=7, j+=4)
    {
        //[A13 - A6] | [B13 - B6] | [C13 - C6] | [D13 - D6] | [D5D4D3D2D1D0-C5C4] | [C3C2C1C0-B5B4B3B2] | [B1B0-A5A4A3A2A1A0]
        dst[j + 0] = src[i + 0];
        dst[j + 1] = src[i + 1];
        dst[j + 2] = src[i + 2];
        dst[j + 3] = src[i + 3];
    }
    return 0;
}

static inline int decodePacketRaw10(unsigned char* src, unsigned char* dst, int width, int height)
{
    if(width & 0x3) {
        return -1;
    }

    int raw10_line_size = 5 * width / 4;
    //    byte0  -        byte1    -      byte2     -     byte3      -  byte4
    // | A7 - A0 | B5 - B0 A9 - A8 | C3 -C0 B9 - B6 | D1 - D0 C9 -C4 | D9 -D2|
    for(size_t i = 0; i < raw10_line_size * height; i+=5)
    {
        //high 8bit
        dst[0] = (src[1] << 6) | (src[0] >> 2);
        dst[1] = (src[2] << 4) | (src[1] >> 4);
        dst[2] = (src[3] << 2) | (src[2] >> 6);
        dst[3] = src[4];

        src+=5;
        dst+=4;
    }
    return 0;
}

static inline int decodePacketRaw12(unsigned char* src, unsigned char* dst, int width, int height)
{
    //TODO...
    return 0;
}

#ifdef OPENCV_DEPENDENCIES
static inline int parseCsiRaw10(unsigned char* src, cv::Mat &dst, int width, int height)
{
    dst = cv::Mat(height, width, CV_8U);
    decodeCsiRaw10(src, dst.data, width, height);
    return 0;
}

static inline int parseCsiRaw12(unsigned char* src, cv::Mat &dst, int width, int height)
{
    dst = cv::Mat(height, width, CV_8U);
    decodeCsiRaw12(src, dst.data, width, height);
    return 0;
}

static inline int parsePacketRaw10(unsigned char* src, cv::Mat &dst, int width, int height)
{
    //TODO...
    dst = cv::Mat(height, width, CV_8U);
    decodePacketRaw10(src, dst.data, width, height);
    return 0;
}

static inline int parsePacketRaw12(unsigned char* src, cv::Mat &dst, int width, int height)
{
    //TODO...
    dst = cv::Mat(height, width, CV_8U);
    decodePacketRaw12(src, dst.data, width, height);
    return 0;
}

static inline int parseIrFrame(const TY_IMAGE_DATA* img, cv::Mat* pIR)
{
    if(img->pixelFormat == TYPixelFormatMono8) {
        *pIR = cv::Mat(img->height, img->width, CV_8U, img->buffer).clone();
    } else if(img->pixelFormat == TYPixelFormatMono10) {
        parseCsiRaw10((uchar*)img->buffer, *pIR, img->width, img->height);
    } else if(img->pixelFormat == TYPixelFormatMono12) {
        parseCsiRaw12((uchar*)img->buffer, *pIR, img->width, img->height);
    } else if(img->pixelFormat == TYPixelFormatPacketMono10) {
        parsePacketRaw10((uchar*)img->buffer, *pIR, img->width, img->height);
    } else if(img->pixelFormat == TYPixelFormatPacketMono12) {
        parsePacketRaw12((uchar*)img->buffer, *pIR, img->width, img->height);
    } else if (img->pixelFormat == TYPixelFormatMono16){
        *pIR = cv::Mat(img->height, img->width, CV_16U, img->buffer).clone();
    } else if (img->pixelFormat == TYPixelFormatTofIRFourGroupMono16){
        *pIR = cv::Mat(img->height * 2, img->width * 2, CV_16U, img->buffer).clone();
    } else {
        return -1;
    }

    return 0;
}

static inline int parseBayer8Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int code = cv::COLOR_BayerGB2BGR;
    switch (img->pixelFormat)
    {
    case TYPixelFormatBayerGBRG8:
        code = cv::COLOR_BayerGR2BGR;
        break;
    case TYPixelFormatBayerBGGR8:
        code = cv::COLOR_BayerRG2BGR;
        break;                
    case TYPixelFormatBayerGRBG8:
        code = cv::COLOR_BayerGB2BGR;
        break;                
    case TYPixelFormatBayerRGGB8:
        code = cv::COLOR_BayerBG2BGR;
        break;
    default:
        LOGE("Invalid bayer8 fmt!");
        return -1;
    }

    cv::Mat raw(img->height, img->width, CV_8U, img->buffer);
    cv::cvtColor(raw, *pColor, code);
    return 0;
}

static inline int parseBayer10Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int code = cv::COLOR_BayerGB2BGR;
    switch (img->pixelFormat)
    {
    case TYPixelFormatBayerGBRG10:
        code = cv::COLOR_BayerGR2BGR;
        break;
    case TYPixelFormatBayerBGGR10:
        code = cv::COLOR_BayerRG2BGR;
        break;                
    case TYPixelFormatBayerGRBG10:
        code = cv::COLOR_BayerGB2BGR;
        break;                
    case TYPixelFormatBayerRGGB10:
        code = cv::COLOR_BayerBG2BGR;
        break;
    default:
        LOGE("Invalid bayer10 fmt!");
        return -1;
    }
    cv::Mat raw;
    parseCsiRaw10((uchar*)img->buffer, raw, img->width, img->height);
    cv::cvtColor(raw, *pColor, code);
  
    return 0;
}

static inline int parseBayer12Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int code = cv::COLOR_BayerGB2BGR;
    switch (img->pixelFormat)
    {
    case TYPixelFormatBayerGBRG12:
        code = cv::COLOR_BayerGR2BGR;
        break;
    case TYPixelFormatBayerBGGR12:
        code = cv::COLOR_BayerRG2BGR;
        break;
    case TYPixelFormatBayerGRBG12:
        code = cv::COLOR_BayerGB2BGR;
        break;
    case TYPixelFormatBayerRGGB12:
        code = cv::COLOR_BayerBG2BGR;
        break;
    default:
        LOGE("Invalid bayer12 fmt!");
        return -1;
    }
    cv::Mat raw;
    parseCsiRaw12((uchar*)img->buffer, raw, img->width, img->height);
    cv::cvtColor(raw, *pColor, code);

    return 0;
}

static inline int parsePacketBayer10Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int code = cv::COLOR_BayerGB2BGR;
    switch (img->pixelFormat)
    {
    case TYPixelFormatPacketBayerGBRG10:
        code = cv::COLOR_BayerGR2BGR;
        break;
    case TYPixelFormatPacketBayerBGGR10:
        code = cv::COLOR_BayerRG2BGR;
        break;                
    case TYPixelFormatPacketBayerGRBG10:
        code = cv::COLOR_BayerGB2BGR;
        break;                
    case TYPixelFormatPacketBayerRGGB10:
        code = cv::COLOR_BayerBG2BGR;
        break;
    default:
        LOGE("Invalid packet bayer10 fmt!");
        return -1;
    }
    cv::Mat raw;
    parsePacketRaw10((uchar*)img->buffer, raw, img->width, img->height);
    cv::cvtColor(raw, *pColor, code);
    return 0;
}

static inline int parsePacketBayer12Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int code = cv::COLOR_BayerGB2BGR;
    switch (img->pixelFormat)
    {
    case TYPixelFormatPacketBayerGBRG12:
        code = cv::COLOR_BayerGR2BGR;
        break;
    case TYPixelFormatPacketBayerBGGR12:
        code = cv::COLOR_BayerRG2BGR;
        break;                
    case TYPixelFormatPacketBayerGRBG12:
        code = cv::COLOR_BayerGB2BGR;
        break;                
    case TYPixelFormatPacketBayerRGGB12:
        code = cv::COLOR_BayerBG2BGR;
        break;
    default:
        LOGE("Invalid packet bayer12 fmt!");
        return -1;
    }
    cv::Mat raw;
    parsePacketRaw12((uchar*)img->buffer, raw, img->width, img->height);
    cv::cvtColor(raw, *pColor, code);
    return 0;
}

static inline int parseColorFrame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
    int ret = 0;
    if (img->pixelFormat == TYPixelFormatJPEG){
        std::vector<uchar> _v((uchar*)img->buffer, (uchar*)img->buffer + img->size);
        *pColor = cv::imdecode(_v, cv::IMREAD_COLOR);
        ASSERT(img->width == pColor->cols && img->height == pColor->rows);
    }
    else if (img->pixelFormat == TYPixelFormatYUV422_8){
        cv::Mat yuv(img->height, img->width, CV_8UC2, img->buffer);
        cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YUYV);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CbCr_Planar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *pColor, cv::COLOR_YUV2BGR_I420);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CrCb_Planar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *pColor, cv::COLOR_YUV420p2BGR);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CbCr_Semiplanar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *pColor, cv::COLOR_YUV2BGR_NV12);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CrCb_Semiplanar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *pColor, cv::COLOR_YUV2BGR_NV21);
    }
    else if (img->pixelFormat == TYPixelFormatRGB8){
        cv::Mat rgb(img->height, img->width, CV_8UC3, img->buffer);
        cv::cvtColor(rgb, *pColor, cv::COLOR_RGB2BGR);
    }
    else if (img->pixelFormat == TYPixelFormatBGR8){
        *pColor = cv::Mat(img->height, img->width, CV_8UC3, img->buffer).clone();
    }
    else if (img->pixelFormat == TYPixelFormatBayerGBRG8 || 
            img->pixelFormat == TYPixelFormatBayerBGGR8 || 
            img->pixelFormat == TYPixelFormatBayerGRBG8 || 
            img->pixelFormat == TYPixelFormatBayerRGGB8) 
    {
        ret = parseBayer8Frame(img, pColor);
    }
    else if (img->pixelFormat == TYPixelFormatBayerGBRG10 || 
            img->pixelFormat == TYPixelFormatBayerBGGR10 || 
            img->pixelFormat == TYPixelFormatBayerGRBG10 || 
            img->pixelFormat == TYPixelFormatBayerRGGB10) 
    {
        ret = parseBayer10Frame(img, pColor);
    }
    else if(img->pixelFormat == TYPixelFormatBayerGBRG12 || 
            img->pixelFormat == TYPixelFormatBayerBGGR12 || 
            img->pixelFormat == TYPixelFormatBayerGRBG12 || 
            img->pixelFormat == TYPixelFormatBayerRGGB12) 
    {
        ret = parseBayer12Frame(img, pColor);
    }
    else if(img->pixelFormat == TYPixelFormatPacketBayerGBRG10 || 
            img->pixelFormat == TYPixelFormatPacketBayerBGGR10 || 
            img->pixelFormat == TYPixelFormatPacketBayerGRBG10 || 
            img->pixelFormat == TYPixelFormatPacketBayerRGGB10)
    {
        ret = parsePacketBayer10Frame(img, pColor);
    }
    else if(img->pixelFormat == TYPixelFormatPacketBayerGBRG12 || 
        img->pixelFormat == TYPixelFormatPacketBayerBGGR12 || 
        img->pixelFormat == TYPixelFormatPacketBayerGRBG12 || 
        img->pixelFormat == TYPixelFormatPacketBayerRGGB12) 
    {
        ret = parsePacketBayer12Frame(img, pColor);
    }
    else if (img->pixelFormat == TYPixelFormatMono8){
        cv::Mat gray(img->height, img->width, CV_8U, img->buffer);
        cv::cvtColor(gray, *pColor, cv::COLOR_GRAY2BGR);
    }
    else if (img->pixelFormat == TYPixelFormatMono10){
        parseCsiRaw10((uchar*)img->buffer, *pColor, img->width, img->height);
    }
    else if(img->pixelFormat == TYPixelFormatMono12) {
        parseCsiRaw12((uchar*)img->buffer, *pColor, img->width, img->height);
    }
    else if (img->pixelFormat == TYPixelFormatPacketMono10){
        parsePacketRaw10((uchar*)img->buffer, *pColor, img->width, img->height);
    }
    else if(img->pixelFormat == TYPixelFormatPacketMono12) {
        parsePacketRaw12((uchar*)img->buffer, *pColor, img->width, img->height);
    }

    return ret;
}

static inline int parseImage(const TY_IMAGE_DATA* img, cv::Mat* image)
{
    int ret = 0;
    if (img->pixelFormat == TYPixelFormatJPEG){
        std::vector<uchar> _v((uchar*)img->buffer, (uchar*)img->buffer + img->size);
        *image = cv::imdecode(_v, cv::IMREAD_COLOR);
        ASSERT(img->width == image->cols && img->height == image->rows);
    }
    else if (img->pixelFormat == TYPixelFormatYUV422_8){
        cv::Mat yuv(img->height, img->width, CV_8UC2, img->buffer);
        cv::cvtColor(yuv, *image, cv::COLOR_YUV2BGR_YUYV);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CbCr_Planar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *image, cv::COLOR_YUV2BGR_I420);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CrCb_Planar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *image, cv::COLOR_YUV420p2BGR);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CbCr_Semiplanar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *image, cv::COLOR_YUV2BGR_NV12);
    }
    else if(img->pixelFormat == TYPixelFormatYCbCr420_8_YY_CrCb_Semiplanar) {
        cv::Mat yuv420(img->height + img->height/2, img->width, CV_8UC1, img->buffer);
        cv::cvtColor(yuv420, *image, cv::COLOR_YUV2BGR_NV21);
    }
    else if (img->pixelFormat == TYPixelFormatRGB8){
        cv::Mat rgb(img->height, img->width, CV_8UC3, img->buffer);
        cv::cvtColor(rgb, *image, cv::COLOR_RGB2BGR);
    }
    else if (img->pixelFormat == TYPixelFormatBGR8){
        *image = cv::Mat(img->height, img->width, CV_8UC3, img->buffer).clone();
    }
    else if (img->pixelFormat == TYPixelFormatBayerGBRG8 || 
            img->pixelFormat == TYPixelFormatBayerBGGR8 || 
            img->pixelFormat == TYPixelFormatBayerGRBG8 || 
            img->pixelFormat == TYPixelFormatBayerRGGB8) 
    {
        ret = parseBayer8Frame(img, image);
    }
    else if (img->pixelFormat == TYPixelFormatBayerGBRG10 || 
            img->pixelFormat == TYPixelFormatBayerBGGR10 || 
            img->pixelFormat == TYPixelFormatBayerGRBG10 || 
            img->pixelFormat == TYPixelFormatBayerRGGB10) 
    {
        ret = parseBayer10Frame(img, image);
    }
    else if(img->pixelFormat == TYPixelFormatBayerGBRG12 || 
            img->pixelFormat == TYPixelFormatBayerBGGR12 || 
            img->pixelFormat == TYPixelFormatBayerGRBG12 || 
            img->pixelFormat == TYPixelFormatBayerRGGB12) 
    {
        ret = parseBayer12Frame(img, image);
    }
    else if (img->pixelFormat == TYPixelFormatPacketBayerGBRG10 || 
        img->pixelFormat == TYPixelFormatPacketBayerBGGR10 || 
        img->pixelFormat == TYPixelFormatPacketBayerGRBG10 || 
        img->pixelFormat == TYPixelFormatPacketBayerRGGB10) 
    {
        ret = parsePacketBayer10Frame(img, image);
    }
    else if(img->pixelFormat == TYPixelFormatPacketBayerGBRG12 || 
            img->pixelFormat == TYPixelFormatPacketBayerBGGR12 || 
            img->pixelFormat == TYPixelFormatPacketBayerGRBG12 || 
            img->pixelFormat == TYPixelFormatPacketBayerRGGB12) 
    {
        ret = parsePacketBayer12Frame(img, image);
    }

    else if(img->pixelFormat == TYPixelFormatMono8) {
        *image = cv::Mat(img->height, img->width, CV_8U, img->buffer).clone();
    }
    else if (img->pixelFormat == TYPixelFormatMono10){
        ret = parseCsiRaw10((uchar*)img->buffer, *image, img->width, img->height);
    }
    else if (img->pixelFormat == TYPixelFormatPacketMono10){
        ret = parsePacketRaw10((uchar*)img->buffer, *image, img->width, img->height);
    }
    else if(img->pixelFormat == TYPixelFormatMono12) {
        ret = parseCsiRaw12((uchar*)img->buffer, *image, img->width, img->height);
    } 
    else if(img->pixelFormat == TYPixelFormatPacketMono12) {
        ret = parsePacketRaw12((uchar*)img->buffer, *image, img->width, img->height);
    } 
    else if (img->pixelFormat == TYPixelFormatMono16){
        *image = cv::Mat(img->height, img->width, CV_16U, img->buffer).clone();
    }
    else if (img->pixelFormat == TYPixelFormatTofIRFourGroupMono16) {
        *image = cv::Mat(img->height * 2, img->width * 2, CV_16U, img->buffer).clone();
    }
    else {
        return -1;
    }

    return ret;
}

static inline int parseFrame(const TY_FRAME_DATA& frame, cv::Mat* pDepth
                             , cv::Mat* pLeftIR, cv::Mat* pRightIR
                             , cv::Mat* pColor)
{
    for (int i = 0; i < frame.validCount; i++){
        if (frame.image[i].status != TY_STATUS_OK) continue;

        // get depth image
        if (pDepth && frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM){
            if (frame.image[i].pixelFormat == TYPixelFormatCoord3D_ABC16) {
                *pDepth = cv::Mat(frame.image[i].height, frame.image[i].width
                              , CV_16SC3, frame.image[i].buffer).clone();
            }
          else {
              *pDepth = cv::Mat(frame.image[i].height, frame.image[i].width
                            , CV_16U, frame.image[i].buffer).clone();
          }
        }
        // get left ir image
        if (pLeftIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT){
            parseIrFrame(&frame.image[i], pLeftIR);
        }
        // get right ir image
        if (pRightIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT){
            parseIrFrame(&frame.image[i], pRightIR);
        }
        // get BGR
        if (pColor && frame.image[i].componentID == TY_COMPONENT_RGB_CAM){
            parseColorFrame(&frame.image[i], pColor);
        }
    }

    return 0;
}

enum{
    PC_FILE_FORMAT_XYZ = 0,
};

static void writePC_XYZ(const cv::Point3f* pnts, const cv::Vec3b *color, size_t n, FILE* fp)
{
    if (color){
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f %d %d %d\n", pnts[i].x, pnts[i].y, pnts[i].z, color[i][0], color[i][1], color[i][2]);
            }
        }
    }
    else{
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f 0 0 0\n", pnts[i].x, pnts[i].y, pnts[i].z);
            }
        }
    }
}

static void writePointCloud(const cv::Point3f* pnts, const cv::Vec3b *color, size_t n, const char* file, int format)
{
    FILE* fp = fopen(file, "w");
    if (!fp){
        return;
    }

    switch (format){
    case PC_FILE_FORMAT_XYZ:
        writePC_XYZ(pnts, color, n, fp);
        break;
    default:
        break;
    }

    fclose(fp);
}
#else


#endif

class CallbackWrapper
{
public:
    typedef void(*TY_FRAME_CALLBACK) (TY_FRAME_DATA*, void* userdata);

    CallbackWrapper(){
        _hDevice = NULL;
        _cb = NULL;
        _userdata = NULL;
        _exit = true;
    }

    TY_STATUS TYRegisterCallback(TY_DEV_HANDLE hDevice, TY_FRAME_CALLBACK v, void* userdata)
    {
        _hDevice = hDevice;
        _cb = v;
        _userdata = userdata;
        _exit = false;
        _cbThread.create(&workerThread, this);
        return TY_STATUS_OK;
    }

    void TYUnregisterCallback()
    {
        if (!_exit) {
            _exit = true;
            _cbThread.destroy();
        }
    }

private:
    static void* workerThread(void* userdata)
    {
        CallbackWrapper* pWrapper = (CallbackWrapper*)userdata;
        TY_FRAME_DATA frame;

        while (!pWrapper->_exit)
        {
            int err = TYFetchFrame(pWrapper->_hDevice, &frame, 100);
            if (!err) {
                pWrapper->_cb(&frame, pWrapper->_userdata);
            }
        }
        LOGI("frameCallback exit!");
        return NULL;
    }

    TY_DEV_HANDLE _hDevice;
    TY_FRAME_CALLBACK _cb;
    void* _userdata;

    bool _exit;
    TYThread _cbThread;
};



#ifdef _WIN32
static int get_fps() {
    static int fps_counter = 0;
    static clock_t fps_tm = 0;
   const int kMaxCounter = 250;
   fps_counter++;
   if (fps_counter < kMaxCounter) {
     return -1;
   }
   int elapse = (clock() - fps_tm);
   int v = (int)(((float)fps_counter) / elapse * CLOCKS_PER_SEC);
   fps_tm = clock();

   fps_counter = 0;
   return v;
 }
#else
static int get_fps() {
    static int fps_counter = 0;
    static clock_t fps_tm = 0;
    const int kMaxCounter = 200;
    struct timeval start;
    fps_counter++;
    if (fps_counter < kMaxCounter) {
        return -1;
    }

    gettimeofday(&start, NULL);
    int elapse = start.tv_sec * 1000 + start.tv_usec / 1000 - fps_tm;
    int v = (int)(((float)fps_counter) / elapse * 1000);
    gettimeofday(&start, NULL);
    fps_tm = start.tv_sec * 1000 + start.tv_usec / 1000;

    fps_counter = 0;
    return v;
}
#endif

static std::vector<uint8_t> TYReadBinaryFile(const char* filename)
{
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()){
        return std::vector<uint8_t>();
    }
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    std::vector<uint8_t> vec;
    vec.reserve(fileSize);

    // read the data:
    vec.insert(vec.begin(),
               std::istream_iterator<uint8_t>(file),
               std::istream_iterator<uint8_t>());

    return vec;
}

#endif
