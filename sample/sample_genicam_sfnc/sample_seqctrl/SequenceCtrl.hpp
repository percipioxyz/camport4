#ifndef TY_SEQUENCER_CONTROLLER_H_
#define TY_SEQUENCER_CONTROLLER_H_

#include "TYApi.h"
#include "TYParameter.h"
#include "TYImageProc.h"
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
class SequencerController {
public:
    SequencerController(TY_DEV_HANDLE hDevice, const std::vector<double>& exposures);
    ~SequencerController();

    TY_STATUS setup();
    TY_STATUS disable();
    bool isEnabled() const;

    int getSeqSetCount() const;
    double getExposure(int seqSet) const;

private:
    TY_STATUS disableSequencerControl();

    TY_STATUS sequencerSetting(int64_t seq_path,
                                       int64_t nextSet, const char* trigger_source,
                                       const char* trigger_activation);

    TY_DEV_HANDLE m_hDevice;
    std::vector<double> m_exposures;
    bool m_originalExposureAuto;
    bool m_exposureAutoSaved;
    bool m_enabled;
};

#endif
