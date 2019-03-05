/*******************************************************************************
 * Copyright (c) 2019, fortiss GmbH
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *   Jose Cabral - initial implementation
 *******************************************************************************/

#ifndef _SERVICEREGISTRYENTRY2SERVICEREGISTRYENTRY_H_
#define _SERVICEREGISTRYENTRY2SERVICEREGISTRYENTRY_H_

#include <funcbloc.h>
#include <forte_serviceregistryentry.h>

class FORTE_ServiceRegistryEntry2ServiceRegistryEntry: public CFunctionBlock{
  DECLARE_FIRMWARE_FB(FORTE_ServiceRegistryEntry2ServiceRegistryEntry)

private:
  static const CStringDictionary::TStringId scm_anDataInputNames[];
  static const CStringDictionary::TStringId scm_anDataInputTypeIds[];
  CIEC_ServiceRegistryEntry &IN() {
    return *static_cast<CIEC_ServiceRegistryEntry*>(getDI(0));
  };

  static const CStringDictionary::TStringId scm_anDataOutputNames[];
  static const CStringDictionary::TStringId scm_anDataOutputTypeIds[];
  CIEC_ServiceRegistryEntry &OUT() {
    return *static_cast<CIEC_ServiceRegistryEntry*>(getDO(0));
  };

  static const TEventID scm_nEventREQID = 0;
  static const TForteInt16 scm_anEIWithIndexes[];
  static const TDataIOID scm_anEIWith[];
  static const CStringDictionary::TStringId scm_anEventInputNames[];

  static const TEventID scm_nEventCNFID = 0;
  static const TForteInt16 scm_anEOWithIndexes[];
  static const TDataIOID scm_anEOWith[];
  static const CStringDictionary::TStringId scm_anEventOutputNames[];

  static const SFBInterfaceSpec scm_stFBInterfaceSpec;

   FORTE_FB_DATA_ARRAY(1, 1, 1, 0);

  void executeEvent(int pa_nEIID);

public:
  FUNCTION_BLOCK_CTOR(FORTE_ServiceRegistryEntry2ServiceRegistryEntry){
  };

  virtual ~FORTE_ServiceRegistryEntry2ServiceRegistryEntry(){};

};

#endif //close the ifdef sequence from the beginning of the file

