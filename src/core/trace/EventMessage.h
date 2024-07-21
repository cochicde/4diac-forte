#ifndef _FORTE_TESTS_CORE_TRACE_EVENT_MESSAGE_H_
#define _FORTE_TESTS_CORE_TRACE_EVENT_MESSAGE_H_

#include <memory>
#include <string>
#include <vector>

#include <iostream>

class AbstractPayload;

class EventMessage {
public:
  EventMessage(std::string paEventType, std::unique_ptr<AbstractPayload> paPayload, int64_t paTimestamp);

  virtual ~EventMessage() = default;

  EventMessage(const EventMessage& paOther);
  EventMessage& operator=(const EventMessage& paOther);

  EventMessage(EventMessage&& paOther) = default;
  EventMessage& operator=(EventMessage&& paOther) = default;

  std::string getPayloadString() const;

  std::string getTimestampString() const;

  int64_t getTimestamp () const { return mTimestamp;}

  bool operator==(const EventMessage& paOther) const;

  template<typename T>
  std::unique_ptr<T> getPayload() const;

  std::string getTypeName() const;

  std::string getEventType() const;
private:
  std::string mEventType;
  std::unique_ptr<AbstractPayload> mPayload;
  int64_t mTimestamp;
};

class AbstractPayload {
public:
  AbstractPayload(std::string paTypeName, std::string paInstanceName);

  virtual ~AbstractPayload() = default;

  AbstractPayload(const AbstractPayload&) = default;
  AbstractPayload& operator=(const AbstractPayload&) = default;

  AbstractPayload(AbstractPayload&) = default;
  AbstractPayload& operator=(AbstractPayload&&) = default;


  std::string getString() const;

  virtual std::unique_ptr<AbstractPayload> clone() const = 0;

  bool operator==(const AbstractPayload& paOther) const;


// protected:
  virtual bool specificPayloadEqual(const AbstractPayload& paOther) const = 0;

  virtual std::string specificPayloadString() const = 0;

  std::string mTypeName;
  std::string mInstanceName;
};

class FBInputEventPayload : public AbstractPayload {
public:
  FBInputEventPayload(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId);

private:

  std::unique_ptr<AbstractPayload> clone() const override;

  std::string specificPayloadString() const override;

  bool specificPayloadEqual(const AbstractPayload& paOther) const override;
 
  uint64_t mEventId;
};

class FBOutputEventPayload : public AbstractPayload {
public:
  FBOutputEventPayload(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId, uint64_t paEventCounter, const std::vector<std::string>& paOutputs);

  std::unique_ptr<AbstractPayload> clone() const override;

  std::string specificPayloadString() const override;

  bool specificPayloadEqual(const AbstractPayload& paOther) const override;
 
  uint64_t mEventId;
  uint64_t mEventCounter;
  std::vector<std::string> mOutputs;
};

class FBDataPayload : public AbstractPayload {
public:
  FBDataPayload(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue);

private:
  std::unique_ptr<AbstractPayload> clone() const override;

  std::string specificPayloadString() const override;

  bool specificPayloadEqual(const AbstractPayload& paOther) const override;

  uint64_t mDataId;
  std::string mValue;
};

class FBInstanceDataPayload : public AbstractPayload {
public:
  FBInstanceDataPayload(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB);

private:

  std::unique_ptr<AbstractPayload> clone() const override;

  std::string specificPayloadString() const override;

  bool specificPayloadEqual(const AbstractPayload& paOther) const override;

  std::vector<std::string> mInputs;
  std::vector<std::string> mOutputs;
  std::vector<std::string> mInternal;
  std::vector<std::string> mInternalFB;
};

template<typename T>
std::unique_ptr<T> EventMessage::getPayload() const {
  return std::unique_ptr<T>(dynamic_cast<T*>(mPayload->clone().release()));
}

#endif //  _FORTE_TESTS_CORE_TRACE_EVENT_MESSAGE_H_