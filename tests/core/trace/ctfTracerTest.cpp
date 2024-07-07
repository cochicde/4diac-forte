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

#include "trace/manualEcet.h"

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


/**
 * @brief Get the list of message from a directory containing CTF traces
 * 
 * @param path Directory containing ctf traces and, if needed, the metadata associated to them
 * @return sorted list of events found in path
 */
std::unordered_map<std::string, std::vector<EventMessage>> getEventMessages(std::string path);

std::unordered_map<std::string, std::vector<EventMessage>> getExternalEventMessage(std::string path);

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

    BOOST_TEST_INFO("Expected vs traced: Same size ");
    BOOST_CHECK_EQUAL(actualMessages.size(), expectedMessages.size());

      // although vectors can be check directly, this granularity helps debugging in case some message is different
    for(size_t i = 0; i < expectedMessages.size(); i++ ){
      BOOST_TEST_INFO("Expected event number " + std::to_string(i));
      BOOST_CHECK(actualMessages[i] == expectedMessages[i]);
    }

    // add extra event to check that the comparison fails
    expectedMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_RESTART", "START", 2),0);
    BOOST_CHECK(actualMessages != expectedMessages);

    // undo in case is needed again later
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

BOOST_AUTO_TEST_CASE(sequential_events_test) {
  return;
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
  resourceMessages.emplace_back("externalEventInput", std::make_unique<FBExternalEventPayload>("E_RESTART", "START", 0, 0),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBEventPayload>("E_RESTART", "START", 0),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_RESTART", "START", 0),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBEventPayload>("E_CTU", "Counter", 0),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"FALSE", "0"}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_CTU", "Counter", 0),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "TRUE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "1"), 0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBEventPayload>("E_SWITCH", "Switch", 0),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_SWITCH", "Switch", std::vector<std::string>{"FALSE"}, std::vector<std::string>{}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("inputData", std::make_unique<FBDataPayload>("E_SWITCH", "Switch", 0, "TRUE"), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_SWITCH", "Switch", 1),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBEventPayload>("E_CTU", "Counter", 1),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"TRUE", "1"}, std::vector<std::string>{}, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_CTU", "Counter", 1),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "FALSE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "0"), 0);
  resourceMessages.emplace_back("externalEventInput", std::make_unique<FBExternalEventPayload>("E_RESTART", "START", 2, 4),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBEventPayload>("E_RESTART", "START", 2),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBEventPayload>("E_RESTART", "START", 2),0);

  auto ctfMessages = getEventMessages(CTF_OUTPUT_DIR);

  checkMessages(allMessages, ctfMessages);
}

