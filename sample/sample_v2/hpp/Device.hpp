#pragma once

#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <stdint.h>
#include <algorithm>
#include <iostream>
#include <sstream>

#include "Frame.hpp"

namespace percipio_layer {

class TYDevice;
class DeviceList;
class TYContext;
class TYFrame;
class FastCamera;

inline std::string parseInterfaceID(const std::string& ifaceId)
{
    std::string type_s = ifaceId.substr(0, ifaceId.find('-'));
    
    if (type_s == "eth" || type_s == "wifi") {
        const size_t IdLength = 18 + type_s.length();
        if (ifaceId.length() <= IdLength) {
            return ifaceId;
        }
        
        std::string new_id = ifaceId.substr(0, IdLength);
        std::string ip_s = ifaceId.substr(IdLength);
        
        try {
            uint32_t ip = static_cast<uint32_t>(std::stoul(ip_s, nullptr, 16));
            const uint8_t* ip_arr = reinterpret_cast<const uint8_t*>(&ip);
            
            std::ostringstream oss;
            oss << new_id << " ip:" 
                << static_cast<uint32_t>(ip_arr[0]) << "."
                << static_cast<uint32_t>(ip_arr[1]) << "."
                << static_cast<uint32_t>(ip_arr[2]) << "."
                << static_cast<uint32_t>(ip_arr[3]);
            return oss.str();
        } catch (...) {
            return ifaceId;
        }
    }
    
    return ifaceId;
}

class TYDeviceInfo
{
public:
    ~TYDeviceInfo() = default;
    
    TYDeviceInfo(const TYDeviceInfo&) = delete;
    TYDeviceInfo& operator=(const TYDeviceInfo&) = delete;
    
    TYDeviceInfo(TYDeviceInfo&&) = default;
    TYDeviceInfo& operator=(TYDeviceInfo&&) = default;

    explicit TYDeviceInfo(const TY_DEVICE_BASE_INFO& info);

    friend class TYDevice;
    friend class DeviceList;

    const char* id() const { return _info.id; }
    const TY_INTERFACE_INFO& Interface() const { return _info.iface; }
    
    const char* vendorName() const
    {
        return (strlen(_info.userDefinedName) != 0) ? 
               _info.userDefinedName : _info.vendorName;
    }
    
    const char* modelName() const { return _info.modelName; }
    const char* buildHash() const { return _info.buildHash; }
    const char* configVersion() const { return _info.configVersion; }

    const TY_VERSION_INFO& hardwareVersion() const { return _info.hardwareVersion; }
    const TY_VERSION_INFO& firmwareVersion() const { return _info.firmwareVersion; }

    const char* mac() const;
    const char* ip() const;
    const char* netmask() const;
    const char* gateway() const;
    const char* broadcast() const;
    
private:
    TY_DEVICE_BASE_INFO _info;
};

using EventCallback = std::function<void(void* userdata)>;
using EventPair = std::pair<void*, EventCallback>;

class TYDevice
{
public:
    ~TYDevice();
    TYDevice(const TYDevice&) = delete;
    TYDevice& operator=(const TYDevice&) = delete;
    
    TYDevice(TYDevice&& other) noexcept;
    TYDevice& operator=(TYDevice&& other) noexcept;

    TYDevice(TY_DEV_HANDLE handle, const TY_DEVICE_BASE_INFO& info);

    friend class FastCamera;
    friend class TYStream;
    friend class DeviceList;
    friend class TYPropertyManager;

    std::shared_ptr<TYDeviceInfo> getDeviceInfo() const;
    void registerEventCallback(TY_EVENT eventID, void* data, EventCallback cb);
    
private:
    TY_DEV_HANDLE _handle;
    TY_DEVICE_BASE_INFO _dev_info;

    std::map<TY_EVENT, EventPair> _eventCallbackMap;
    std::function<void(const TY_EVENT_INFO*)> _event_callback;
    
    void onDeviceEventCallback(const TY_EVENT_INFO* event_info);
    
    static void eventCallbackWrapper(TY_EVENT_INFO* event_info, void* userdata);
};

class DeviceList {
public:
    ~DeviceList();
    
