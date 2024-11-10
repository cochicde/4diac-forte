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

#include <babeltrace2/babeltrace.h>
#include <boost/test/unit_test.hpp>

#include "config.h"
#include "device.h"
#include "ecet.h"
#include "EventMessage.h"
#include "trace/barectf_platform_forte.h"
#include "../fbtests/fbtesterglobalfixture.h"
#include "utils/parameterParser.h"

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
   * @brief  get the resource instance out of the device
   * 
   * @param paDevice Device where the to look for the resource 
   * @param paResourceName resource name ID
   * @return a pointer to the resource with the provided name, nullptr if a resource with the provided name does not exist
   */
  CResource* getResource(CDevice* paDevice, CStringDictionary::TStringId paResourceName);

  /**
   * @brief Compares two maps of expected messages from resources to the actual ones
   * 
   * @param paExpected expected messages
   * @param paActual actual messages
   */
  void checkMessages(std::unordered_map<std::string, std::vector<EventMessage>>& paExpected, std::unordered_map<std::string, std::vector<EventMessage>>& paActual);

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

    auto resource = getResource(device.get(), resourceName);

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
    paMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_RESTART", "START", 65534),0);
    paMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 0, 0, std::vector<std::string>{}),0);

  };

  auto addFinalEvents = [](std::vector<EventMessage>& paMessages, std::size_t paFinalEventCount){
    paMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_RESTART", "START", 65534),0);
    paMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_RESTART", "START", 2, paFinalEventCount, std::vector<std::string>{}),0);
  };

  // default resource in the test device
  expectedMessages[CStringDictionary::getInstance().get(g_nStringIdEMB_RES)] = {}; 

  auto& defaultResourceMessages = expectedMessages[CStringDictionary::getInstance().get(g_nStringIdEMB_RES)];
  addInitialEvents(defaultResourceMessages);
  addFinalEvents(defaultResourceMessages, 0); // the RESTART output event doesn't generate any event since it's not connected to anything
  
  // resource with example FBs
  expectedMessages[CStringDictionary::getInstance().get(g_nStringIdMyResource)] = {};

  auto& resourceMessages = expectedMessages[CStringDictionary::getInstance().get(g_nStringIdMyResource)];
  addInitialEvents(resourceMessages);

  auto eventCounter = 0;

  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 0),0);
  eventCounter++;
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"FALSE", "0"}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  // timestamp cannot properly be tested, so setting everythin to zero
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 0, eventCounter, std::vector<std::string>{"TRUE", "1"}),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "TRUE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "1"), 0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_SWITCH", "Switch", 0),0);
  eventCounter++;
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_SWITCH", "Switch", std::vector<std::string>{"FALSE"}, std::vector<std::string>{}, std::vector<std::string>{}, std::vector<std::string>{}), 0);
  resourceMessages.emplace_back("inputData", std::make_unique<FBDataPayload>("E_SWITCH", "Switch", 0, "TRUE"), 0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_SWITCH", "Switch", 1, eventCounter, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>("E_CTU", "Counter", 1),0);
  eventCounter++;
  resourceMessages.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>("E_CTU", "Counter", std::vector<std::string>{"1"}, std::vector<std::string>{"TRUE", "1"}, std::vector<std::string>{}, std::vector<std::string>{}),0);
  resourceMessages.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>("E_CTU", "Counter", 1, eventCounter, std::vector<std::string>{"FALSE", "0"}),0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 0, "FALSE"), 0);
  resourceMessages.emplace_back("outputData", std::make_unique<FBDataPayload>("E_CTU", "Counter", 1, "0"), 0);
  addFinalEvents(resourceMessages, eventCounter);

  auto ctfMessages = getEventMessages(CTF_OUTPUT_DIR);

  checkMessages(expectedMessages, ctfMessages);
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
  
  auto resource = getResource(device.get(), paResourceName);

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

CResource* getResource(CDevice* paDevice, CStringDictionary::TStringId paResourceName){
  forte::core::TNameIdentifier id;
  id.pushBack(paResourceName);
  forte::core::TNameIdentifier::CIterator nonConstIterator(id.begin());
  return dynamic_cast<CResource*>(paDevice->getFB(nonConstIterator));
}

}

