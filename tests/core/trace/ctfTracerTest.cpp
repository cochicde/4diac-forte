/*******************************************************************************
 * Copyright (c) 2024 Jose Cabral
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Jose Cabral- initial API and implementation and/or initial documentation
 *******************************************************************************/
#include <thread>
#include <functional>

#include <babeltrace2/babeltrace.h>
#include <boost/test/unit_test.hpp>

#include "config.h"
#include "device.h"
#include "ecet.h"
#include "EventMessage.h"
#include "trace/barectf_platform_forte.h"
#include "../fbtests/fbtesterglobalfixture.h"
#include "stdfblib/ita/replay/deviceReplayer.h"
#include "stdfblib/ita/replay/utils.h"
#include "ForteBootFileLoader.h"
#include "CommandParser.h"



#ifdef FORTE_ENABLE_GENERATED_SOURCE_CPP
#include "ctfTracerTest_gen.cpp"
#endif


// ******************************* //
// * Helper Methods Declarations * //
// ******************************* //

namespace {

  /**
   * @brief Cleanup the output directory of traces, copy the metadata file to it and setups the bareCTF tracer to it 
   * 
   * @param paDestMetadata metada destination file inside the CTF_OUTPUT_DIR
   */
  void prepareTraceTest(std::string paDestMetadata);

  /**
   * @brief create a FB network with a E_SWITCH and E_CTU  with some connections 
   * 
   * @param paResourceName name for the resource where the network is located
   * @param paDeviceName name for the device
   * 
   * @return the created device wuth the network of FB in it
  */
  std::unique_ptr<CDevice> createExampleDevice(CStringDictionary::TStringId paResourceName, CStringDictionary::TStringId paDeviceName = g_nStringIdMyDevice);

  /**
   * @brief Create a Non Deterministic device. It contains two resources communicating with each other and a cycle event
   * which is injecting changes in the order of the external events into one of the resources
   * 
   * @param paResourceName1 name of the first resource
   * @param paResourceName2 name of the second resource
   * @param paDeviceName name of the device
   * @return the created device with the network of FBs in it 
   */
  std::unique_ptr<CDevice> createNonDeterministicExample(CStringDictionary::TStringId paResourceName1, 
        CStringDictionary::TStringId paResourceName2, 
        CStringDictionary::TStringId paDeviceName = g_nStringIdMyDevice);

  /**
   * @brief Create a device from file path
   * 
   * @param paDeviceName name of the device
   * @param paFilePath path to the boot file of the device
   * @return the created device 
   */
  std::unique_ptr<CDevice> createDeviceFromFile(CStringDictionary::TStringId paDeviceName, const std::string& paFilePath);

  /**
   * @brief Compares two maps of expected messages from resources to the actual ones
   * 
   * @param paExpected expected messages
   * @param paActual actual messages
   */
  void checkMessages(std::unordered_map<std::string, std::vector<EventMessage>>& paExpected, std::unordered_map<std::string, std::vector<EventMessage>>& paActual);

  void testAlgorithm(std::function<std::unique_ptr<CDevice>(void)> paCreateDevice, const std::size_t milliSeconds);

  void testTraces(CDevice& paDevice, std::unordered_map<std::string, std::vector<EventMessage>>& paAllTracedEvents, 
  std::unordered_map<std::string, std::vector<EventMessage>>& paAllGeneratedEvents);

}

USE_STRING_ID(Counter);
USE_STRING_ID(COLD);
USE_STRING_ID(CU);
USE_STRING_ID(CUO);
USE_STRING_ID(EI);
USE_STRING_ID(EO1);
USE_STRING_ID(E_CTU);
USE_STRING_ID(E_SWITCH);
USE_STRING_ID(G);
USE_STRING_ID(MyDevice);
USE_STRING_ID(PV);
USE_STRING_ID(Q);
USE_STRING_ID(R);
USE_STRING_ID(START);
USE_STRING_ID(Switch);

/**
 * @brief Helper operator for BOOST_TEST to print 
 * 
 * @param paOs Stream to print to
 * @param paEventMessage Message to print
 * @return std::ostream& stream with the printed message
 */
std::ostream& operator<<(std::ostream &paOs, const EventMessage &paEventMessage) {
  paOs << paEventMessage.getPayloadString();
  return paOs;
}

