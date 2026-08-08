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
#include <limits>
#include <chrono>

#include "TYImageProc.h"
#include "TYCoordinateMapper.h"
#include "TYFeatureList.h"

#include "CommandLineParser.hpp"
#include "CommandLineFeatureHelper.hpp"
#include "TYImageShow.h"

static inline void cin_clear_rest_line() {
    if (!std::cin) {
        std::cin.clear();
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

static std::string ty_comp_window_name(const TY_COMPONENT_ID compID, const uint32_t RegionID = 0) {
    switch(compID) {
        case TY_COMPONENT_RGB_CAM: return "Color_" + std::to_string(RegionID);
        case TY_COMPONENT_DEPTH_CAM: return "Depth_" + std::to_string(RegionID);
        case TY_COMPONENT_IR_CAM_LEFT: return "LeftIR_" + std::to_string(RegionID);
        case TY_COMPONENT_IR_CAM_RIGHT: return "RightIR_" + std::to_string(RegionID);
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

static float get_fps() {
    static int fps_counter = 0;
    static std::chrono::steady_clock::time_point fps_tm = std::chrono::steady_clock::now();
    const float kIntervalMs = 5000.0f;
    fps_counter++;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float elapsed_ms = std::chrono::duration<float, std::milli>(now - fps_tm).count();
    if (elapsed_ms < kIntervalMs) {
        return -elapsed_ms;
    }
    float v = fps_counter / (elapsed_ms / 1000.0f);
    fps_tm = now;
    fps_counter = 0;
    return v;
}

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
    auto win = ty_comp_window_name(image.componentID, image.regionID);
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
