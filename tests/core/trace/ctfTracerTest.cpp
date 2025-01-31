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
#include "utils/parameterParser.h"
#include "deviceReplayer.h"
#include "arch/timerHandlerFactory.h"
#include "core/ecetFactory.h"
#include "utils.h"
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
   * @brief Get the list of message from a directory containing CTF traces separated by resource
   * 
   * @param path directory containing ctf traces
   * @return events mapped to the resource they belong to
   */
  std::unordered_map<std::string, std::vector<EventMessage>> getEventMessages(std::string path);

  /**
   * @brief Get the resource name from an output port of the source port of a file source component
   * 
   * When creating a source file component, the output ports are based on the files which are based on each resource 
   * 
   * @param paPort the port where to read the name from
   * @return the name of the resource
   */
  std::string getResourceNameFromTraceOutputPort(const bt_port_output*	paPort);

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
   * @brief Filter a list of events based on a given function
   * 
   * @param paEvents list of events to be filtered, separated by a string key (usually resource name)
   * @param paFilterIn function to check if the event should be kept
   * @return list of filtered events 
   */
  std::unordered_map<std::string, std::vector<EventMessage>> filterEvents(const std::unordered_map<std::string, 
      std::vector<EventMessage>>& paEvents, std::function<bool(const EventMessage&)> paFilterIn);

  /**
   * @brief Compares two maps of expected messages from resources to the actual ones
   * 
   * @param paExpected expected messages
   * @param paActual actual messages
   */
  void checkMessages(std::unordered_map<std::string, std::vector<EventMessage>>& paExpected, std::unordered_map<std::string, std::vector<EventMessage>>& paActual);

  void testAlgorithm(std::function<std::unique_ptr<CDevice>(void)> paCreateDevice, const std::size_t milliSeconds);

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

    auto resource = dynamic_cast<CResource*>(forte::unit_test::utils::getFB(device.get(), resourceName));

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
  expectedMessages[CStringDictionary::getInstance().get(deviceName)] = {}; 

  // default resource in the test device
  expectedMessages[CStringDictionary::getInstance().get(g_nStringIdEMB_RES)] = {}; 

  // auto& defaultResourceMessages = expectedMessages[CStringDictionary::getInstance().get(g_nStringIdEMB_RES)];
  // addInitialEvents(defaultResourceMessages);
  // addFinalEvents(defaultResourceMessages, 0); // the RESTART output event doesn't generate any event since it's not connected to anything
  
  // resource with example FBs
  expectedMessages[CStringDictionary::getInstance().get(resourceName)] = {};

  auto& resourceMessages = expectedMessages[CStringDictionary::getInstance().get(resourceName)];
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

  auto ctfMessages = getEventMessages(CTF_OUTPUT_DIR);

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

  testAlgorithm(createDevice, 3000);
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

std::unordered_map<std::string, std::vector<EventMessage>> getEventMessages(std::string path){
  
  // create graph
  auto graph = bt_graph_create(0);

  // Source file Component
  const bt_plugin* ctfPlugin;
  if(BT_PLUGIN_FIND_STATUS_OK != bt_plugin_find("ctf", BT_FALSE, BT_FALSE, BT_TRUE, BT_FALSE, BT_TRUE, &ctfPlugin)){
    std::cout << "Could not load ctf plugin" << std::endl;
    std::abort();
  }
  auto fileSourceClass = bt_plugin_borrow_source_component_class_by_name_const(ctfPlugin, "fs"); 

  // create parameters for the source file component
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

  // Forte event reader component

  const bt_plugin* fortePlugin;
  if(BT_PLUGIN_FIND_STATUS_OK != bt_plugin_find("forte", BT_FALSE, BT_FALSE, BT_FALSE, BT_TRUE, BT_TRUE, &fortePlugin)){
    std::cout << "Could not load forte plugin" << std::endl;
    std::abort();
  }
  auto forteReaderClass = bt_plugin_borrow_sink_component_class_by_name_const(fortePlugin, "event_reader"); 

  std::unordered_map<std::string, std::vector<EventMessage>> messages;

  // create a sink forte even reader component for each resource
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

  if(BT_GRAPH_RUN_STATUS_OK != bt_graph_run(graph)){
    std::cout << "Could not run graph" << std::endl;
    std::abort();
  }

  return messages;
}

std::string getResourceNameFromTraceOutputPort(const bt_port_output*	paPort)	
{
  auto outputPortBase = bt_port_output_as_port_const(paPort);
  auto portName = bt_port_get_name(outputPortBase);

  // Port name has the following pattern: TRACE-ID | STREAM-CLASS-ID | STREAM-ID
  // where STREAM-ID contains the absolut path to the trace file. 
  // The file name is given in the CTF tracer inside forte as 
  // "trace_"  + INSTANCE_NAME + "_" + DATE_TIME + ".ctf"),

  CParameterParser portNameParser(portName, '|', 3);
  portNameParser.parseParameters();
  CParameterParser fileNameParser(std::filesystem::path(portNameParser[2]).filename().c_str(), '_', 4);
  auto totalParameters = fileNameParser.parseParameters();

  std::string result = fileNameParser[1];

  // in case the instance name has underscore in it
  for(std::size_t i = 2; i < totalParameters - 2; i++ ){
    result += std::string("_") + fileNameParser[i];
  }

  return result;
}

