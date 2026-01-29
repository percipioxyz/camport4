#include "Device.hpp"
namespace {

std::string TY_ERROR(TY_STATUS status)
{
    std::ostringstream ss;
    ss << status << "(" << TYErrorString(status) << ").";
    return ss.str();
}

TY_STATUS searchDevice(std::vector<TY_DEVICE_BASE_INFO>& out, 
                      const char* inf_id = nullptr, 
                      TY_INTERFACE_TYPE type = TY_INTERFACE_ALL)
{
    out.clear();
    
    if (TYUpdateInterfaceList() != TY_STATUS_OK) {
        return TY_STATUS_ERROR;
    }

    uint32_t n = 0;
    if (TYGetInterfaceNumber(&n) != TY_STATUS_OK || n == 0) {
        return TY_STATUS_ERROR;
    }

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    if (TYGetInterfaceList(ifaces.data(), n, &n) != TY_STATUS_OK) {
        return TY_STATUS_ERROR;
    }

    std::vector<TY_INTERFACE_HANDLE> hIfaces;
    hIfaces.reserve(ifaces.size());
    
    auto closeInterfaces = [](std::vector<TY_INTERFACE_HANDLE>& handles) {
        for (auto hIface : handles) {
            if (hIface) {
                TYCloseInterface(hIface);
            }
        }
        handles.clear();
    };
    
    for (const auto& iface : ifaces) {
        if (type & iface.type) {
            if (!inf_id || strcmp(inf_id, iface.id) == 0) {
                TY_INTERFACE_HANDLE hIface = nullptr;
                TY_STATUS status = TYOpenInterface(iface.id, &hIface);
                if (status == TY_STATUS_OK && hIface) {
                    hIfaces.push_back(hIface);
                    if (inf_id) break; //find the interface setted, just return
                }
            }
        }
    }

    if (hIfaces.empty()) {
        return TY_STATUS_ERROR;
    }

    updateDevicesParallel(hIfaces, static_cast<uint32_t>(hIfaces.size()));

    for (auto hIface : hIfaces) {
        uint32_t deviceCount = 0;
        if (TYGetDeviceNumber(hIface, &deviceCount) != TY_STATUS_OK) {
            continue;
        }
        
        if (deviceCount > 0) {
            std::vector<TY_DEVICE_BASE_INFO> devs(deviceCount);
            if (TYGetDeviceList(hIface, devs.data(), deviceCount, &deviceCount) == TY_STATUS_OK) {
                out.insert(out.end(), devs.begin(), devs.begin() + deviceCount);
            }
        }
    }

    closeInterfaces(hIfaces);

    if (out.empty()) {
        std::cout << "No devices found" << std::endl;
        return TY_STATUS_ERROR;
    }

    return TY_STATUS_OK;
}

} // anonymous namespace

