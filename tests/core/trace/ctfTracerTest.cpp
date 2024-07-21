/*******************************************************************************
 * Copyright (c) 2019 fotiss GmbH
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Jose Cabral- initial API and implementation and/or initial documentation
 *******************************************************************************/
#include <iomanip>
#include <optional>
#include <thread>

#include <babeltrace2/babeltrace.h>
#include <boost/test/unit_test.hpp>

#include "../stdfblib/ita/EMB_RES.h"
#include "config.h"
#include "ctfTracerTest_gen.cpp"
#include "device.h"
#include "ecetFactory.h"
#include "timerHandlerFactory.h"
#include "trace/EventMessage.h"
#include "trace/barectf_platform_forte.h"
#include "../fbtests/fbtesterglobalfixture.h"
#include "utils/parameterParser.h"
#include "trace/internalTracer.h"

#include "cfb.h"
#include "basicfb.h"

#include "trace/manualEcet.h"

#include <set>
#include <functional>

/**
 * @brief create a FB network with a E_SWITCH and E_CTU  with some connections 
 * 
 * @param paResourceName name for the resource where the network is located
 * @param paDeviceName name for the device
 * 
 * @return the created device que the network of FB in it
*/
std::unique_ptr<CDevice> createExampleDevice(CStringDictionary::TStringId paResourceName, CStringDictionary::TStringId paDeviceName = g_nStringIdMyDevice);

std::unique_ptr<CDevice> createNonDeterministicExample(CStringDictionary::TStringId paResourceName1, CStringDictionary::TStringId paResourceName2, CStringDictionary::TStringId paDeviceName = g_nStringIdMyDevice);


std::ostream& operator<<(std::ostream &paOs, const EventMessage &paEventMessage) {
  paOs << paEventMessage.getPayloadString();
  return paOs;
}


/**
 * @brief Get the list of message from a directory containing CTF traces
 * 
 * @param path Directory containing ctf traces and, if needed, the metadata associated to them
 * @return sorted list of events found in path
 */
std::unordered_map<std::string, std::vector<EventMessage>> getEventMessages(std::string path);

std::unordered_map<std::string, std::vector<EventMessage>> getNeededEvents(const std::unordered_map<std::string, std::vector<EventMessage>>& paEvents, std::function<bool(CStringDictionary::TStringId)> paIsNeeded);

CResource* getResource(CDevice* paDevice, CStringDictionary::TStringId paResourceName);

/**
 * @brief Print messages as babeltrace2 pretty print
 * @param messages List of messages to print
 */
void printPrettyMessages(const std::vector<EventMessage>& messages);

void prepareTraceTest(std::string paDestMetadata);

void checkMessages(std::unordered_map<std::string, std::vector<EventMessage>>& paExpected, std::unordered_map<std::string, std::vector<EventMessage>>& paActual){
  for(auto& [resource, expectedMessages] : paExpected){
    auto& actualMessages = paActual[resource];

    BOOST_TEST_INFO("Resource: " + resource + " Expected vs traced: Same size ");
    BOOST_CHECK_EQUAL(expectedMessages.size(), actualMessages.size());

      // although vectors can be check directly, this granularity helps debugging in case some message is different
    for(size_t i = 0; i < std::min(expectedMessages.size(), actualMessages.size()); i++ ){
      BOOST_TEST_INFO("Resource: " + resource + " Expected event number " + std::to_string(i));
      BOOST_CHECK_EQUAL(expectedMessages[i], actualMessages[i]);
    }

    // add extra event to check that the comparison fails
    expectedMessages.emplace_back("sendOutputEvent", std::make_unique<FBInputEventPayload>("E_RESTART", "START", 2),0);
    BOOST_CHECK(expectedMessages != actualMessages);

    // remove the recently added message in case is needed again later
    expectedMessages.pop_back();
  }
}

std::string getResourceNameFromTraceOutputPort(const bt_port_output*	paPort)	
{
  auto outputPortBase = bt_port_output_as_port_const(paPort);
  auto portName = bt_port_get_name(outputPortBase);

  // Port name has the following pattern: TRACE-ID | STREAM-CLASS-ID | STREAM-ID
  // where STREAM-ID contains the absolut path to the trace file. 
  // The file name is given in the CTF tracer inside forte as 
  // "trace_"  + INSTANCE_NAME + "_" + DATE + ".ctf"),

  CParameterParser portNameParser(portName, '|', 3);
  portNameParser.parseParameters();
  CParameterParser fileNameParser(std::filesystem::path(portNameParser[2]).filename().c_str(), '_', 4);
  fileNameParser.parseParameters();

  return fileNameParser[1];
}