std::unique_ptr<CDevice> createExampleDevice(CStringDictionary::TStringId paResourceName, CStringDictionary::TStringId paDeviceName){
  auto device = std::make_unique<CTesterDevice>(paDeviceName);

  BOOST_TEST_INFO("Create Resource");
  BOOST_CHECK(EMGMResponse::Ready == device->createFB(paResourceName, g_nStringIdEMB_RES));

  BOOST_TEST_INFO("Start Device");
  BOOST_CHECK(device->initialize());
  
  auto resource = dynamic_cast<CResource*>(forte::unit_test::utils::getFB(device.get(), paResourceName));

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
    auto resource = dynamic_cast<CResource*>(forte::unit_test::utils::getFB(device.get(), paResourceName1));

    auto cycleName = g_nStringIdE_CYCLE;
    auto ctuName = g_nStringIdE_CTU;
    auto publishName = g_nStringIdPUBLISH_1;

    BOOST_TEST_INFO(CStringDictionary::getInstance().get(paResourceName1));

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
    command.mAdditionalParams = "T#200ms";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdPV);
    command.mAdditionalParams = "0";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(publishName);
    command.mFirstParam.pushBack(g_nStringIdQI);
    command.mAdditionalParams = "TRUE";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(publishName);
    command.mFirstParam.pushBack(g_nStringIdID);
    command.mAdditionalParams = "239.0.0.1:61000";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

  }

  // resource 2
  {
    auto resource = dynamic_cast<CResource*>(forte::unit_test::utils::getFB(device.get(), paResourceName2));

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
    command.mAdditionalParams = "T#200ms";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: CTU.PV -> 0");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(ctuName);
    command.mFirstParam.pushBack(g_nStringIdPV);
    command.mAdditionalParams = "0";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: SUBSCRIBE.QI -> TRUE");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdQI);
    command.mAdditionalParams = "TRUE";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));


    BOOST_TEST_INFO("Literal: Pulbish.ID -> 239.0.0.1:61000");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(subscribeName);
    command.mFirstParam.pushBack(g_nStringIdID);
    command.mAdditionalParams = "239.0.0.1:61000";
    BOOST_ASSERT(EMGMResponse::Ready == resource->executeMGMCommand(command));

    BOOST_TEST_INFO("Literal: MUL.IN1 -> UINT#10");
    command.mFirstParam.clear();
    command.mFirstParam.pushBack(mulName);
    command.mFirstParam.pushBack(g_nStringIdIN1);
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

  forte::unit_test::utils::setFactoriesSettings({});

  {
    auto device = paCreateDevice(); 


    device->startDevice();
    // wait for all events to be triggered

    std::this_thread::sleep_for(std::chrono::milliseconds(milliSeconds));

    device->changeExecutionState(EMGMCommandType::Kill);
    device->awaitShutdown();
  }

  // disable logging 
  BarectfPlatformFORTE::setup("");

  auto allTracedEvents = getEventMessages(CTF_OUTPUT_DIR);

  forte::unit_test::utils::setFactoriesSettings(
      {EcetFactory::AvailableEcets::fake,
      TimerHandlerFactory::AvailableTimers::fakeTimer,
      CFlexibleTracer::AvailableTracers::Internal});

  auto device = paCreateDevice(); 


  // function to filter events which are interesting for the replay algorithm, i.e. output events from service FBs
  auto isValidType = [validTypes = forte::unit_test::utils::getValidTypes(*device)](const EventMessage& paMessage){
    if(paMessage.getEventType() != "sendOutputEvent"){
      return false;
    }
    auto type = CStringDictionary::getInstance().getId(paMessage.getPayload<AbstractPayload>()->getTypeName().c_str());
    return validTypes.find(type) != validTypes.end();
  };

  auto allTracedExternalEvents = filterEvents(allTracedEvents, isValidType);

  auto deviceReplayer = CDeviceReplayer(*device, allTracedExternalEvents);

  auto reproducedEvents = deviceReplayer.reproduceAll();

  // To test the algorithm, we compare only the outputs and instanceData events to the generated ones
  auto isInteretingType = [](const EventMessage& paMessage){
    auto messageType = paMessage.getEventType();
    return messageType == "sendOutputEvent";
  };

  auto allInterestingEvents = filterEvents(allTracedEvents, isInteretingType);

  allInterestingEvents.erase(device->getInstanceName());

  auto interestingGeneratedMessages = filterEvents(reproducedEvents, isInteretingType);

  checkMessages(allInterestingEvents, interestingGeneratedMessages);
}


std::unordered_map<std::string, std::vector<EventMessage>> filterEvents(const std::unordered_map<std::string, std::vector<EventMessage>>& paEvents, std::function<bool(const EventMessage&)> paFilterIn){

  std::unordered_map<std::string, std::vector<EventMessage>> result;

  for(const auto& [resourceName, messages] : paEvents){
    result.insert({resourceName, {}});
    auto& resultMessages = result[resourceName];

    for(auto& message : messages ){
      if(paFilterIn(message)){
        resultMessages.push_back(message);
      }
    }
  }

  return result;
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

