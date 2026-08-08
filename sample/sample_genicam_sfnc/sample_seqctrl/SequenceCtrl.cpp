/*
 * Features can set in sequence conctrol are differ, 
 * This sample based on GM465.
 * If you are using other Cameras, feature pass in 
 * and setting needs to be modified
 */
#include "../common/common.hpp"
#include "SequenceCtrl.hpp"

SequencerController::SequencerController(TY_DEV_HANDLE hDevice, const std::vector<double>& exposures)
    : m_hDevice(hDevice)
    , m_exposures(exposures)
    , m_originalExposureAuto(false)
    , m_exposureAutoSaved(false)
    , m_enabled(false)
{
}

SequencerController::~SequencerController()
{
}

int SequencerController::getSeqSetCount() const
{
    return static_cast<int>(m_exposures.size());
}

double SequencerController::getExposure(int seqSet) const
{
    if (seqSet < 0 || seqSet >= static_cast<int>(m_exposures.size())) {
        return 0.0;
    }
    return m_exposures[seqSet];
}

TY_STATUS SequencerController::disableSequencerControl()
{
    TY_STATUS ret;
    LOGD("SequencerControl: Disabling SequencerMode");
    ret = TYEnumSetString(m_hDevice, "SequencerMode", "Off");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerControl: Failed to disable SequencerMode");
        return ret;
    }
    return TY_STATUS_OK;
}

TY_STATUS SequencerController::sequencerSetting(int64_t seq_path,
                                                  int64_t nextSet, const char* trigger_source,
                                                  const char* trigger_activation)
{
    TY_STATUS ret = TYIntegerSetValue(m_hDevice, "SequencerPathSelector", seq_path);
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerSetting: Failed to set SequencerPathSelector");
        return ret;
    }

    ret = TYIntegerSetValue(m_hDevice, "SequencerSetNext", nextSet);
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerSetting: Failed to set SequencerSetNext");
        return ret;
    }

    ret = TYEnumSetString(m_hDevice, "SequencerTriggerSource", trigger_source);
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerSetting: Failed to set SequencerTriggerSource=%s", trigger_source);
        return ret;
    }

    if (trigger_activation) {
        ret = TYEnumSetString(m_hDevice, "SequencerTriggerActivation", trigger_activation);
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerSetting: Failed to set SequencerTriggerActivation=%s", trigger_activation);
            return ret;
        }
    }

    return TY_STATUS_OK;
}

