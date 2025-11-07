
#include "ImageSpeckleFilter.hpp"

#include <stdio.h>

#include <cmath>
#include <cstring>
#include <stdexcept>

struct Point2s {
    Point2s(short _x, short _y) {
        x = _x;
        y = _y;
    }
    short x, y;
};

static const int DIST_SHIFT = 7;

void DepthSpeckleFilter::filterSpecklesImpl(uint16_t* image_data) {
    uint16_t u_max_diff = (uint16_t)(max_diff / depth_scale_unit);
    int width = depth_width;
    int height = depth_height;
    std::vector<char>& _buf = _labelBuf;
    const int npixels = width * height;                                                       // number of pixels
    const size_t bufSize = npixels * (int)(sizeof(Point2s) + sizeof(int) + sizeof(uint8_t));  // all pixel buffer
    if (_buf.size() < bufSize) {
        _buf.resize((int)bufSize);
    }

    uint8_t* buf = (uint8_t*)(&_buf[0]);
    int i, j, dstep = width;  //(int)(img.step / sizeof(T));
    int* labels = (int*)buf;
    buf += npixels * sizeof(labels[0]);
    Point2s* wbuf = (Point2s*)buf;
    buf += npixels * sizeof(wbuf[0]);
    uint8_t* rtype = (uint8_t*)buf;
    int curlabel = 0;

    // clear out label assignments
    memset(labels, 0, npixels * sizeof(labels[0]));

    for (i = 0; i < height; i++) {
        uint16_t* ds = image_data + i * width;
        int* ls = labels + width * i;  // label ptr for a row

        for (j = 0; j < width; j++) {
            if (ds[j] != new_val) {    // not a bad disparity
                if (ls[j]) {           // has a label, check for bad label
                    if (rtype[ls[j]])  // small region, zero out disparity
                        ds[j] = (uint16_t)new_val;
                }
                // no label, assign and propagate
                else {
                    int64_t dist_sum = 0;
                    Point2s* ws = wbuf;             // initialize wavefront
                    Point2s p((short)j, (short)i);  // current pixel
                    curlabel++;                     // next label
                    int count = 0;                  // current region size
                    ls[j] = curlabel;

                    // wavefront propagation
                    while (ws >= wbuf) {  // wavefront not empty
                        count++;
                        // put neighbors onto wavefront
                        uint16_t* dpp = image_data + p.y * width + p.x;
                        uint16_t dp = *dpp;
                        int* lpp = labels + width * p.y + p.x;

                        if (p.x < width - 1 && !lpp[+1] && dpp[+1] != new_val && std::abs(dp - dpp[+1]) <= max_diff) {
                            lpp[+1] = curlabel;
                            *ws++ = Point2s(p.x + 1, p.y);
                            dist_sum += dpp[+1];
                        }

                        if (p.x > 0 && !lpp[-1] && dpp[-1] != new_val && std::abs(dp - dpp[-1]) <= max_diff) {
                            lpp[-1] = curlabel;
                            *ws++ = Point2s(p.x - 1, p.y);
                            dist_sum += dpp[-1];
                        }

                        if (p.y < height - 1 && !lpp[+width] && dpp[+dstep] != new_val &&
                            std::abs(dp - dpp[+dstep]) <= max_diff) {
                            lpp[+width] = curlabel;
                            *ws++ = Point2s(p.x, p.y + 1);
                            dist_sum += dpp[+dstep];
                        }

                        if (p.y > 0 && !lpp[-width] && dpp[-dstep] != new_val &&
                            std::abs(dp - dpp[-dstep]) <= max_diff) {
                            lpp[-width] = curlabel;
                            *ws++ = Point2s(p.x, p.y - 1);
                            dist_sum += dpp[-dstep];
                        }

                        // pop most recent and propagate
                        // NB: could try least recent, maybe better convergence
                        p = *--ws;
                    }

                    int32_t avg_distance = 0;
                    if (count > 0) {
                        avg_distance = (int32_t)(dist_sum / count);  // average distance
                    }
                    // find the maximum speckle size for this distance
                    auto phy_max_speckle_size = physicSpeckleSizeLUTMap.upper_bound(avg_distance >> DIST_SHIFT)->second;

                    // assign label type
                    if (count <= max_speckle_size || count < phy_max_speckle_size) {  // speckle region
                        rtype[ls[j]] = 1;                                             // small region label
                        ds[j] = (uint16_t)new_val;
                    } else {
                        rtype[ls[j]] = 0;  // large region label
                    }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////

int DepthSpeckleFilter::init(int width, int height) {
    depth_width  = width;
    depth_height = height;
    physicSpeckleSizeLUTMap.clear();
    physicSpeckleSizeLUTMap[0] = 0;
    physicSpeckleSizeLUTMap[0xfffffff] = 0;
    const float fx = depth_cali_intri[0];
    const float phy_x = sqrt(max_physical_size);
    const int shift_val = (1 << DIST_SHIFT);
    const int end_idx = (0xfffff >> DIST_SHIFT) + 1;
    const float w_scale = float(depth_width) / depth_cali_width;
    for (int idx = 1; idx <= end_idx; idx++) {
        const float distance = (idx * shift_val) * depth_scale_unit + depth_offset;  // convert to mm
        if (distance <= 0.1) {
            physicSpeckleSizeLUTMap[idx] = 0;
        }
        const float w = phy_x / distance * fx * w_scale;
        float sz = (int)(w * w);
        sz = std::min(sz, float(0xffff));
        physicSpeckleSizeLUTMap[idx] = int(sz);
        if (sz < max_speckle_size) {
            break;
        }
    }
    return 0;
}

void DepthSpeckleFilter::Compute(uint16_t* image_data) {
    filterSpecklesImpl(image_data);
}