BOOST_AUTO_TEST_SUITE (tracer_test)

BOOST_AUTO_TEST_CASE(sequential_events_test) {

  prepareTraceTest("metadata");

  auto resourceName = g_nStringIdMyResource;
  auto deviceName = g_nStringIdMyDevice;

  // The inner scope is to make sure the destructors of the resources are 
  // called which flushes the output
  {
    auto device = createExampleDevice(resourceName, deviceName);

    auto resource = dynamic_cast<CResource*>(forte::ita::replay::utils::getFB(device.get(), resourceName));

    device->startDevice();
    // wait for all events to be triggered
    while(resource->getResourceEventExecution()->isProcessingEvents()){
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    device->changeExecutionState(EMGMCommandType::Stop);
    resource->getResourceEventExecution()->joinEventChainExecutionThread();
  }

  // disable logging 
  BarectfPlatformFORTE::setup("");

  std::unordered_map<std::string, std::vector<EventMessage>>  expectedMessages;

  auto addInitialEvents = [](std::vector<EventMessage>& paMessages){
    paMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 0, 0, std::vector<std::string>{}),0);

  };

  // auto addFinalEvents = [](std::vector<EventMessage>& paMessages, std::size_t paFinalEventCount){
  //   paMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 2, paFinalEventCount, std::vector<std::string>{}),0);
  // };

  // device resource has no events
  expectedMessages[CStringDictionary::get(deviceName)] = {}; 

  // default resource in the test device
  expectedMessages[CStringDictionary::get(g_nStringIdEMB_RES)] = {}; 

  // auto& defaultResourceMessages = expectedMessages[CStringDictionary::get(g_nStringIdEMB_RES)];
  // addInitialEvents(defaultResourceMessages);
  // addFinalEvents(defaultResourceMessages, 0); // the RESTART output event doesn't generate any event since it's not connected to anything
  
  // resource with example FBs
  expectedMessages[CStringDictionary::get(resourceName)] = {};

  auto& resourceMessages = expectedMessages[CStringDictionary::get(resourceName)];
  addInitialEvents(resourceMessages);

  auto eventCounter = 0;

  // timestamp cannot properly be tested, so setting everythin to zero

  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 0),0);
  eventCounter++;
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"FALSE", "0"}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "TRUE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "1"), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 0, eventCounter, std::vector<std::string>{"TRUE", "1"}),0);
  resourceMessages.emplace_back("inputData", std::make_unique<FBDataPayload>("E_SWITCH", "Switch", 0, "TRUE"), 0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_SWITCH", "Switch", 0),0);
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_SWITCH", "Switch", std::vector<std::string>{"TRUE"}, std::vector<std::string>{}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  eventCounter++;
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_SWITCH", "Switch", 1, eventCounter, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 1),0);
  eventCounter++;
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"TRUE", "1"}, std::vector<std::string>{}, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "FALSE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "0"), 0);
  // resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 1, eventCounter, std::vector<std::string>{"FALSE", "0"}),0);
  // addFinalEvents(resourceMessages, eventCounter);

  auto ctfMessages = forte::ita::replay::utils::getEventMessages(CTF_OUTPUT_DIR).value();

  checkMessages(expectedMessages, ctfMessages);
}

BOOST_AUTO_TEST_CASE(non_deterministic_events_test) {

  auto createDevice = [] () {
    auto resource1Name = g_nStringIdMyResource;
    auto resource2Name = g_nStringIdMyResource2;
    auto deviceName = g_nStringIdMyDevice;
    return createNonDeterministicExample(resource1Name, resource2Name, deviceName);
  };  
 testAlgorithm(createDevice, 5000);
}

BOOST_AUTO_TEST_CASE(reference_systems_test) {
  auto createDevice = [](){
    return createDeviceFromFile(g_nStringIdReferenceSystemDevice, REFERENCE_SYSTEMS_FILE);
  };

  testAlgorithm(createDevice, 60000);
}

BOOST_AUTO_TEST_SUITE_END()

// ****************************** //
// * Helper Methods Definitions * //
// ****************************** //