namespace percipio_layer {

TYDeviceInfo::TYDeviceInfo(const TY_DEVICE_BASE_INFO& info)
    : _info(info)
{
}

const char* TYDeviceInfo::mac() const
{
    return TYIsNetworkInterface(_info.iface.type) ? _info.netInfo.mac : nullptr;
}

const char* TYDeviceInfo::ip() const
{
    return TYIsNetworkInterface(_info.iface.type) ? _info.netInfo.ip : nullptr;
}

const char* TYDeviceInfo::netmask() const
{
    return TYIsNetworkInterface(_info.iface.type) ? _info.netInfo.netmask : nullptr;
}

const char* TYDeviceInfo::gateway() const
{
    return TYIsNetworkInterface(_info.iface.type) ? _info.netInfo.gateway : nullptr;
}

const char* TYDeviceInfo::broadcast() const
{
    return TYIsNetworkInterface(_info.iface.type) ? _info.netInfo.broadcast : nullptr;
}

// TYDevice
TYDevice::TYDevice(TY_DEV_HANDLE handle, const TY_DEVICE_BASE_INFO& info)
    : _handle(handle), _dev_info(info)
{
    _event_callback = [this](const TY_EVENT_INFO* event_info) {
        this->onDeviceEventCallback(event_info);
    };
    TYRegisterEventCallback(_handle, &TYDevice::eventCallbackWrapper, this);
}

TYDevice::~TYDevice()
{
    if (_handle) {
        TYCloseDevice(_handle);
    }
}

TYDevice::TYDevice(TYDevice&& other) noexcept
    : _handle(other._handle),
      _dev_info(std::move(other._dev_info)),
      _eventCallbackMap(std::move(other._eventCallbackMap)),
      _event_callback(std::move(other._event_callback))
{
    other._handle = nullptr;
    if (_handle) {
        TYRegisterEventCallback(_handle, &TYDevice::eventCallbackWrapper, this);
    }
}

TYDevice& TYDevice::operator=(TYDevice&& other) noexcept
{
    if (this != &other) {
        if (_handle) {
            TYCloseDevice(_handle);
        }
        
        _handle = other._handle;
        _dev_info = std::move(other._dev_info);
        _eventCallbackMap = std::move(other._eventCallbackMap);
        _event_callback = std::move(other._event_callback);
        
        other._handle = nullptr;
        
        if (_handle) {
            TYRegisterEventCallback(_handle, &TYDevice::eventCallbackWrapper, this);
        }
    }
    return *this;
}

void TYDevice::eventCallbackWrapper(TY_EVENT_INFO* event_info, void* userdata)
{
    TYDevice* device = static_cast<TYDevice*>(userdata);
    if (device && device->_event_callback) {
        device->_event_callback(event_info);
    }
}

void TYDevice::registerEventCallback(TY_EVENT eventID, void* data, EventCallback cb)
{
    _eventCallbackMap[eventID] = std::make_pair(data, std::move(cb));
}

void TYDevice::onDeviceEventCallback(const TY_EVENT_INFO* event_info)
{
    auto it = _eventCallbackMap.find(event_info->eventId);
    if (it != _eventCallbackMap.end() && it->second.second) {
        it->second.second(it->second.first);
    }
}

std::shared_ptr<TYDeviceInfo> TYDevice::getDeviceInfo() const
{
    return std::make_shared<TYDeviceInfo>(_dev_info);
}

// DeviceList 
DeviceList::DeviceList(std::vector<TY_DEVICE_BASE_INFO> devices)
    : devs(std::move(devices))
{
}

DeviceList::~DeviceList()
{
    for (TY_INTERFACE_HANDLE iface : gifaces) {
        if (iface) {
            TYCloseInterface(iface);
        }
    }
    gifaces.clear();
}

std::shared_ptr<TYDeviceInfo> DeviceList::getDeviceInfo(size_t idx) const
{
    if (idx >= devs.size()) {
        std::cerr << "Index out of range" << std::endl;
        return nullptr;
    }
    return std::make_shared<TYDeviceInfo>(devs[idx]);
}

std::shared_ptr<TYDevice> DeviceList::getDevice(size_t idx) const
{
    if (idx >= devs.size()) {
        std::cerr << "Index out of range" << std::endl;
        return nullptr;
    }

    TY_INTERFACE_HANDLE hIface = nullptr;
    TY_STATUS status = TYOpenInterface(devs[idx].iface.id, &hIface);
    if (status != TY_STATUS_OK || !hIface) {
        std::cerr << "Open interface failed: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }

    gifaces.insert(hIface);
    
    std::string ifaceId = devs[idx].iface.id;
    std::cout << "Open device " << devs[idx].id 
              << " on interface " << parseInterfaceID(ifaceId) << std::endl;

    TY_DEV_HANDLE hDevice = nullptr;
    status = TYOpenDevice(hIface, devs[idx].id, &hDevice);
    if (status != TY_STATUS_OK) {
        std::cerr << "Open device failed: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }

    TY_DEVICE_BASE_INFO info;
    if (TYGetDeviceInfo(hDevice, &info) != TY_STATUS_OK) {
        TYCloseDevice(hDevice);
        std::cerr << "Get device info failed" << std::endl;
        return nullptr;
    }

    return std::make_shared<TYDevice>(hDevice, info);
}

std::shared_ptr<TYDevice> DeviceList::getDeviceBySN(const char* sn) const
{
    if (!sn) {
        std::cerr << "Invalid parameters" << std::endl;
        return nullptr;
    }

    auto it = std::find_if(devs.begin(), devs.end(),
        [sn](const TY_DEVICE_BASE_INFO& info) {
            return strcmp(info.id, sn) == 0;
        });

    if (it == devs.end()) {
        std::cerr << "Device SN:" << sn << " not found!" << std::endl;
        return nullptr;
    }

    return getDevice(std::distance(devs.begin(), it));
}

std::shared_ptr<TYDevice> DeviceList::getDeviceByIP(const char* ip) const
{
    if (!ip) {
        std::cerr << "Invalid parameters" << std::endl;
        return nullptr;
    }

    for (size_t i = 0; i < devs.size(); ++i) {
        if (TYIsNetworkInterface(devs[i].iface.type)) {
            TY_INTERFACE_HANDLE hIface = nullptr;
            TY_STATUS status = TYOpenInterface(devs[i].iface.id, &hIface);
            if (status != TY_STATUS_OK || !hIface) {
                continue;
            }

            gifaces.insert(hIface);
            
            std::string open_log = "Open device ";
            TY_DEV_HANDLE hDevice = nullptr;
            
            if (strlen(ip) > 0) {
                open_log += ip;
                status = TYOpenDeviceWithIP(hIface, ip, &hDevice);
            } else {
                open_log += devs[i].id;
                status = TYOpenDevice(hIface, devs[i].id, &hDevice);
            }
            
            open_log += "\non interface " + parseInterfaceID(devs[i].iface.id);
            std::cout << open_log << std::endl;

            if (status != TY_STATUS_OK) {
                continue;
            }

            TY_DEVICE_BASE_INFO info;
            if (TYGetDeviceInfo(hDevice, &info) != TY_STATUS_OK) {
                TYCloseDevice(hDevice);
                continue;
            }

            return std::make_shared<TYDevice>(hDevice, info);
        }
    }

    std::cerr << "Device IP:" << ip << " not found!" << std::endl;
    return nullptr;
}

// TYContext 
TYContext::TYContext()
{
    if (TYInitLib() != TY_STATUS_OK) {
        throw std::runtime_error("Failed to initialize library");
    }
    
    TY_VERSION_INFO ver;
    if (TYLibVersion(&ver) == TY_STATUS_OK) {
        std::cout << "=== lib version: " << ver.major << "." 
                  << ver.minor << "." << ver.patch << std::endl;
    }
}

TYContext::~TYContext()
{
    TYDeinitLib();
}

std::shared_ptr<DeviceList> TYContext::queryDeviceList(const char* iface) const
{
    std::vector<TY_DEVICE_BASE_INFO> devs;
    if (searchDevice(devs, iface) == TY_STATUS_OK) {
        return std::make_shared<DeviceList>(std::move(devs));
    }
    return std::make_shared<DeviceList>(std::vector<TY_DEVICE_BASE_INFO>());
}

std::shared_ptr<DeviceList> TYContext::queryNetDeviceList(const char* iface) const
{
    std::vector<TY_DEVICE_BASE_INFO> devs;
    if (searchDevice(devs, iface, TY_INTERFACE_ETHERNET | TY_INTERFACE_IEEE80211) == TY_STATUS_OK) {
        return std::make_shared<DeviceList>(std::move(devs));
    }
    return std::make_shared<DeviceList>(std::vector<TY_DEVICE_BASE_INFO>());
}

bool TYContext::ForceNetDeviceIP(ForceIPStyle style, const std::string& mac,
                                const std::string& ip, const std::string& mask,
                                const std::string& gateway) const
{
    if (TYUpdateInterfaceList() != TY_STATUS_OK) {
        return false;
    }

    uint32_t n = 0;
    if (TYGetInterfaceNumber(&n) != TY_STATUS_OK || n == 0) {
        return false;
    }

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    if (TYGetInterfaceList(ifaces.data(), n, &n) != TY_STATUS_OK) {
        return false;
    }

    const char* ip_save = ip.c_str();
    const char* netmask_save = mask.c_str();
    const char* gateway_save = gateway.c_str();
    bool open_needed = false;

    switch (style) {
        case ForceIPStyle::Dynamic:
            if (ip != "0.0.0.0") {
                open_needed = true;
            }
            ip_save = "0.0.0.0";
            netmask_save = "0.0.0.0";
            gateway_save = "0.0.0.0";
            break;
        case ForceIPStyle::Static:
            open_needed = true;
            break;
        case ForceIPStyle::Force:
            break;
    }

    bool result = false;
    
    for (const auto& iface : ifaces) {
        if (TYIsNetworkInterface(iface.type)) {
            TY_INTERFACE_HANDLE hIface = nullptr;
            if (TYOpenInterface(iface.id, &hIface) != TY_STATUS_OK || !hIface) {
                continue;
            }

            if (TYForceDeviceIP(hIface, mac.c_str(), ip.c_str(), 
                               mask.c_str(), gateway.c_str()) == TY_STATUS_OK) {
                std::cout << "Set Temporary IP/Netmask/Gateway ...Done!" << std::endl;
                
                if(open_needed) {
                    TYUpdateDeviceList(hIface);
                    TY_DEV_HANDLE hDev;
                    if(TYOpenDeviceWithIP(hIface, ip.c_str(), &hDev) == TY_STATUS_OK){
                        int32_t ip_i[4];
                        uint8_t ip_b[4];
                        int32_t ip32;
                        sscanf(ip_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_IP, ip32) );
                        sscanf(netmask_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_SUBMASK, ip32) );
                        sscanf(gateway_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_GATEWAY, ip32) );

                        result = true;
                        std::cout << "**** Set Persistent IP/Netmask/Gateway ...Done! ****" <<std::endl;
                    } else {
                        result = false;
                    }
                } else {
                    result = true;
                }
            }
            
            TYCloseInterface(hIface);
        }
    }
    
