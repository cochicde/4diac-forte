
#ifndef TRACE_TRACER_H
#define TRACE_TRACER_H


#include <vector>
#include <string>

class CTracer {
  public: 

    CTracer() = default;
    virtual ~CTracer() = default;

    virtual bool isEnabled() = 0;

    virtual void traceInstanceData(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB) = 0;

    virtual void traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) = 0;

    virtual void traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) = 0;

    virtual void traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) = 0;

    virtual void traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) = 0;

    virtual void traceExternalInputEvent(std::string paTypeName, std::string paInstanceName,
        uint64_t paEventId,
        uint64_t paEventCounter, 
        const std::vector<std::string>& paOutputs = {}) = 0;
};

#endif // TRACE_TRACER_H