TY_STATUS SequencerController::setup()
{
    if (m_exposures.empty()) {
        LOGE("SequencerController: exposures vector is empty");
        return TY_STATUS_INVALID_PARAMETER;
    }

    TY_STATUS ret;
    bool seqSupported = false;
    ret = TYParamExist(m_hDevice, "SequencerMode", &seqSupported);
    if (ret != TY_STATUS_OK || !seqSupported) {
        LOGE("SequencerController: SequencerMode is not supported on this device!");
        return TY_STATUS_ERROR;
    }

    ret = TYEnumSetString(m_hDevice, "AcquisitionMode", "SingleFrame");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to set AcquisitionMode to SingleFrame");
        return ret;
    }

    ret = TYEnumSetString(m_hDevice, "TriggerSource", "Software");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to set TriggerSource to Software");
        return ret;
    }

    LOGD("SequencerController: Selecting Left source and disabling auto exposure");
    ret = TYEnumSetString(m_hDevice, "SourceSelector", "Left");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to select Left source");
        return ret;
    }

    TY_ACCESS_MODE access;
    ret = TYParamGetAccess(m_hDevice, "ExposureAuto", &access);
    if (ret == TY_STATUS_OK && (access & TY_ACCESS_WRITABLE)) {
        ret = TYBooleanGetValue(m_hDevice, "ExposureAuto", &m_originalExposureAuto);
        if (ret == TY_STATUS_OK) {
            m_originalExposureAuto = true;
            m_exposureAutoSaved = true;
            LOGD("SequencerController: Saved original ExposureAuto=%s",
                 m_originalExposureAuto ? "true" : "false");
        }
        LOGD("SequencerController: Disabling auto exposure");
        ret = TYBooleanSetValue(m_hDevice, "ExposureAuto", false);
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to disable ExposureAuto");
            return ret;
        }
    }

    LOGD("SequencerController: Enabling SequencerConfigurationMode");
    ret = TYEnumSetString(m_hDevice, "SequencerConfigurationMode", "On");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to enable SequencerConfigurationMode");
        return ret;
    }
    LOGD("SequencerController: Enable SequencerConfigurationMode done!");

    int64_t setMax = 0;
    ret = TYIntegerGetMax(m_hDevice, "SequencerSetSelector", &setMax);
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to get SequencerSetSelector max");
        return ret;
    }

    LOGD("SequencerController: read SequencerSetSelector done!");

    int seqCount = static_cast<int>(m_exposures.size());
    if (setMax < seqCount) {
        LOGE("SequencerController: Device supports only %lld sequencer sets, need %d",
             (long long)setMax, seqCount);
        return TY_STATUS_ERROR;
    }

    for (int idx = 0; idx < seqCount; idx++) {
        int nextSet = (idx + 1) % seqCount;

        LOGD("SequencerController: Configuring set %d (exposure=%.0fus, next=%d)",
             idx, m_exposures[idx], nextSet);

        ret = TYIntegerSetValue(m_hDevice, "SequencerSetSelector", idx);
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to set SequencerSetSelector to %d", idx);
            return ret;
        }

        ret = TYCommandExec(m_hDevice, "SequencerSetReset");
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to reset sequencer set %d", idx);
            return ret;
        }

        ret = TYEnumSetString(m_hDevice, "SourceSelector", "Left");
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to select Left source");
            return ret;
        }

        ret = TYFloatSetValue(m_hDevice, "ExposureTime", m_exposures[idx]);
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to set ExposureTime=%.0f for set %d",
                 m_exposures[idx], idx);
            return ret;
        }

        if (idx == 0) {
            //This means sequence Start when StartCapture Called
            ret = sequencerSetting(1, 0, "AcquisitionStart", nullptr);
            if (ret != TY_STATUS_OK) {
                LOGE("SequencerController: Failed to set SequencerSetting for set %d (path 1)", idx);
                return ret;
            }
        }

        ret = sequencerSetting(0, nextSet, "FrameEnd", nullptr);
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to set SequencerSetting for set %d (path 0)", idx);
            return ret;
        }

        ret = TYCommandExec(m_hDevice, "SequencerSetSave");
        if (ret != TY_STATUS_OK) {
            LOGE("SequencerController: Failed to save sequencer set %d", idx);
            return ret;
        }
        LOGD("SequencerController: Set %d saved successfully", idx);
    }
    
    LOGD("SequencerController: Setting SequencerSetStart=0");
    ret = TYIntegerSetValue(m_hDevice, "SequencerSetStart", 0);
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to set SequencerSetStart");
        return ret;
    }

    LOGD("SequencerController: Disabling SequencerConfigurationMode");
    ret = TYEnumSetString(m_hDevice, "SequencerConfigurationMode", "Off");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to disable SequencerConfigurationMode");
        return ret;
    }

    LOGD("SequencerController: Enabling SequencerMode");
    ret = TYEnumSetString(m_hDevice, "SequencerMode", "On");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to enable SequencerMode");
        return ret;
    }

    LOGD("SequencerController: Configuration complete - %d sets cycling with exposures", seqCount);
    for (int i = 0; i < seqCount; i++) {
        LOGD("  set %d: %.0f us", i, m_exposures[i]);
    }

    m_enabled = true;

    return TY_STATUS_OK;
}

TY_STATUS SequencerController::disable()
{
    TY_STATUS ret = disableSequencerControl();
    if (ret != TY_STATUS_OK) {
        return ret;
    }

    if (m_exposureAutoSaved) {
        ret = TYEnumSetString(m_hDevice, "SourceSelector", "Left");
        if (ret != TY_STATUS_OK) {
            LOGW("SequencerController: Failed to select Left source for AEC restore");
            return ret;
        }

        LOGD("SequencerController: Restoring ExposureAuto=%s",
             m_originalExposureAuto ? "true" : "false");
        ret = TYBooleanSetValue(m_hDevice, "ExposureAuto", m_originalExposureAuto);
        if (ret != TY_STATUS_OK) {
            LOGW("SequencerController: Failed to restore ExposureAuto");
            return ret;
        }
        m_exposureAutoSaved = false;
    }

    ret = TYEnumSetString(m_hDevice, "AcquisitionMode", "Continuous");
    if (ret != TY_STATUS_OK) {
        LOGE("SequencerController: Failed to set AcquisitionMode to Continuous");
        return ret;
    }
    return TY_STATUS_OK;
}

bool SequencerController::isEnabled() const
{
    return m_enabled;
}