    return result;
}


TYCamInterface::TYCamInterface()
{
    TYContext::getInstance();
    Reset();
}

TY_STATUS TYCamInterface::Reset()
{
    TY_STATUS status;
    status = TYUpdateInterfaceList();
    if(status != TY_STATUS_OK) return status;

    uint32_t n = 0;
    status = TYGetInterfaceNumber(&n);
    if(status != TY_STATUS_OK) return status;

    if(n == 0) return TY_STATUS_OK;

    ifaces.resize(n);
    status = TYGetInterfaceList(&ifaces[0], n, &n);
    return status;
}

void TYCamInterface::List(std::vector<std::string>& interfaces) const
{
    for(auto& iter : ifaces) {
        std::cout << iter.id << std::endl;
        interfaces.push_back(iter.id);
    }
}

// FastCamera
FastCamera::FastCamera()
{
}

FastCamera::FastCamera(const char* sn)
{
    open(sn);
}

FastCamera::~FastCamera()
{
    close();
}

TY_STATUS FastCamera::open(const char* sn)
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    
    const char* inf = mIfaceId.empty() ? nullptr : mIfaceId.c_str();
    auto devList = TYContext::getInstance().queryDeviceList(inf);
    
    if (devList->empty()) {
        std::cerr << "Device list is empty!" << std::endl;
        return TY_STATUS_ERROR;
    }

    device = (sn && strlen(sn) != 0) ? 
             devList->getDeviceBySN(sn) : devList->getDevice(0);
             
    if (!device) {
        return TY_STATUS_ERROR;
    }

    return TYGetComponentIDs(device->_handle, &components);
}

