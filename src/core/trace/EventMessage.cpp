#include "EventMessage.h"

#include <array>
#include <iomanip>


// ******************** //
// EventMessage methods //
// ******************** //

EventMessage::EventMessage(std::string paEventType, std::unique_ptr<AbstractPayload> paPayload, int64_t paTimestamp) : 
  mEventType(std::move(paEventType)), mPayload(std::move(paPayload)), mTimestamp(paTimestamp){
}

EventMessage::EventMessage(const EventMessage& paOther) {
  this->mEventType = paOther.mEventType;
  this->mTimestamp = paOther.mTimestamp;
  this->mPayload = paOther.mPayload->clone();
}

EventMessage& EventMessage::operator=(const EventMessage& paOther) {
  this->mEventType = paOther.mEventType;
  this->mTimestamp = paOther.mTimestamp;
  this->mPayload = paOther.mPayload->clone();
  return *this;
}

std::string EventMessage::getPayloadString() const {
  return mEventType + (mPayload != nullptr ? ": " + mPayload->getString() : "");
}

std::string EventMessage::getTimestampString() const {
  auto nanoseconds = mTimestamp;
  auto milliseconds = mTimestamp / 1000000;
  
  auto seconds = milliseconds / 1000;
  auto minutes = seconds / 60;
  auto hours = minutes / 60;

  nanoseconds %= 1000000000;
  seconds %= 60;
  minutes %= 60;

  std::stringstream ss;
  ss << "[" << 
      std::setw(2) << std::setfill('0') << hours + 1 << ":" <<
      std::setw(2) << std::setfill('0') << minutes << ":" <<
      std::setw(2) << std::setfill('0') << seconds << "." <<
      std::setw(6) << std::setfill('0') << nanoseconds <<  "]";

  return ss.str();
}

bool EventMessage::operator==(const EventMessage& paOther) const {
  return mEventType == paOther.mEventType  && 
    (mPayload == nullptr) ? (paOther.mPayload == nullptr) : (paOther.mPayload != nullptr && *mPayload == *paOther.mPayload);
}

std::string EventMessage::getEventType() const {
  return mEventType;
}

// *********************** //
// AbstractPayload Methods //
// *********************** //
AbstractPayload::AbstractPayload(std::string paTypeName, std::string paInstanceName) 
  : mTypeName(std::move(paTypeName)), mInstanceName(std::move(paInstanceName))  {
}

std::string AbstractPayload::getString() const {
  return "{ typeName = \"" + mTypeName + "\", instanceName = \"" + mInstanceName + "\"" + specificPayloadString() + " }";
}

bool AbstractPayload::operator==(const AbstractPayload& paOther) const {
  return mTypeName == paOther.mTypeName && mInstanceName == paOther.mInstanceName && specificPayloadEqual(paOther);
}

// ************** //
// FBEventPayload //
// ************** //
FBEventPayload::FBEventPayload(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) : 
  AbstractPayload(std::move(paTypeName), std::move(paInstanceName)),
  mEventId(paEventId) {
}

std::unique_ptr<AbstractPayload> FBEventPayload::clone() {
  return std::make_unique<FBEventPayload>(*this);
}

std::string FBEventPayload::specificPayloadString() const {
  return ", eventId = " + std::to_string(mEventId);
}

bool FBEventPayload::specificPayloadEqual(const AbstractPayload& paOther) const {
  const auto& childInstance = dynamic_cast<const FBEventPayload&>(paOther); 
  return mEventId == childInstance.mEventId;
}

// ************* //
// FBDataPayload //
// ************* //
FBDataPayload::FBDataPayload(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) :
  AbstractPayload(std::move(paTypeName), std::move(paInstanceName)),
  mDataId(paDataId), mValue(paValue){
}

std::unique_ptr<AbstractPayload> FBDataPayload::clone() {
  return std::make_unique<FBDataPayload>(*this);
}

std::string FBDataPayload::specificPayloadString() const {
  auto convertedString = mValue;

  // add backslash to quoutes and double quotes
  for(auto toReplace : {"\"", "\'"}){
    for(auto pos = convertedString.find(toReplace); pos != std::string::npos; pos = convertedString.find(toReplace, pos + 2)){
      convertedString.insert(pos, "\\");
    }
  }
  return ", dataId = " + std::to_string(mDataId) + ", value = \"" + convertedString + "\"";
}