BOOST_AUTO_TEST_SUITE (tracer_test)

#include "forte_dint.h"
#include "forte_bool.h"
#include "forte_string.h"

BOOST_AUTO_TEST_CASE(events_test) {
  
  CIEC_BOOL var1(false);
  CIEC_DINT var2(98);
  CIEC_STRING var3(std::string("1"));

  CIEC_ANY* values[] = {&var1, &var2, &var3};
  std::vector<std::string> outputs(3);
  outputs.reserve(3);

  for(size_t i = 0; i < outputs.size(); ++i) {
    CIEC_ANY *value = values[i];
    auto size = value->getToStringBufferSize();
    char buffer[size];
    buffer[value->toString(buffer, size)] = '\0';
    outputs[i].assign(buffer);
  }

  CFlexibelTracer::setTracer("SomeOtherThing");

  std::vector<EventMessage> expectedGeneratedMessagesResource1;
  std::vector<EventMessage> expectedGeneratedMessagesResource2;

  std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&> resourceToMessagesMap;
  resourceToMessagesMap.insert({g_nStringIdE_CTU, expectedGeneratedMessagesResource1});
  resourceToMessagesMap.insert({g_nStringIdMyDevice, expectedGeneratedMessagesResource2});

  CInternalTracer::setResourceOutputMap(resourceToMessagesMap); 

  CFlexibelTracer tracer1(g_nStringIdE_CTU, 10);
  CFlexibelTracer tracer2(g_nStringIdMyDevice, 10);

  auto traceMe = [&outputs](CFlexibelTracer& tracer){
      tracer.traceReceiveInputEvent("E_RESTART", "START", 65534);
      tracer.traceSendOutputEvent("E_RESTART", "START", 0, 1, outputs);
      tracer.traceReceiveInputEvent("E_CTU", "Counter", 0);
      tracer.traceInstanceData("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"FALSE", "0"}, std::vector<std::string>{}, std::vector<std::string>{});
      tracer.traceSendOutputEvent("E_CTU", "Counter", 0, 1, outputs);
      tracer.traceOutputData("E_CTU", "Counter", 0, "TRUE");
      tracer.traceOutputData("E_CTU", "Counter", 1, "1");
      tracer.traceReceiveInputEvent("E_SWITCH", "Switch", 0);
      tracer.traceInstanceData("E_SWITCH", "Switch", std::vector<std::string>{"FALSE"}, std::vector<std::string>{}, std::vector<std::string>{}, std::vector<std::string>{});
      tracer.traceInputData("E_SWITCH", "Switch", 0, "TRUE");
      tracer.traceSendOutputEvent("E_SWITCH", "Switch", 0, 1, std::vector<std::string>{});
      tracer.traceReceiveInputEvent("E_CTU", "Counter", 1);
      tracer.traceInstanceData("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"TRUE", "1"}, std::vector<std::string>{}, std::vector<std::string>{});
      tracer.traceSendOutputEvent("E_CTU", "Counter", 0, 1, std::vector<std::string>{});
      tracer.traceOutputData("E_CTU", "Counter", 0, "FALSE");
      tracer.traceOutputData("E_CTU", "Counter", 1, "0");
      tracer.traceReceiveInputEvent("E_RESTART", "START", 65534);
      tracer.traceSendOutputEvent("E_RESTART", "START", 0, 1, outputs);
  };

  for(auto i = 0; i < 100; i++ ){
     traceMe(tracer1);
      traceMe(tracer2);
  }

  printPrettyMessages(expectedGeneratedMessagesResource1);

  auto messages1 = expectedGeneratedMessagesResource1;

  printPrettyMessages(messages1);

  auto messages2 = std::move(expectedGeneratedMessagesResource1);

  printPrettyMessages(messages2);


}

BOOST_AUTO_TEST_CASE(sequential_events_test) {
  prepareTraceTest("metadata");

  auto resourceName = g_nStringIdMyResource;
  auto deviceName = g_nStringIdMyDevice;

  // The inner scope is to make sure the destructors of the resources are 
  // called which flushes the output
  {
    auto device = createExampleDevice(resourceName, deviceName);

    auto resource = getResource(device.get(), resourceName);
    
    device->startDevice();
    // wait for all events to be triggered
    while(resource->getResourceEventExecution()->isProcessingEvents()){
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    device->changeFBExecutionState(EMGMCommandType::Stop);
    resource->getResourceEventExecution()->joinEventChainExecutionThread();
  }

  // disable logging 
  BarectfPlatformFORTE::setup("");

  std::unordered_map<std::string, std::vector<EventMessage>>  allMessages;
  allMessages[CStringDictionary::getInstance().get(g_nStringIdMyResource)] = {};
  allMessages[CStringDictionary::getInstance().get(g_nStringIdMyDevice)] = {};

  auto& resourceMessages = allMessages[CStringDictionary::getInstance().get(g_nStringIdMyResource)];
  // message for the device is empty

  // timestamp cannot properly be tested, so setting everythin to zero
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_RESTART", "START", 65534),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 0, 1, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 0),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"FALSE", "0"}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 0, 1, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "TRUE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "1"), 0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_SWITCH", "Switch", 0),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_SWITCH", "Switch", std::vector<std::string>{"FALSE"}, std::vector<std::string>{}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("inputData", std::make_unique<FBDataPayload>("E_SWITCH", "Switch", 0, "TRUE"), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_SWITCH", "Switch", 1, 1, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 1),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"TRUE", "1"}, std::vector<std::string>{}, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 1, 1, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "FALSE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "0"), 0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_RESTART", "START", 65534),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 2, 1, std::vector<std::string>{}),0);

  auto ctfMessages = getEventMessages(CTF_OUTPUT_DIR);

  checkMessages(ctfMessages, allMessages);
}

BOOST_AUTO_TEST_CASE(non_deterministic_events_test) {
  // prepareTraceTest("metadata");

  auto resource1Name = g_nStringIdMyResource;
  auto resource2Name = g_nStringIdMyResource2;
  auto deviceName = g_nStringIdMyDevice;

  // The inner scope is to make sure the destructors of the resources are 
  // called which flushes the output
  // {

  //   auto device = createNonDeterministicExample(resource1Name, resource2Name, deviceName); 
    
  //   auto resource1 = getResource(device.get(), resource1Name);
  //   auto resource2 = getResource(device.get(), resource2Name);

  //   device->startDevice();
  //   // wait for all events to be triggered

  //   // TODO: Let it run for a random amount of time to make it more realistic
  //   std::this_thread::sleep_for(std::chrono::milliseconds(60000));

  //   device->changeFBExecutionState(EMGMCommandType::Kill);
  //   resource1->getResourceEventExecution()->joinEventChainExecutionThread();
  //   resource2->getResourceEventExecution()->joinEventChainExecutionThread();
  // }

  // // disable logging 
  // BarectfPlatformFORTE::setup("");

  auto allTracedEvents = getEventMessages(CTF_OUTPUT_DIR);

  {
    CEcetFactory::setEcetNameToCreate("manual");
    CTimerHandlerFactory::setTimeHandlerNameToCreate("fakeTimer");
    CFlexibelTracer::setTracer("SomeOtherThing");

    std::vector<EventMessage> expectedGeneratedMessagesResource1;
    std::vector<EventMessage> expectedGeneratedMessagesResource2;
    std::vector<EventMessage> expectedGeneratedMessagesDevice;

    std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&> resourceToMessagesMap;
    resourceToMessagesMap.insert({resource1Name,  expectedGeneratedMessagesResource1});
    resourceToMessagesMap.insert({resource2Name,  expectedGeneratedMessagesResource2});
    resourceToMessagesMap.insert({deviceName,  expectedGeneratedMessagesDevice});
    CInternalTracer::setResourceOutputMap(resourceToMessagesMap); 

    auto device = createNonDeterministicExample(resource1Name, resource2Name, deviceName);

    std::set<CStringDictionary::TStringId> validTypes;

    for(const auto& resource : device->getFBList()){
      if(auto container = dynamic_cast<forte::core::CFBContainer*>(resource); container != nullptr){
        for(const auto& fb : container->getFBList()){
          // all service FBs
          if(dynamic_cast<CCompositeFB*>(fb) == nullptr && dynamic_cast<CBasicFB*>(fb) == nullptr){
            validTypes.insert(fb->getFBTypeId());
         }
       }
      }
    }

    CManualEventExecutionThread::smValidTypes = validTypes;

    auto isValidType = [&validTypes](CStringDictionary::TStringId paType){
      return validTypes.find(paType) != validTypes.end();
    };

    auto allTracedExternalEvents = getNeededEvents(allTracedEvents, isValidType);

    class helper {
      public:
      helper(CResource* paResource, const std::vector<EventMessage>& paEvents) : resource{paResource}, 
                    ecet{dynamic_cast<CManualEventExecutionThread*>(resource->getResourceEventExecution())},
                    mEvents{paEvents} {}
      CResource* resource;
      CManualEventExecutionThread* ecet;
      const std::vector<EventMessage>& mEvents;
    };

    auto resourceNames = std::vector<CStringDictionary::TStringId>({resource1Name, resource2Name});
    
    std::vector<helper> resourceHelpers;

    for(const auto resourceName : resourceNames){
      resourceHelpers.emplace_back(getResource(device.get(), resourceName), allTracedExternalEvents[CStringDictionary::getInstance().get(resourceName)]);
    }

    //  for(auto& resourceHelper : resourceHelpers){
    //   // we allow the starting event sent to RESTART so it's traced
    //   auto allowOneEvent = [&resourceHelper, counter = 0](TEventEntry paEventEntry) mutable {
    //     if(counter++ == 0){
    //       resourceHelper.ecet->insertFront(paEventEntry);
    //     }
    //   }; 
    //   resourceHelper.ecet->allowInternallyGeneratedEventChains(true, allowOneEvent);
    // }

    device->startDevice();


    for(auto& helper : resourceHelpers){
    //  helper.ecet->allowInternallyGeneratedEventChains(false);
      // uint64_t previousEventCounter = 0;
      for(const auto& externalEvent : helper.mEvents){
        auto payload = externalEvent.getPayload<FBOutputEventPayload>();
        
        forte::core::TNameIdentifier id;
        id.pushBack(CStringDictionary::getInstance().getId(payload->mInstanceName.c_str()));
        forte::core::TNameIdentifier::CIterator anotherIterator(id.begin());
        auto fb = helper.resource->getContainedFB(anotherIterator);

        helper.ecet->triggerEventOnCounter(CConnectionPoint(fb, payload->mEventId), payload->mEventCounter, payload->mOutputs);
 
      }
      auto releaseEcet = [helper](TEventEntry){
        helper.ecet->removeControllFromOutside();
      };
      helper.ecet->allowInternallyGeneratedEventChains(true, releaseEcet);
      helper.ecet->removeControllFromOutside();
    }

    device->changeFBExecutionState(EMGMCommandType::Kill);

    for(auto& helper : resourceHelpers){
      helper.resource->getResourceEventExecution()->joinEventChainExecutionThread();
    }

    std::unordered_map<std::string, std::vector<EventMessage>> expectedMessages;

    expectedMessages.insert({CStringDictionary::getInstance().get(resource1Name), std::move(expectedGeneratedMessagesResource1)});
    expectedMessages.insert({CStringDictionary::getInstance().get(resource2Name), std::move(expectedGeneratedMessagesResource2)});
    expectedMessages.insert({CStringDictionary::getInstance().get(deviceName), std::move(expectedGeneratedMessagesDevice)});

    checkMessages(allTracedEvents, expectedMessages);
  }
}

BOOST_AUTO_TEST_SUITE_END()

std::unique_ptr<CDevice> createNonDeterministicExample(CStringDictionary::TStringId paResourceName1, CStringDictionary::TStringId paResourceName2, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);

  // resource 1
  {
    auto resource = new EMB_RES(paResourceName1, *device);
    device->addFB(resource);
    resource->initialize();

    auto cycleName = g_nStringIdE_CYCLE;
    auto ctuName = g_nStringIdE_CTU;
    auto publishName = g_nStringIdPUBLISH_1;

    BOOST_TEST_INFO(CStringDictionary::getInstance().get(paResourceName1));

    BOOST_TEST_INFO("Create FB Cycle");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(cycleName, g_nStringIdE_CYCLE, *resource)));
      
    BOOST_TEST_INFO("Create FB CTU");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(ctuName, g_nStringIdE_CTU, *resource)));

    BOOST_TEST_INFO("Create FB Publish");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(publishName, g_nStringIdPUBLISH_1, *resource)));

    forte::core::SManagementCMD command;
    command.mCMD = EMGMCommandType::CreateConnection;
    command.mDestination = CStringDictionary::scmInvalidStringId;

    // Events
    BOOST_TEST_INFO("Event connection: Start.COLD -> PUBLISH.INIT");
    command.mFirstParam.pushBack(g_nStringIdSTART);
    command.mFirstParam.pushBack(g_nStringIdCOLD);
    command.mSecondParam.pushBack(publishName);
    command.mSecondParam.pushBack(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Publish.INITO -> Cycle.START");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(publishName);
    command.mFirstParam.pushBack(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(cycleName);
    command.mSecondParam.pushBack(g_nStringIdSTART);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Cycle.EO -> CTU.CU");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(cycleName);
    command.mFirstParam.pushBack(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(ctuName);
    command.mSecondParam.pushBack(g_nStringIdCU);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: CTU.CUO -> Publish.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdCUO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(publishName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Data
    BOOST_TEST_INFO("Event connection: CTU.CV -> Publish.SD_1");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdCV);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(publishName);
    command.mSecondParam.pushBack(g_nStringIdSD_1);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Literals
    command.mCMD = EMGMCommandType::Write;

    BOOST_TEST_INFO("Literal: Cycle.DT -> T#200ms");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(cycleName);
    command.mFirstParam.pushBack(g_nStringIdDT);
    command.mAdditionalParams = CIEC_STRING("T#200ms");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdPV);
    command.mAdditionalParams = CIEC_STRING("0");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(publishName);
    command.mFirstParam.pushBack(g_nStringIdQI);
    command.mAdditionalParams = CIEC_STRING("TRUE");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(publishName);
    command.mFirstParam.pushBack(g_nStringIdID);
    command.mAdditionalParams = CIEC_STRING("239.0.0.1:61000");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

  }

  // resource 2
  {
    auto resource = new EMB_RES(paResourceName2, *device);

    device->addFB(resource);
    resource->initialize();

    auto cycleName = g_nStringIdE_CYCLE;
    auto ctuName = g_nStringIdE_CTU;
    auto subscribeName = g_nStringIdSUBSCRIBE_1;
    auto addName = g_nStringIdADD;
    auto mulName = g_nStringIdMUL;
    auto uint2uintFirst = g_nStringIdUINT2UINT;
    auto uint2uintSecond = g_nStringIdUINT2UINT_1;
    auto uint2uintThird = g_nStringIdUINT2UINT_2;

    BOOST_TEST_INFO(CStringDictionary::getInstance().get(paResourceName2));

    BOOST_TEST_INFO("Create FB Subscribe");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(subscribeName, g_nStringIdSUBSCRIBE_1, *resource)));

    BOOST_TEST_INFO("Create FB Cycle");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(cycleName, g_nStringIdE_CYCLE, *resource)));
      
    BOOST_TEST_INFO("Create FB CTU");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(ctuName, g_nStringIdE_CTU, *resource)));

    BOOST_TEST_INFO("Create FB ADD");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(addName, g_nStringIdF_ADD, *resource)));

    BOOST_TEST_INFO("Create FB MUL");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(mulName, g_nStringIdF_MUL, *resource)));

    BOOST_TEST_INFO("Create FB UINT2UINT 1");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(uint2uintFirst, g_nStringIdUINT2UINT, *resource)));

    BOOST_TEST_INFO("Create FB UINT2UINT 2");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(uint2uintSecond, g_nStringIdUINT2UINT, *resource)));

    BOOST_TEST_INFO("Create FB UINT2UINT 3");
    BOOST_ASSERT(EMGMResponse::Ready == resource->addFB(CTypeLib::createFB(uint2uintThird, g_nStringIdUINT2UINT, *resource)));

    forte::core::SManagementCMD command;
    command.mCMD = EMGMCommandType::CreateConnection;
    command.mDestination = CStringDictionary::scmInvalidStringId;

    // Events
    BOOST_TEST_INFO("Event connection: Start.COLD -> SUBSCRIBE.INIT");
    command.mFirstParam.pushBack(g_nStringIdSTART);
    command.mFirstParam.pushBack(g_nStringIdCOLD);
    command.mSecondParam.pushBack(subscribeName);
    command.mSecondParam.pushBack(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.INIT -> Cycle.START");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(cycleName);
    command.mSecondParam.pushBack(g_nStringIdSTART);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));
    
    BOOST_TEST_INFO("Event connection: Cycle.EO -> CTU.CU");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(cycleName);
    command.mFirstParam.pushBack(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(ctuName);
    command.mSecondParam.pushBack(g_nStringIdCU);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: CTU.CUO -> ADD.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdCUO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(addName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: ADD.CNF -> UINT2UINT_3.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(addName);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintThird);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.IND -> UINT2UINT_1.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdIND);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintFirst);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_1.CNF -> MUL.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(uint2uintFirst);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(mulName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: MUL.CNF -> UINT2UINT_2.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(mulName);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintSecond);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_2.CNF -> ADD.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(uint2uintSecond);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(addName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Data
    BOOST_TEST_INFO("Event connection: CTU.CV -> ADD.IN1");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdCV);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(addName);
    command.mSecondParam.pushBack(g_nStringIdIN1);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: ADD.OUT -> UINT2UINT_3.IN");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(addName);
    command.mFirstParam.pushBack(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintThird);
    command.mSecondParam.pushBack(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.RD_1 -> UINT2UINT_1.IN");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdRD_1);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintFirst);
    command.mSecondParam.pushBack(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_1.OUT -> MUL.IN2");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(uint2uintFirst);
    command.mFirstParam.pushBack(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(mulName);
    command.mSecondParam.pushBack(g_nStringIdIN2);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: MUL.OUT -> UINT2UINT_2.IN");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(mulName);
    command.mFirstParam.pushBack(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(uint2uintSecond);
    command.mSecondParam.pushBack(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_2.OUT -> ADD.IN2");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(uint2uintSecond);
    command.mFirstParam.pushBack(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(addName);
    command.mSecondParam.pushBack(g_nStringIdIN2);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Literals
    command.mCMD = EMGMCommandType::Write;

    BOOST_TEST_INFO("Literal: Cycle.DT -> T#200ms");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(cycleName);
    command.mFirstParam.pushBack(g_nStringIdDT);
    command.mAdditionalParams = CIEC_STRING("T#200ms");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdPV);
    command.mAdditionalParams = CIEC_STRING("0");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: SUBSCRIBE.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdQI);
    command.mAdditionalParams = CIEC_STRING("TRUE");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdID);
    command.mAdditionalParams = CIEC_STRING("239.0.0.1:61000");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: MUL.IN1 -> UINT#10");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(mulName);
    command.mFirstParam.pushBack(g_nStringIdIN1);
    command.mAdditionalParams = CIEC_STRING("UINT#10");
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));
  }

  return device;
}