TY_STATUS FastCamera::openByIP(const char* ip)
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    
    const char* inf = mIfaceId.empty() ? nullptr : mIfaceId.c_str();
    auto devList = TYContext::getInstance().queryNetDeviceList(inf);
    
    if (devList->empty()) {
        std::cerr << "Network device list is empty!" << std::endl;
        return TY_STATUS_ERROR;
    }

    device = (ip && strlen(ip) != 0) ? 
             devList->getDeviceByIP(ip) : devList->getDevice(0);
             
    if (!device) {
        std::cerr << "Open device failed!" << std::endl;
        return TY_STATUS_ERROR;
    }

    return TYGetComponentIDs(device->_handle, &components);
}

TY_STATUS FastCamera::setIfaceId(const char* inf)
{
    mIfaceId = inf ? inf : "";
    return TY_STATUS_OK;
}

void FastCamera::close()
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    if (isRunning) {
        doStop();
    }
    device.reset();
}

std::shared_ptr<TYFrame> FastCamera::fetchFrames(uint32_t timeout_ms)
{
    TY_FRAME_DATA tyframe;
    TY_STATUS status = TYFetchFrame(handle(), &tyframe, timeout_ms);
    
    if (status != TY_STATUS_OK) {
        std::cerr << "Frame fetch failed: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }
    
    auto frame = std::make_shared<TYFrame>(tyframe);
    TYEnqueueBuffer(handle(), tyframe.userBuffer, tyframe.bufferSize);
    return frame;
}

TY_COMPONENT_ID FastCamera::StreamIdx2CompID(StreamIdx idx)
{
    switch (idx) {
        case StreamIdx::stream_depth:     return TY_COMPONENT_DEPTH_CAM;
        case StreamIdx::stream_color:     return TY_COMPONENT_RGB_CAM;
        case StreamIdx::stream_ir_left:   return TY_COMPONENT_IR_CAM_LEFT;
        case StreamIdx::stream_ir_right:  return TY_COMPONENT_IR_CAM_RIGHT;
        default:                          return 0;
    }
}

bool FastCamera::has_stream(StreamIdx idx) const
{
    return components & StreamIdx2CompID(idx);
}

TY_STATUS FastCamera::stream_enable(StreamIdx idx)
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    return TYEnableComponents(handle(), StreamIdx2CompID(idx));
}

