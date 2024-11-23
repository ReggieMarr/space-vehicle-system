
#ifndef FlightSequencer_HPP
#define FlightSequencer_HPP

#include "FlightComputer/FlightSequencer/FlightSM.hpp"
#include "FlightComputer/FlightSequencer/FlightSequencerComponentAc.hpp"
#include "FlightComputer/FlightSequencer/FlightSequencer_FlightSMStatesEnumAc.hpp"
#include "FlightComputer/FlightSequencer/FlightSequencer_statusSerializableAc.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Os/Mutex.hpp"

namespace FlightComputer {
class FlightSequencer : public FlightSequencerComponentBase,
                        public FlightSM_Interface {

public:
  // ----------------------------------------------------------------------
  // Construction, initialization, and destruction
  // ----------------------------------------------------------------------

  //! Construct object FlightSequencer
  //!
  FlightSequencer(const char *const compName /*!< The component name*/
  );

  //! Initialize object TransmitterInterface
  //!
  void init(const NATIVE_INT_TYPE queueDepth,  /*!< The queue depth*/
            const NATIVE_INT_TYPE instance = 0 /*!< The instance number*/
  );

  //! Destroy object FlightSequencer
  //!
  ~FlightSequencer();

  // TODO we should see if this definition is neccessary
  virtual bool FlightSM_isTBurnReached(const FwEnumStoreType stateMachineId);
  virtual void FlightSM_engageThrust(const FwEnumStoreType stateMachineId);
  virtual void FlightSM_disengageThrust(const FwEnumStoreType stateMachineId);
  virtual void FlightSM_initFlightStatus(const FwEnumStoreType stateMachineId);
  virtual void
  FlightSM_updateFlightStatus(const FwEnumStoreType stateMachineId);
  virtual void
  FlightSM_checkLowAltReached(const FwEnumStoreType stateMachineId);

PRIVATE:
  FwSizeType signalCounter;
  FlightSequencer_status status; // = {0, false, 0, 0};
  FlightSM flightSM;
  FlightSM_Signals lastSignal = FlightSM_Signals::TERMINATE_SIG;
  FwEnumStoreType stateMachineId = 1;
  Os::Mutex signalLock;
  U32 timeCnt = 0;

  bool updateTlms();

  //! Handler implementation for run
  //!
  void run_handler(const NATIVE_INT_TYPE portNum, /*!< The port number*/
                   NATIVE_UINT_TYPE context       /*!<
                     The call order
                     */
  );
  void IGNITE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq);
  void TERMINATE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq);
};

} // end namespace FlightComputer
#endif