BOOST_AUTO_TEST_CASE(non_deterministic_events_test) {
  prepareTraceTest("metadata");

  auto resource1Name = g_nStringIdMyResource;
  auto resource2Name = g_nStringIdMyResource2;
  auto deviceName = g_nStringIdMyDevice;

  // The inner scope is to make sure the destructors of the resources are 
  // called which flushes the output
  {

    auto device = createNonDeterministicExample(resource1Name, resource2Name, deviceName); 
    
    auto resource1 = getResource(device.get(), resource1Name);
    auto resource2 = getResource(device.get(), resource2Name);

    device->startDevice();
    // wait for all events to be triggered

    // TODO: Let it run for a random amount of time to make it more realistic
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    device->changeFBExecutionState(EMGMCommandType::Stop);
    resource1->getResourceEventExecution()->joinEventChainExecutionThread();
    resource2->getResourceEventExecution()->joinEventChainExecutionThread();
  }

  // disable logging 
  BarectfPlatformFORTE::setup("");

  auto allTracedEvents = getEventMessages(CTF_OUTPUT_DIR);
  auto allTracedExternalEvents = getExternalEventMessage(CTF_OUTPUT_DIR);

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

    class helper {
      public:
      helper(CResource* paResource, std::vector<EventMessage>& paExternalEvents) : resource{paResource}, 
                    ecet{dynamic_cast<CManualEventExecutionThread*>(resource->getResourceEventExecution())},
                    externalEvents{paExternalEvents} {}
      CResource* resource;
      CManualEventExecutionThread* ecet;
      std::vector<EventMessage>& externalEvents;
    };

    auto resourceNames = std::vector<CStringDictionary::TStringId>({resource1Name, resource2Name});
    
    std::vector<helper> resourceHelpers;

    for(const auto resourceName : resourceNames){
      resourceHelpers.emplace_back(getResource(device.get(), resourceName), allTracedExternalEvents[CStringDictionary::getInstance().get(resourceName)]);
    }

    device->startDevice();

    for(auto& helper : resourceHelpers){
      for(auto& externalEvent : helper.externalEvents){
        auto payload = externalEvent.getPayload<FBExternalEventPayload>();
        helper.ecet->advance(payload.mEventCounter);


        forte::core::TNameIdentifier id;
        id.pushBack(CStringDictionary::getInstance().getId(payload.mInstanceName.c_str()));
        forte::core::TNameIdentifier::CIterator anotherIterator(id.begin());
        auto fb = helper.resource->getContainedFB(anotherIterator);

        helper.ecet->insertFront(CConnectionPoint(fb, payload.mEventId));
      }
      auto releaseEcet = [&helper](TEventEntry){
        helper.ecet->removeControllFromOutside();
      };
      helper.ecet->allowInternallyGeneratedEventChains(true, releaseEcet);
    }

    device->changeFBExecutionState(EMGMCommandType::Stop);

    for(auto& helper : resourceHelpers){
      helper.resource->getResourceEventExecution()->joinEventChainExecutionThread();
    }

    std::unordered_map<std::string, std::vector<EventMessage>> expectedMessages;

    expectedMessages.insert({CStringDictionary::getInstance().get(resource1Name), std::move(expectedGeneratedMessagesResource1)});
    expectedMessages.insert({CStringDictionary::getInstance().get(resource2Name), std::move(expectedGeneratedMessagesResource2)});
    expectedMessages.insert({CStringDictionary::getInstance().get(deviceName), std::move(expectedGeneratedMessagesDevice)});

    checkMessages(expectedMessages, allTracedEvents);
  }
}

BOOST_AUTO_TEST_SUITE_END()