bool FBDataPayload::specificPayloadEqual(const AbstractPayload& paOther) const {
  const auto& childInstance = dynamic_cast<const FBDataPayload&>(paOther); 
  return mDataId == childInstance.mDataId && mValue == childInstance.mValue;
}

// ********************* //
// FBInstanceDataPayload //
// ********************* //
FBInstanceDataPayload::FBInstanceDataPayload(std::string paTypeName, std::string paInstanceName, const std::vector<std::string>& paInputs,
    const std::vector<std::string>& paOutputs, const std::vector<std::string>& paInternal, const std::vector<std::string>& paInternalFB) : 
      AbstractPayload(std::move(paTypeName), std::move(paInstanceName)),
      mInputs(paInputs), mOutputs(paOutputs), mInternal(paInternal), mInternalFB(paInternalFB)
{
}

std::unique_ptr<AbstractPayload> FBInstanceDataPayload::clone() {
  return std::make_unique<FBInstanceDataPayload>(*this);
}

std::string FBInstanceDataPayload::specificPayloadString() const {

  auto createStringFromVector = [](const std::vector<std::string>& vec) -> std::string {
    std::string result = "[";
    for(size_t i = 0; i < vec.size(); i++) {
      if(i != 0) {
        result += ",";
      }
      result += " [" + std::to_string(i) + "] = \"" + vec[i] + "\""; 
    }
    result += " ]";
    return result;
  };

  return ", _inputs_len = " + std::to_string(mInputs.size()) + ", inputs = " + createStringFromVector(mInputs) +
    ", _outputs_len = " + std::to_string(mOutputs.size()) + ", outputs = " + createStringFromVector(mOutputs) +
    ", _internal_len = " + std::to_string(mInternal.size()) + ", internal = " + createStringFromVector(mInternal) +
    ", _internalFB_len = " + std::to_string(mInternalFB.size()) + ", internalFB = " + createStringFromVector(mInternalFB);
}

bool FBInstanceDataPayload::specificPayloadEqual(const AbstractPayload& paOther) const {
  const auto& childInstance = dynamic_cast<const FBInstanceDataPayload&>(paOther);
  if(mInputs.size() > 0){
    std::string a = mInputs[0].data();
    auto b = a;
    b = "test";
  } 
  return mInputs == childInstance.mInputs 
        && mOutputs == childInstance.mOutputs 
        && mInternal == childInstance.mInternal 
        && mInternalFB == childInstance.mInternalFB;
}

// ********************* //
// FBExternalEventPayload //
// ********************* //
FBExternalEventPayload::FBExternalEventPayload(std::string paTypeName, std::string paInstanceName, uint64_t paEventId, uint64_t paEventCounter,
    const std::vector<std::string>& paOutputs) : 
      AbstractPayload(std::move(paTypeName), std::move(paInstanceName)),
      mEventId(paEventId), mEventCounter(paEventCounter), mOutputs(paOutputs)
{
}

std::unique_ptr<AbstractPayload> FBExternalEventPayload::clone() {
  return std::make_unique<FBExternalEventPayload>(*this);
}

std::string FBExternalEventPayload::specificPayloadString() const {

  auto createStringFromVector = [](const std::vector<std::string>& vec) -> std::string {
    std::string result = "[";
    for(size_t i = 0; i < vec.size(); i++) {
      if(i != 0) {
        result += ",";
      }
      result += " [" + std::to_string(i) + "] = \"" + vec[i] + "\""; 
    }
    result += " ]";
    return result;
  };

  return ", eventId = " + std::to_string(mEventId) + " eventCounter = " + std::to_string(mEventCounter) + 
    ", _outputs_len = " + std::to_string(mOutputs.size()) + ", outputs = " + createStringFromVector(mOutputs);
}

bool FBExternalEventPayload::specificPayloadEqual(const AbstractPayload& paOther) const {
  const auto& childInstance = dynamic_cast<const FBExternalEventPayload&>(paOther); 
  return mEventId == childInstance.mEventId && mEventCounter == childInstance.mEventCounter 
        && mOutputs == childInstance.mOutputs; 
}