namespace {

void prepareTraceTest(std::string paDestMetadata) {
  std::filesystem::path destMetadata(CTF_OUTPUT_DIR);

  // remove previous trace files
  std::filesystem::remove_all(destMetadata);
  std::filesystem::create_directory(destMetadata);

  std::filesystem::copy_file(METADATA_FILE, destMetadata / std::move(paDestMetadata));

  BarectfPlatformFORTE::setup(destMetadata);
}

std::unique_ptr<CDevice> createExampleDevice(CStringDictionary::TStringId paResourceName, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);

  BOOST_TEST_INFO("Create Resource");
  BOOST_CHECK(EMGMResponse::Ready == device->createFB(paResourceName, g_nStringIdEMB_RES));

  BOOST_TEST_INFO("Start Device");
  BOOST_CHECK(device->initialize());
  
  auto resource = dynamic_cast<CResource*>(forte::ita::replay::utils::getFB(device.get(), paResourceName));

  auto startInstanceName = g_nStringIdSTART;
  auto counterInstanceName = g_nStringIdCounter;
  auto switchInstanceName = g_nStringIdSwitch;

  BOOST_TEST_INFO("Create E_CTU");
  BOOST_CHECK(EMGMResponse::Ready == resource->createFB(counterInstanceName, g_nStringIdE_CTU));

  BOOST_TEST_INFO("Create E_SWITCH");
  BOOST_CHECK(EMGMResponse::Ready == resource->createFB(switchInstanceName, g_nStringIdE_SWITCH));

  forte::core::SManagementCMD command;
  command.mCMD = EMGMCommandType::CreateConnection;
  command.mDestination = CStringDictionary::scmInvalidStringId;

  BOOST_TEST_INFO("Event connection: Start.COLD -> Counter.CU");
  command.mFirstParam.push_back(startInstanceName);
  command.mFirstParam.push_back(g_nStringIdCOLD);
  command.mSecondParam.push_back(counterInstanceName);
  command.mSecondParam.push_back(g_nStringIdCU);

  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO("Event connection: Counter.CUO -> Switch.EI");
  command.mFirstParam.clear();
  command.mFirstParam.push_back(counterInstanceName);
  command.mFirstParam.push_back(g_nStringIdCUO);
  command.mSecondParam.clear();
  command.mSecondParam.push_back(switchInstanceName);
  command.mSecondParam.push_back(g_nStringIdEI);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO("Data connection: Counter.Q -> Switch.G ");
  command.mFirstParam.clear();
  command.mFirstParam.push_back(counterInstanceName);
  command.mFirstParam.push_back(g_nStringIdQ);
  command.mSecondParam.clear();
  command.mSecondParam.push_back(switchInstanceName);
  command.mSecondParam.push_back(g_nStringIdG);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  BOOST_TEST_INFO(" Data constant value: Counter.PV = 1");
  command.mFirstParam.clear();
  command.mFirstParam.push_back(counterInstanceName);
  command.mFirstParam.push_back(g_nStringIdPV);
  BOOST_CHECK(EMGMResponse::Ready == resource->writeValue(command.mFirstParam, std::string("1"), false));

  BOOST_TEST_INFO("Event connection: Switch.EO1 -> Counter.R ");
  command.mFirstParam.clear();
  command.mFirstParam.push_back(switchInstanceName);
  command.mFirstParam.push_back(g_nStringIdEO1);
  command.mSecondParam.clear();
  command.mSecondParam.push_back(counterInstanceName);
  command.mSecondParam.push_back(g_nStringIdR);
  BOOST_CHECK(EMGMResponse::Ready == resource->executeMGMCommand(command));

  return device;
}

