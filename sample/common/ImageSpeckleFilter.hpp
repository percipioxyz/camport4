#pragma once

#include <map>
#include <vector>
#include <cstdint>

class DepthSpeckleFilter {
   public:
    DepthSpeckleFilter() {
        new_val = 0;
        max_speckle_size = 50;
        max_diff = 6;
        depth_cali_width = -1;
        depth_cali_height = -1;
        max_physical_size = 20.0;
        depth_width = -1;
        depth_height = -1;
        depth_offset = 0;
        depth_scale_unit = 1;
        max_physical_size = 0;//0 means speckle physical size filter is not enable
    }

    /**
     * @brief init internal lut and other infos
     * @param width  width of depth image
     * @param height  height of depth image
     */

    int init(int width, int height);

    /**
     * @brief compute and filter depth image, it will be done in place
     * @param image_data  depth image data pointer, need be uint16_t data without any padding
     */
    void Compute(uint16_t *image_data);

    int new_val;               // new val, used to replace the pixel values in the speckle region
    double max_diff;              // Maximum difference value, used to determine the speckle region, physical Z diff(Not just depth data val diff)
    int max_speckle_size;      // max speckle size in unit of pixel
    double max_physical_size;  // max Speckle Physical Size to be Filtered-Out, uint is mm^2

    int depth_width;
    int depth_height;

    // calibration parameters
    double depth_scale_unit;     // scale uint of depth, actual val = val * scale_unit
    double depth_offset;         // Offset of depth, actual val = val * scale_unit + offset
    double depth_cali_intri[9];  // intrinsic of depth
    int depth_cali_width;        // width correspondence to intrinsic resolution
    int depth_cali_height;       // height correspondence to intrinsic resolution

   private:
    std::vector<char> _labelBuf;
    // Phisical Speckle Size LUT
    // key: distance in depth (not physic unit) , value : speckle size
    std::map<int, uint32_t> physicSpeckleSizeLUTMap;

    void filterSpecklesImpl(uint16_t *image_data);
};