std::unique_ptr<CDevice> createExampleDevice(CStringDictionary::TStringId paResourceName, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);
  auto resource = new EMB_RES(paResourceName, *device);
  device->addFB(resource);

  resource->initialize();

  auto startInstanceName = g_nStringIdSTART;
  auto counterInstanceName = g_nStringIdCounter;
  auto switchInstanceName = g_nStringIdSwitch;

  resource->addFB(CTypeLib::createFB(counterInstanceName, g_nStringIdE_CTU, *resource));
  resource->addFB(CTypeLib::createFB(switchInstanceName, g_nStringIdE_SWITCH, *resource));

  forte::core::SManagementCMD command;
  command.mCMD = EMGMCommandType::CreateConnection;
  command.mDestination = CStringDictionary::scmInvalidStringId;

  BOOST_TEST_INFO("Event connection: Start.COLD -> Counter.CU");
  command.mFirstParam.pushBack(startInstanceName);
  command.mFirstParam.pushBack(g_nStringIdCOLD);
  command.mSecondParam.pushBack(counterInstanceName);
  command.mSecondParam.pushBack(g_nStringIdCU);

  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO("Event connection: Counter.CUO -> Switch.EI");
  command.mFirstParam.clear();
  command.mFirstParam.pushBack(counterInstanceName);
  command.mFirstParam.pushBack(g_nStringIdCUO);
  command.mSecondParam.clear();
  command.mSecondParam.pushBack(switchInstanceName);
  command.mSecondParam.pushBack(g_nStringIdEI);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO("Data connection: Counter.Q -> Switch.G ");
  command.mFirstParam.clear();
  command.mFirstParam.pushBack(counterInstanceName);
  command.mFirstParam.pushBack(g_nStringIdQ);
  command.mSecondParam.clear();
  command.mSecondParam.pushBack(switchInstanceName);
  command.mSecondParam.pushBack(g_nStringIdG);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO(" Data constant value: Counter.PV = 1");
  command.mFirstParam.clear();
  command.mFirstParam.pushBack(counterInstanceName);
  command.mFirstParam.pushBack(g_nStringIdPV);
  BOOST_CHECK(EMGMResponse::Ready == resource->writeValue(command.mFirstParam, CIEC_STRING(std::string("1")), false));

  BOOST_TEST_INFO("Event connection: Switch.EO1 -> Counter.R ");
  command.mFirstParam.clear();
  command.mFirstParam.pushBack(switchInstanceName);
  command.mFirstParam.pushBack(g_nStringIdEO1);
  command.mSecondParam.clear();
  command.mSecondParam.pushBack(counterInstanceName);
  command.mSecondParam.pushBack(g_nStringIdR);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  return device;
}

