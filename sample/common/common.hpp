#ifndef SAMPLE_COMMON_COMMON_HPP_
#define SAMPLE_COMMON_COMMON_HPP_

#include "Utils.hpp"

#include <fstream>
#include <iterator>
#include <cmath>
#include <string>
#include <memory>
#include <iostream>
#include <typeinfo>

#include "TYImageProc.h"
#include "TYCoordinateMapper.h"
#include "TYFeatureList.h"

#include "TYThread.hpp"
#include "CommandLineParser.hpp"
#include "CommandLineFeatureHelper.hpp"
#include "TYImageShow.h"

static std::string ty_comp_window_name(const TY_COMPONENT_ID compID) {
    switch(compID) {
        case TY_COMPONENT_RGB_CAM: return "Color";
        case TY_COMPONENT_DEPTH_CAM: return "Depth";
        case TY_COMPONENT_IR_CAM_LEFT: return "LeftIR";
        case TY_COMPONENT_IR_CAM_RIGHT: return "RightIR";
        default: return "Unknown";
    }
}

static TYImageInfo ty_image_info(const TY_IMAGE_DATA& image_data) {
    TYImageInfo info;
    info.width = image_data.width;
    info.height = image_data.height; 
    info.format = image_data.pixelFormat;
    info.dataSize = image_data.size;
    info.data = image_data.buffer; 
    return info;
}

enum{
    PC_FILE_FORMAT_XYZ = 0,
};

static void writePC_XYZ(const float* pnts, const uint8_t *color, size_t n, FILE* fp)
{
    if (color){
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[3*i + 0])){
                fprintf(fp, "%f %f %f %d %d %d\n", pnts[3*i + 0], pnts[3*i + 1], pnts[3*i + 2], color[3*i + 0], color[3*i + 1], color[3*i + 2]);
            }
        }
    }
    else{
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[3*i + 0])){
                fprintf(fp, "%f %f %f 0 0 0\n", pnts[3*i + 0], pnts[3*i + 1], pnts[3*i + 2]);
            }
        }
    }
}

static void writePointCloud(const float* pnts, const uint8_t* color, size_t n, const char* file, int format)
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

static inline TY_STATUS decode_and_display_image(const TY_IMAGE_DATA &image, const float depth_scale_unit = 1.f)
{
    if (image.status != TY_STATUS_OK) return TY_STATUS_ERROR;
    uint32_t destSize;
    auto win = ty_comp_window_name(image.componentID);
    TYImageInfo image_info = ty_image_info(image);
    TYDecodeError err = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_AUTO);
    switch (err) {
        case TY_DECODE_SUCCESS:{
            TYDecodeResult retInfo;
            std::vector<uint8_t> image_data(destSize);
            ASSERT_DEC_OK(TYDecodeImage(&image_info,  TY_OUTPUT_FORMAT_AUTO, (void*)&image_data[0], destSize, &retInfo));
            TYDisplayImage(win.c_str(), retInfo.width, retInfo.height, retInfo.format, &image_data[0]);
            break;
        }
        case TY_DECODE_NO_DECODE_NEEDED:
            TYDisplayImage(win.c_str(), image.width, image.height, image.pixelFormat, image.buffer, depth_scale_unit);
            break;
        default:
            LOGE("Get decode buffer size failed for %s, err: %d", win.c_str(), err);
            return TY_STATUS_ERROR;
    }
    return TY_STATUS_OK;
}

#endif