    DeviceList(const DeviceList&) = delete;
    DeviceList& operator=(const DeviceList&) = delete;
    
    DeviceList(DeviceList&&) = default;
    DeviceList& operator=(DeviceList&&) = default;

    explicit DeviceList(std::vector<TY_DEVICE_BASE_INFO> devices);

    bool empty() const noexcept { return devs.empty(); }
    size_t devCount() const noexcept { return devs.size(); }

    std::shared_ptr<TYDeviceInfo> getDeviceInfo(size_t idx) const;
    std::shared_ptr<TYDevice> getDevice(size_t idx) const;
    std::shared_ptr<TYDevice> getDeviceBySN(const char* sn) const;
    std::shared_ptr<TYDevice> getDeviceByIP(const char* ip) const;

    friend class TYContext;
    
private:
    std::vector<TY_DEVICE_BASE_INFO> devs;
    mutable std::set<TY_INTERFACE_HANDLE> gifaces;
};

enum class ForceIPStyle : int {
    Dynamic = 0,
    Force = 1,
    Static = 2
};

class TYContext {
public:
    static TYContext& getInstance() {
        static TYContext instance;
        return instance;
    }
    
    TYContext(const TYContext&) = delete;
    TYContext& operator=(const TYContext&) = delete;
    
    std::shared_ptr<DeviceList> queryDeviceList(const char* iface = nullptr) const;
    std::shared_ptr<DeviceList> queryNetDeviceList(const char* iface = nullptr) const;
    
    bool ForceNetDeviceIP(ForceIPStyle style, const std::string& mac, 
                         const std::string& ip, const std::string& mask, 
                         const std::string& gateway) const;

private:
    TYContext();
    ~TYContext();
};

class TYCamInterface
{
public:
    TYCamInterface();
    ~TYCamInterface() = default;
    
    TYCamInterface(const TYCamInterface&) = delete;
    TYCamInterface& operator=(const TYCamInterface&) = delete;
    
    TY_STATUS Reset();
    void List(std::vector<std::string>& interfaces) const;
    
private:
    std::vector<TY_INTERFACE_INFO> ifaces;
};

class FastCamera
{
public:
    enum StreamIdx
    {
        stream_depth = 0x1,
        stream_color = 0x2,
        stream_ir_left = 0x4,
        stream_ir_right = 0x8,
        stream_ir = stream_ir_left
    };
    
    friend class TYFrame;
    
    FastCamera();
    explicit FastCamera(const char* sn);
    ~FastCamera();
    
    FastCamera(const FastCamera&) = delete;
    FastCamera& operator=(const FastCamera&) = delete;
    
    virtual TY_STATUS open(const char* sn);
    TY_STATUS setIfaceId(const char* inf);
    virtual TY_STATUS openByIP(const char* ip);
    virtual bool has_stream(StreamIdx idx) const;
    virtual TY_STATUS stream_enable(StreamIdx idx);
    virtual TY_STATUS stream_disable(StreamIdx idx);

    virtual TY_STATUS start();
    virtual TY_STATUS stop();
    virtual void close();

    std::shared_ptr<TYFrame> tryGetFrames(uint32_t timeout_ms);

    TY_DEV_HANDLE handle() const { return device ? device->_handle : nullptr; }

    void RegisterOfflineEventCallback(EventCallback cb, void* data) { 
        if (device) {
            device->registerEventCallback(TY_EVENT_DEVICE_OFFLINE, data, cb); 
        }
    }
    
private:
    std::string mIfaceId;
    mutable std::mutex _dev_lock;

    TY_COMPONENT_ID components = 0;
    static constexpr size_t BUF_CNT = 3;

    std::atomic<bool> isRunning{false};
    std::shared_ptr<TYFrame> fetchFrames(uint32_t timeout_ms);
    TY_STATUS doStop();

    std::shared_ptr<TYDevice> device;
    std::vector<std::vector<uint8_t>> stream_buffer{BUF_CNT};
    
    static TY_COMPONENT_ID StreamIdx2CompID(StreamIdx idx);
};

}