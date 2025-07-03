#include "Device.hpp"
#include <cmath>

using namespace percipio_layer;
class TofPhasetoIrPostProcess: public ImageProcesser {
public:
    TofPhasetoIrPostProcess():ImageProcesser("ir") {}
    int parse(const std::shared_ptr<TYImage>& image) {
      ImageProcesser::parse(image);

      _image = std::shared_ptr<TYImage>(new TYImage(*image));
      uint16_t *output = (uint16_t *)_image->buffer();
      switch (image->pixelFormat()) {
        case TYPixelFormatTofIRFourGroupMono16: {
          int depthwidth = image->width();
          int depthheight = image->height();
          int heightd4_x_width = depthwidth*depthheight;

          uint16_t * phase0 = (uint16_t *)(image->buffer());
          uint16_t * phase180 = &(phase0[heightd4_x_width]);
          uint16_t * phase90 = &(phase0[heightd4_x_width * 2]);
          uint16_t * phase270 = &(phase0[heightd4_x_width * 3]);
  
          uint16_t ulum_0, ulum_90, ulum_180, ulum_270;// 
          int16_t lum_0, lum_90, lum_180, lum_270;
          int deltsin, deltcos, lummod;
          for (int line = 0; line < depthheight; line++)
          {
            for (int ci = 0; ci < depthwidth; ci++)
            {
              int onestarti_depth = (line*depthwidth) + ci;
              ulum_0 = ((phase0[onestarti_depth]) );
              ulum_90 = ((phase90[onestarti_depth]) );
              ulum_180 = ((phase180[onestarti_depth]) );
              ulum_270 = ((phase270[onestarti_depth]) );
  
              lum_180 = ulum_0; 
              lum_90 = ulum_180;
              lum_0 = ulum_90;
              lum_270 = ulum_270;
  
              deltsin = (int32_t(lum_90) - int32_t(lum_270)) ;
              deltcos = (int32_t(lum_0) - int32_t(lum_180)) ;
              lummod = ((int32_t(std::sqrt(deltsin*deltsin + deltcos*deltcos))) *2);
              //phase270[onestarti_depth] = lummod;
              output[onestarti_depth] = lummod;
            }
          }
          return TY_STATUS_OK;
        }
        default:
          return TY_STATUS_NOT_IMPLEMENTED;
      }
    }
};

class TofCamera : public FastCamera
{
    public:
        TofCamera() : FastCamera() {};
        ~TofCamera() {}; 

        TY_STATUS Init();
        bool needUndistortion() {return depth_needUndistort;}
        TY_CAMERA_CALIB_INFO &getCalib() { return depth_calib;} 
    private:
        float f_depth_scale_unit = 1.f;
        bool depth_needUndistort = false;
        TY_CAMERA_CALIB_INFO depth_calib;
};

TY_STATUS TofCamera::Init()
{
    TY_STATUS status;
    status = stream_enable(stream_ir);
    if(status != TY_STATUS_OK) return status;
    
    TYGetFloat(handle(), TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &f_depth_scale_unit);
    TYHasFeature(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &depth_needUndistort);
    TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &depth_calib, sizeof(depth_calib));
    return TY_STATUS_OK;
}


int main(int argc, char* argv[])
{
    std::string ID;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }

    TofCamera tof;
    if(TY_STATUS_OK != tof.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;
    tof.Init();
    TYFrameParser parser;
    std::shared_ptr<ImageProcesser> postir_process = std::shared_ptr<ImageProcesser>(new TofPhasetoIrPostProcess());
    parser.setImageProcesser(TY_COMPONENT_IR_CAM_LEFT, postir_process);
    
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != tof.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }


    while(!process_exit) {
        auto frame = tof.tryGetFrames(2000);
        if(frame) {
            parser.update(frame);
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