std::unique_ptr<CDevice> createNonDeterministicExample(CStringDictionary::TStringId paResourceName1, CStringDictionary::TStringId paResourceName2, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);
  auto resource1 = new EMB_RES(paResourceName1, *device);
  auto resource2 = new EMB_RES(paResourceName2, *device);

  device->addFB(resource1);
  device->addFB(resource2);


  resource1->initialize();
  resource2->initialize();

  auto createNetwork = [](CResource* paResource, CStringDictionary::TStringId paCycleName, 
                                                CStringDictionary::TStringId paRandom1Name,
                                                CStringDictionary::TStringId paRandom2Name,
                                                CStringDictionary::TStringId paAddName,
                                                CStringDictionary::TStringId paReal2RealName,
                                                CStringDictionary::TStringId paPublishName)
  {
    BOOST_TEST_INFO("Create FB Cycle");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paCycleName, g_nStringIdE_CYCLE, *paResource)));
    
    BOOST_TEST_INFO("Create FB Random 1");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paRandom1Name, g_nStringIdFB_RANDOM, *paResource)));

    BOOST_TEST_INFO("Create FB Random 2");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paRandom2Name, g_nStringIdFB_RANDOM, *paResource)));

    BOOST_TEST_INFO("Create FB Add");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paAddName, g_nStringIdF_ADD, *paResource)));

    BOOST_TEST_INFO("Create FB Real2Real");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paReal2RealName, g_nStringIdREAL2REAL, *paResource)));

    BOOST_TEST_INFO("Create FB Publish_1");
    //BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paPublishName, g_nStringIdPUBLISH_1, *paResource)));
    BOOST_ASSERT(EMGMResponse::Ready == paResource->addFB(CTypeLib::createFB(paPublishName, g_nStringIdFB_RANDOM, *paResource)));

    forte::core::SManagementCMD command;
    command.mCMD = EMGMCommandType::CreateConnection;
    command.mDestination = CStringDictionary::scmInvalidStringId;

    // Events

    BOOST_TEST_INFO("Event connection: Start.COLD -> Random1.INIT");
    command.mFirstParam.pushBack(g_nStringIdSTART);
    command.mFirstParam.pushBack(g_nStringIdCOLD);
    command.mSecondParam.pushBack(paRandom1Name);
    command.mSecondParam.pushBack(g_nStringIdINIT);

    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Random1.INITO -> Random2.INIT");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom1Name);
    command.mFirstParam.pushBack(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paRandom2Name);
    command.mSecondParam.pushBack(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Random2.INITO -> Publish.INIT");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom2Name);
    command.mFirstParam.pushBack(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paPublishName);
    command.mSecondParam.pushBack(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Publish.INITO -> Cycle.START");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paPublishName);
    command.mFirstParam.pushBack(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paCycleName);
    command.mSecondParam.pushBack(g_nStringIdSTART);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Cycle.EO -> Random1.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paCycleName);
    command.mFirstParam.pushBack(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paRandom1Name);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Cycle.EO -> Random2.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paCycleName);
    command.mFirstParam.pushBack(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paRandom2Name);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Random1.CNF -> Add.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom1Name);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paAddName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Random2.CNF -> Add.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom2Name);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paAddName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Add.CNF -> Real2Real.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paAddName);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paReal2RealName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));


    BOOST_TEST_INFO("Event connection: Real2Real.CNF -> Publish.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paReal2RealName);
    command.mFirstParam.pushBack(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paPublishName);
    command.mSecondParam.pushBack(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    // Data

    BOOST_TEST_INFO("Data connection: Random1.VAL -> ADD.IN1");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom1Name);
    command.mFirstParam.pushBack(g_nStringIdVAL);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paAddName);
    command.mSecondParam.pushBack(g_nStringIdIN1);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Data connection: Random2.VAL -> ADD.IN2");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom2Name);
    command.mFirstParam.pushBack(g_nStringIdVAL);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paAddName);
    command.mSecondParam.pushBack(g_nStringIdIN2);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Data connection: Add.OUT -> Real2Real.IN");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paAddName);
    command.mFirstParam.pushBack(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.pushBack(paReal2RealName);
    command.mSecondParam.pushBack(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    // BOOST_TEST_INFO("Data connection: Real2Real.OUT -> Publish.SD_1");
    // command.mFirstParam.clear();
    // command.mFirstParam.pushBack(paReal2RealName);
    // command.mFirstParam.pushBack(g_nStringIdOUT);
    // command.mSecondParam.clear();
    // command.mSecondParam.pushBack(paPublishName);
    // command.mSecondParam.pushBack(g_nStringIdSD_1);
    // BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    // Literals

    command.mCMD = EMGMCommandType::Write;

    BOOST_TEST_INFO("Literal: Cycle.DT -> T#200ms");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paCycleName);
    command.mFirstParam.pushBack(g_nStringIdDT);
    command.mAdditionalParams = CIEC_STRING("T#200ms");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: Random1.SEED -> 4");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom1Name);
    command.mFirstParam.pushBack(g_nStringIdSEED);
    command.mAdditionalParams = CIEC_STRING("4");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: Random2.SEED -> 10");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(paRandom2Name);
    command.mFirstParam.pushBack(g_nStringIdSEED);
    command.mAdditionalParams = CIEC_STRING("10");
    BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

    // BOOST_TEST_INFO("Literal: Publish.QI -> TRUE");
    // command.mFirstParam.clear();
    // command.mFirstParam.pushBack(paPublishName);
    // command.mFirstParam.pushBack(g_nStringIdQI);
    // command.mAdditionalParams = CIEC_STRING("TRUE");
    // BOOST_ASSERT(EMGMResponse::Ready == paResource->executeMGMCommand(command));

  };

  createNetwork(resource1, g_nStringIdE_CYCLE, 
                          g_nStringIdFB_RANDOM,
                          g_nStringIdFB_RANDOM_1,
                          g_nStringIdF_ADD,
                          g_nStringIdREAL2REAL,
                          g_nStringIdPUBLISH_1);
  
  createNetwork(resource2, g_nStringIdE_CYCLE_1, 
                          g_nStringIdFB_RANDOM_2,
                          g_nStringIdFB_RANDOM_3,
                          g_nStringIdF_ADD_1,
                          g_nStringIdREAL2REAL_1,
                          g_nStringIdPUBLISH_2);


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

std::unordered_map<std::string, std::vector<EventMessage>> getExternalEventMessage(std::string path){

  auto ctfMessages = getEventMessages(path);

  std::unordered_map<std::string, std::vector<EventMessage>> externalEvents;

  for(auto& [resource, tracedMessages] : ctfMessages){
    externalEvents.insert({resource, {}});
    auto& externalEventMessages = externalEvents[resource];
    
    for(const auto& message : tracedMessages ){
      if(message.getEventType() == "externalEventInput"){
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

