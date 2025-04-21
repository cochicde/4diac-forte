#include "core/funcbloc.h"
#include "core/resource.h"
#include "core/cominfra/fbdkasn1layer.h"

void CFunctionBlock::traceOutputEvent(TEventID paEOID, CEventChainExecutionThread *const paECET) {
  if (auto &tracer = getResource()->getTracer(); tracer.isEnabled()) {

    // don't trace events that are not connected since they don't increase the event counter
    if (getEOConUnchecked(static_cast<TPortId>(paEOID))->getDestinationList().size() == 0) {
      return;
    }

    auto size = getFBInterfaceSpec().mNumDOs;
    size_t bufferSizeNeeded = 0;
    for (size_t i = 0; i < size; ++i) {
      bufferSizeNeeded += forte::com_infra::CFBDKASN1ComLayer::getRequiredSerializationSize(*getDO(i));
    }

    std::vector<CStringDictionary::TStringId> mInstanceName;
    getFullQualifiedApplicationInstanceNameId(mInstanceName);

    std::vector<uint8_t> outputs(size);
    auto current = 0;
    for (size_t i = 0; i < size; ++i) {
      if (auto result = forte::com_infra::CFBDKASN1ComLayer::serializeDataPoint(
              &outputs.data()[current], static_cast<int>(bufferSizeNeeded - current), *getDO(i));
          result == -1) {
        DEVLOG_ERROR("Error serialization trace\n");
        break;
      } else {
        current += static_cast<size_t>(result);
      }
    }

    tracer.traceSendOutputEvent(static_cast<uint32_t>(mInstanceName.size()), mInstanceName.data(),
                                static_cast<uint64_t>(paEOID), paECET->mEventCounter,
                                static_cast<uint32_t>(outputs.size()), outputs.data());
  }
}