std::unordered_map<std::string, std::vector<EventMessage>> getEventMessages(std::string path){
  
  const bt_plugin* ctfPlugin;
  if(BT_PLUGIN_FIND_STATUS_OK != bt_plugin_find("ctf", BT_FALSE, BT_FALSE, BT_TRUE, BT_FALSE, BT_TRUE, &ctfPlugin)){
    std::cout << "Could not load ctf plugin" << std::endl;
    std::abort();
  }
  auto fileSourceClass = bt_plugin_borrow_source_component_class_by_name_const(ctfPlugin, "fs"); 

  // const bt_plugin* utilsPlugin;
  // if(BT_PLUGIN_FIND_STATUS_OK != bt_plugin_find("utils", BT_FALSE, BT_FALSE, BT_TRUE, BT_FALSE, BT_TRUE, &utilsPlugin)){
  //   std::cout << "Could not load utils plugin" << std::endl;
  //   std::abort();
  // }
  // auto muxerFilterClass = bt_plugin_borrow_filter_component_class_by_name_const(utilsPlugin, "muxer"); 

  const bt_plugin* fortePlugin;
  if(BT_PLUGIN_FIND_STATUS_OK != bt_plugin_find("forte", BT_FALSE, BT_FALSE, BT_FALSE, BT_TRUE, BT_TRUE, &fortePlugin)){
    std::cout << "Could not load forte plugin" << std::endl;
    std::abort();
  }
  auto forteReaderClass = bt_plugin_borrow_sink_component_class_by_name_const(fortePlugin, "event_reader"); 

  // create graph
  auto graph = bt_graph_create(0);
  
  // Source component
  // create parameters to file source component
  const bt_component_source* tracesComponent;
  auto parameters = bt_value_map_create();
  bt_value *dirsArray;

  if(BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK != bt_value_map_insert_empty_array_entry(parameters, "inputs", &dirsArray)){
    std::cout << "Could not add empty array to map parameter for ctf.source.fs component" << std::endl;
    std::abort();
  }

  if(BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK != bt_value_array_append_string_element(dirsArray, path.c_str())){
    std::cout << "Could not add input folder to ctf.source.fs component's input parameter" << std::endl;
    std::abort();
  }

  if(BT_GRAPH_ADD_COMPONENT_STATUS_OK != bt_graph_add_source_component(graph, fileSourceClass, "traces", parameters, BT_LOGGING_LEVEL_TRACE, &tracesComponent)){
    std::cout << "Could not create Source component" << std::endl;
    std::abort();
  }
  
  // const bt_component_filter*  muxerComponent;
  // if(BT_GRAPH_ADD_COMPONENT_STATUS_OK != bt_graph_add_filter_component(graph, muxerFilterClass, "muxer", nullptr, BT_LOGGING_LEVEL_TRACE, &muxerComponent)){
  //   std::cout << "Could not " << std::endl;
  //   std::abort();
  // }

  std::unordered_map<std::string, std::vector<EventMessage>> messages;

  for(uint64_t i = 0; i < bt_component_source_get_output_port_count(tracesComponent); i++){
    auto port = bt_component_source_borrow_output_port_by_index_const(tracesComponent, i);
    auto resourceName = getResourceNameFromTraceOutputPort(port);
    messages.insert({resourceName, {}});

    const bt_component_sink*  forteReaderComponent;
    auto componentName = std::string("forteReader_") + std::to_string(i); 
    
    if(BT_GRAPH_ADD_COMPONENT_STATUS_OK != 
    bt_graph_add_sink_component_with_initialize_method_data(graph, forteReaderClass, componentName.c_str(), nullptr, &messages[resourceName], BT_LOGGING_LEVEL_TRACE, &forteReaderComponent)){
      std::cout << "Could not create forte event reader component number " << i << std::endl;
      std::abort();
    }

    if(BT_GRAPH_CONNECT_PORTS_STATUS_OK !=  bt_graph_connect_ports(graph, 
        bt_component_source_borrow_output_port_by_index_const(tracesComponent, i), 
        bt_component_sink_borrow_input_port_by_index_const(forteReaderComponent, 0), 
        nullptr)){
          std::cout << "Could not add connection " << i << " from source to forte" << std::endl;
          std::abort();
      }
  }    

  // if(BT_GRAPH_CONNECT_PORTS_STATUS_OK !=  bt_graph_connect_ports(graph, 
  //   bt_component_filter_borrow_output_port_by_index_const(muxerComponent, 0), 
  //   bt_component_sink_borrow_input_port_by_index_const(forteReaderComponent, 0), 
  //   nullptr)){
      
  //     std::abort();
  // }

  if(BT_GRAPH_RUN_STATUS_OK != bt_graph_run(graph)){
    std::cout << "Could not run graph" << std::endl;
    std::abort();
  }

  return messages;	
}