std::unique_ptr<CDevice> createNonDeterministicExample(CStringDictionary::TStringId paResourceName1, CStringDictionary::TStringId paResourceName2, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);

  BOOST_TEST_INFO("Create Resource 1");
  BOOST_CHECK(EMGMResponse::Ready == device->createFB(paResourceName1, g_nStringIdEMB_RES));

  BOOST_TEST_INFO("Create Resource 2");
  BOOST_CHECK(EMGMResponse::Ready == device->createFB(paResourceName2, g_nStringIdEMB_RES));

  BOOST_TEST_INFO("Start Device");
  BOOST_CHECK(device->initialize());
  
  // resource 1
  {
    auto resource = dynamic_cast<CResource*>(forte::ita::replay::utils::getFB(device.get(), paResourceName1));

    auto cycleName = g_nStringIdE_CYCLE;
    auto ctuName = g_nStringIdE_CTU;
    auto publishName = g_nStringIdPUBLISH_1;

    BOOST_TEST_INFO(CStringDictionary::get(paResourceName1));

    BOOST_TEST_INFO("Create FB Cycle");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(cycleName, g_nStringIdE_CYCLE));
      
    BOOST_TEST_INFO("Create FB CTU");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(ctuName, g_nStringIdE_CTU));

    BOOST_TEST_INFO("Create FB Publish");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(publishName, g_nStringIdPUBLISH_1));

    forte::core::SManagementCMD command;
    command.mCMD = EMGMCommandType::CreateConnection;
    command.mDestination = CStringDictionary::scmInvalidStringId;

    // Events
    BOOST_TEST_INFO("Event connection: Start.COLD -> PUBLISH.INIT");
    command.mFirstParam.push_back(g_nStringIdSTART);
    command.mFirstParam.push_back(g_nStringIdCOLD);
    command.mSecondParam.push_back(publishName);
    command.mSecondParam.push_back(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Publish.INITO -> Cycle.START");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(publishName);
    command.mFirstParam.push_back(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(cycleName);
    command.mSecondParam.push_back(g_nStringIdSTART);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: Cycle.EO -> CTU.CU");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(cycleName);
    command.mFirstParam.push_back(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(ctuName);
    command.mSecondParam.push_back(g_nStringIdCU);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: CTU.CUO -> Publish.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdCUO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(publishName);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Data
    BOOST_TEST_INFO("Event connection: CTU.CV -> Publish.SD_1");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdCV);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(publishName);
    command.mSecondParam.push_back(g_nStringIdSD_1);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Literals
    command.mCMD = EMGMCommandType::Write;

    BOOST_TEST_INFO("Literal: Cycle.DT -> T#200ms");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(cycleName);
    command.mFirstParam.push_back(g_nStringIdDT);
    command.mAdditionalParams = "T#200ms";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdPV);
    command.mAdditionalParams = "0";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(publishName);
    command.mFirstParam.push_back(g_nStringIdQI);
    command.mAdditionalParams = "TRUE";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(publishName);
    command.mFirstParam.push_back(g_nStringIdID);
    command.mAdditionalParams = "239.0.0.1:61000";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

  }

  // resource 2
  {
    auto resource = dynamic_cast<CResource*>(forte::ita::replay::utils::getFB(device.get(), paResourceName2));

    auto cycleName = g_nStringIdE_CYCLE;
    auto ctuName = g_nStringIdE_CTU;
    auto subscribeName = g_nStringIdSUBSCRIBE_1;
    auto addName = g_nStringIdADD;
    auto mulName = g_nStringIdMUL;
    auto uint2uintFirst = g_nStringIdUINT2UINT;
    auto uint2uintSecond = g_nStringIdUINT2UINT_1;
    auto uint2uintThird = g_nStringIdUINT2UINT_2;

    BOOST_TEST_INFO(CStringDictionary::get(paResourceName2));

    BOOST_TEST_INFO("Create FB Subscribe");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(subscribeName, g_nStringIdSUBSCRIBE_1));

    BOOST_TEST_INFO("Create FB Cycle");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(cycleName, g_nStringIdE_CYCLE));
      
    BOOST_TEST_INFO("Create FB CTU");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(ctuName, g_nStringIdE_CTU));

    BOOST_TEST_INFO("Create FB ADD");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(addName, g_nStringIdF_ADD));

    BOOST_TEST_INFO("Create FB MUL");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(mulName, g_nStringIdF_MUL));

    BOOST_TEST_INFO("Create FB UINT2UINT 1");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(uint2uintFirst, g_nStringIdUINT2UINT));

    BOOST_TEST_INFO("Create FB UINT2UINT 2");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(uint2uintSecond, g_nStringIdUINT2UINT));

    BOOST_TEST_INFO("Create FB UINT2UINT 3");
    BOOST_ASSERT(EMGMResponse::Ready == resource->createFB(uint2uintThird, g_nStringIdUINT2UINT));

    forte::core::SManagementCMD command;
    command.mCMD = EMGMCommandType::CreateConnection;
    command.mDestination = CStringDictionary::scmInvalidStringId;

    // Events
    BOOST_TEST_INFO("Event connection: Start.COLD -> SUBSCRIBE.INIT");
    command.mFirstParam.push_back(g_nStringIdSTART);
    command.mFirstParam.push_back(g_nStringIdCOLD);
    command.mSecondParam.push_back(subscribeName);
    command.mSecondParam.push_back(g_nStringIdINIT);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.INIT -> Cycle.START");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(subscribeName);
    command.mFirstParam.push_back(g_nStringIdINITO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(cycleName);
    command.mSecondParam.push_back(g_nStringIdSTART);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));
    
    BOOST_TEST_INFO("Event connection: Cycle.EO -> CTU.CU");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(cycleName);
    command.mFirstParam.push_back(g_nStringIdEO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(ctuName);
    command.mSecondParam.push_back(g_nStringIdCU);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: CTU.CUO -> ADD.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdCUO);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(addName);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: ADD.CNF -> UINT2UINT_3.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(addName);
    command.mFirstParam.push_back(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintThird);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.IND -> UINT2UINT_1.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(subscribeName);
    command.mFirstParam.push_back(g_nStringIdIND);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintFirst);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_1.CNF -> MUL.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(uint2uintFirst);
    command.mFirstParam.push_back(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(mulName);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: MUL.CNF -> UINT2UINT_2.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(mulName);
    command.mFirstParam.push_back(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintSecond);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_2.CNF -> ADD.REQ");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(uint2uintSecond);
    command.mFirstParam.push_back(g_nStringIdCNF);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(addName);
    command.mSecondParam.push_back(g_nStringIdREQ);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Data
    BOOST_TEST_INFO("Event connection: CTU.CV -> ADD.IN1");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdCV);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(addName);
    command.mSecondParam.push_back(g_nStringIdIN1);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: ADD.OUT -> UINT2UINT_3.IN");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(addName);
    command.mFirstParam.push_back(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintThird);
    command.mSecondParam.push_back(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: SUBSCRIBE.RD_1 -> UINT2UINT_1.IN");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(subscribeName);
    command.mFirstParam.push_back(g_nStringIdRD_1);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintFirst);
    command.mSecondParam.push_back(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_1.OUT -> MUL.IN2");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(uint2uintFirst);
    command.mFirstParam.push_back(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(mulName);
    command.mSecondParam.push_back(g_nStringIdIN2);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: MUL.OUT -> UINT2UINT_2.IN");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(mulName);
    command.mFirstParam.push_back(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(uint2uintSecond);
    command.mSecondParam.push_back(g_nStringIdIN);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Event connection: UINT2UINT_2.OUT -> ADD.IN2");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(uint2uintSecond);
    command.mFirstParam.push_back(g_nStringIdOUT);
    command.mSecondParam.clear();
    command.mSecondParam.push_back(addName);
    command.mSecondParam.push_back(g_nStringIdIN2);
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    // Literals
    command.mCMD = EMGMCommandType::Write;

    BOOST_TEST_INFO("Literal: Cycle.DT -> T#200ms");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(cycleName);
    command.mFirstParam.push_back(g_nStringIdDT);
    command.mAdditionalParams = "T#200ms";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(ctuName);
    command.mFirstParam.push_back(g_nStringIdPV);
    command.mAdditionalParams = "0";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: SUBSCRIBE.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(subscribeName);
    command.mFirstParam.push_back(g_nStringIdQI);
    command.mAdditionalParams = "TRUE";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(subscribeName);
    command.mFirstParam.push_back(g_nStringIdID);
    command.mAdditionalParams = "239.0.0.1:61000";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: MUL.IN1 -> UINT#10");
    command.mFirstParam.clear();
    command.mFirstParam.push_back(mulName);
    command.mFirstParam.push_back(g_nStringIdIN1);
    command.mAdditionalParams = "UINT#10";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));
  }

  return device;
}