TY_STATUS FastCamera::stream_disable(StreamIdx idx)
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    return TYDisableComponents(handle(), StreamIdx2CompID(idx));
}

TY_STATUS FastCamera::start()
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    
    if (isRunning) {
        std::cerr << "Device is busy!" << std::endl;
        return TY_STATUS_BUSY;
    }

    uint32_t stream_buffer_size = 0;
    TY_STATUS status = TYGetFrameBufferSize(handle(), &stream_buffer_size);
    if (status != TY_STATUS_OK) {
        std::cerr << "Get frame buffer size failed: " << TY_ERROR(status) << std::endl;
        return status;
    }
    
    if (stream_buffer_size == 0) {
        std::cerr << "Frame buffer size is 0" << std::endl;
        return TY_STATUS_DEVICE_ERROR;
    }

    for (auto& buffer : stream_buffer) {
        buffer.resize(stream_buffer_size);
        TYEnqueueBuffer(handle(), buffer.data(), stream_buffer_size);
    }

    status = TYStartCapture(handle());
    if (status != TY_STATUS_OK) {
        std::cerr << "Start capture failed: " << TY_ERROR(status) << std::endl;
        return status;
    }

    isRunning = true;
    return TY_STATUS_OK;
}

TY_STATUS FastCamera::stop()
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    return doStop();
}

TY_STATUS FastCamera::doStop()
{
    if (!isRunning) {
        return TY_STATUS_IDLE;
    }
    
    isRunning = false;
    
    TY_STATUS status = TYStopCapture(handle());
    if (status != TY_STATUS_OK) {
        std::cerr << "Stop capture failed: " << TY_ERROR(status) << std::endl;
    }
    
    TYClearBufferQueue(handle());
    
    for (auto& buffer : stream_buffer) {
        buffer.clear();
        buffer.shrink_to_fit();
    }
    
    return status;
}

std::shared_ptr<TYFrame> FastCamera::tryGetFrames(uint32_t timeout_ms)
{
    std::lock_guard<std::mutex> lock(_dev_lock);
    return fetchFrames(timeout_ms);
}

} // namespace percipio_layer