std::unordered_map<std::string, std::vector<EventMessage>> getNeededEvents(const std::unordered_map<std::string, std::vector<EventMessage>>& paEvents, std::function<bool(CStringDictionary::TStringId)> paIsNeeded){

  std::unordered_map<std::string, std::vector<EventMessage>> externalEvents;

  for(const auto& [resourceName, tracedMessages] : paEvents){
    externalEvents.insert({resourceName, {}});
    auto& externalEventMessages = externalEvents[resourceName];
    
    for(auto& message : tracedMessages ){
      if(message.getEventType() == "sendOutputEvent" && paIsNeeded(CStringDictionary::getInstance().getId(message.getTypeName().c_str()))){
        externalEventMessages.push_back(message);
      }
    }
  }

  return externalEvents;
}

CResource* getResource(CDevice* paDevice, CStringDictionary::TStringId paResourceName){
  // get the resource instance out of the device
  forte::core::TNameIdentifier id;
  id.pushBack(paResourceName);
  forte::core::TNameIdentifier::CIterator nonConstIterator(id.begin());
  return dynamic_cast<CResource*> (paDevice->getContainedFB(nonConstIterator));
}

void printPrettyMessages(const std::vector<EventMessage>& messages) {

  // get the pretty text of a timestamp difference
  auto getTimestampDifference = [](int64_t current, std::optional<int64_t> prev) -> std::string {
    if(prev == std::nullopt){
      return "(+?.\?\?\?\?\?\?\?\?\?)";
    }

    auto diff = current - prev.value();

    auto seconds = diff / 1000000000;
    diff %= 1000000000;

    std::stringstream ss;
    ss << "(" << (diff < 0 ? "-" : "+") << 
      std::setw(1) << std::setfill('0') << seconds << 
      "." << 
      std::setw(9) << std::setfill('0') << diff  << ")"; 
    
    return ss.str();
  };

  std::optional<int64_t> lastTimestamp = std::nullopt;
  for(const auto& message : messages) {
    std::cout << message.getTimestampString() 
      << " " << getTimestampDifference(message.getTimestamp(), lastTimestamp) << " "
      << message.getPayloadString() << std::endl;
    lastTimestamp = message.getTimestamp();
  }
}

void prepareTraceTest(std::string paDestMetadata) {
  // remove previous trace files
  std::filesystem::path destMetadata(CTF_OUTPUT_DIR);
  destMetadata /= std::move(paDestMetadata);

  std::filesystem::remove_all(CTF_OUTPUT_DIR);
  std::filesystem::create_directory(CTF_OUTPUT_DIR);
  std::filesystem::copy_file(METADATA_FILE, destMetadata);

  BarectfPlatformFORTE::setup(CTF_OUTPUT_DIR);
}