std::unique_ptr<CDevice> createDeviceFromFile(CStringDictionary::TStringId paDeviceName, const std::string& paFilePath) {
  auto device = std::make_unique<CTesterDevice>(paDeviceName);
  device->initialize();
  forte::ita::CommandParser commandParser(*device);

  ForteBootFileLoader fileLoader([&commandParser](const char* paDest, char* paCommand) -> bool {
    return EMGMResponse::Ready == commandParser.parseAndExecuteMGMCommand(paDest, paCommand);
  }, paFilePath);
  BOOST_ASSERT(LoadBootResult::LOAD_RESULT_OK == fileLoader.loadBootFile());
  return device;
}

void testAlgorithm(std::function<std::unique_ptr<CDevice>(void)> paCreateDevice, const std::size_t milliSeconds) {
  prepareTraceTest("metadata");

  forte::ita::replay::utils::setFactoriesSettings({});

  auto killDevice = [](CDevice& paDevice, std::size_t paMillisecondsToSleepBeforeKilling = 1000){
    // wait for all events to be triggered
    std::this_thread::sleep_for(std::chrono::milliseconds(paMillisecondsToSleepBeforeKilling));

    paDevice.changeExecutionState(EMGMCommandType::Kill);
    paDevice.awaitShutdown();
  };

  {
    auto device = paCreateDevice(); 
    device->startDevice();
    killDevice(*device, milliSeconds);
  }

  // disable logging 
  BarectfPlatformFORTE::setup("");

  auto allTracedEvents = forte::ita::replay::utils::getEventMessages(CTF_OUTPUT_DIR).value();

  forte::ita::replay::utils::setFactoriesSettings(
      {EcetFactory::AvailableEcets::fake,
      TimerHandlerFactory::AvailableTimers::fakeTimer,
      CFlexibleTracer::AvailableTracers::Internal});

  // test with reproduce all
  {
    auto device = paCreateDevice(); 

    auto allTracedExternalEvents = forte::ita::replay::utils::filterEventsForReplayDevice(allTracedEvents, *device);

    std::unordered_map<std::string, std::vector<EventMessage>> reproducedEvents;

    {
      auto deviceReplayer = CDeviceReplayer(*device, allTracedExternalEvents);
      device->startDevice();
      reproducedEvents = deviceReplayer.reproduceAll();
    }

    killDevice(*device);

    testTraces(*device, allTracedEvents, reproducedEvents);
    
  }

  // test with reproduce next event
  {
    auto device = paCreateDevice(); 

    auto allTracedExternalEvents = forte::ita::replay::utils::filterEventsForReplayDevice(allTracedEvents, *device);

    auto deviceReplayer = CDeviceReplayer(*device, allTracedExternalEvents);

    device->startDevice();

    for(auto container : device->getChildren()){
      auto resourceName = container->getInstanceName();
      while(deviceReplayer.reproduceNextEvent(resourceName));
    }

    auto reproducedEvents = deviceReplayer.getGeneratedEvents();

    killDevice(*device);

    testTraces(*device, allTracedEvents, reproducedEvents);
  }
}

void testTraces(CDevice& paDevice, std::unordered_map<std::string, std::vector<EventMessage>>& paAllTracedEvents, 
  std::unordered_map<std::string, std::vector<EventMessage>>& paAllGeneratedEvents) {
  
  // To test the algorithm, we compare only the outputs events to the generated ones
  auto isInteretingType = [](const EventMessage& paMessage){
    auto messageType = paMessage.getEventType();
    return messageType == "sendOutputEvent";
  };

  auto allInterestingEvents = forte::ita::replay::utils::filterEvents(paAllTracedEvents, isInteretingType);

  allInterestingEvents.erase(paDevice.getInstanceName());

  auto interestingGeneratedMessages = forte::ita::replay::utils::filterEvents(paAllGeneratedEvents, isInteretingType);

  checkMessages(allInterestingEvents, interestingGeneratedMessages);
}

void checkMessages(std::unordered_map<std::string, std::vector<EventMessage>>& paExpected, 
  std::unordered_map<std::string, std::vector<EventMessage>>& paActual){
  
  // check that they have the same amount of keys
  BOOST_CHECK(paExpected.size() == paActual.size());

  for(auto& [resource, expectedMessages] : paExpected){
    BOOST_CHECK(paActual.find(resource) != paActual.end());

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

}

