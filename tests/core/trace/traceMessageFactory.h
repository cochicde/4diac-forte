#ifndef TRACE_MESSAGE_FACTORY_H
#define TRACE_MESSAGE_FACTORY_H

#include "trace/EventMessage.h"

class bt_message;
class bt_field;

class MessageFactory {
  public: 
    static EventMessage createMessage(const bt_message* paMessage);
  private:
    static std::unique_ptr<AbstractPayload> createPayload(const std::string& paEventType, const bt_field* paField);

};

#endif // TRACE_MESSAGE_FACTORY_H