
#include "deviceReplayer.h"

#include "core/device.h"
#include "core/ecetFake.h"

#include <thread>

CDeviceReplayer::CResourceInformation::CResourceInformation(CResource& paResource, const std::vector<EventMessage>& paExternalEvents) 
        : mResourceReplayer(paResource, paExternalEvents), 
          mResource{paResource},
          mEcet{*dynamic_cast<CFakeEventExecutionThread*>(mResource.getResourceEventExecution())}{
}

CDeviceReplayer::CDeviceReplayer(CDevice& paDevice, const std::unordered_map<std::string, std::vector<EventMessage>>& paExternalEvents) : mDevice{paDevice} {
  for(auto child : mDevice.getChildren()){
    auto resource = static_cast<CResource*>(child); // the first generation of children under the device are always resources
    if(std::string resourceStringName = resource->getInstanceName(); 
      paExternalEvents.find(resourceStringName) != paExternalEvents.end()){

      mResourceInformations.emplace_back( 
            *resource, 
            paExternalEvents.at(resourceStringName));
    }
  }
}

std::unordered_map<std::string, std::vector<EventMessage>> CDeviceReplayer::reproduceAll(){

  std::unordered_map<std::string, std::vector<EventMessage>> generatedMessages;

  for(auto& resourceInformation : mResourceInformations){
    generatedMessages.insert({resourceInformation.mResource.getInstanceName(), 
              resourceInformation.mResourceReplayer.reproduceAll()});
  }

  return generatedMessages;
}

std::optional<TEventEntry> CDeviceReplayer::reproduceNextEvent(const std::string& paResourceName) {
  for(auto& resourceInformation : mResourceInformations){
    if(resourceInformation.mResource.getInstanceName() == paResourceName){
      return resourceInformation.mResourceReplayer.reproduceNextEvent();
    }
  }
  return std::nullopt;
}

std::unordered_map<std::string, std::vector<EventMessage>> CDeviceReplayer::getGeneratedEvents(){
  std::unordered_map<std::string, std::vector<EventMessage>> generatedMessages;
  for(auto& resourceInformation : mResourceInformations){
      generatedMessages.insert({resourceInformation.mResource.getInstanceName(), 
                resourceInformation.mResourceReplayer.getGeneratedEvents()});
  }

  return generatedMessages;
}
