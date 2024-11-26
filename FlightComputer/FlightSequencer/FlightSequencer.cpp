// ======================================================================
// \title  TransmitterInterface.cpp
// \brief  cpp file for TransmitterInterface component implementation class
// ======================================================================

/*** INCLUDES
 * ***************************************************************************************************************************/

#include "FlightSequencer.hpp"
#include "FlightComputer/Common/Common.hpp"
#include "FlightComputer/FlightSequencer/FlightSequencer_FlightSMStatesEnumAc.hpp"
#include "FlightComputer/FlightSequencer/FppConstantsAc.hpp"
#include "FlightSM.hpp"
#include "Fw/Sm/SmSignalBuffer.hpp"
#include "Fw/Time/Time.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include <FlightComputer/FlightSequencer/FlightSequencer.hpp>
#include <Fw/Logger/Logger.hpp> //To log debug text
#include <Fw/Types/Assert.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>

namespace FlightComputer {

FlightSequencer ::FlightSequencer(const char *const compName)
    : FlightSequencerComponentBase(compName), FlightSM_Interface(),
      flightSM(this), signalLock() {}

FlightSequencer ::~FlightSequencer() {}

// Check if T-burn time is reached using Fw::Time
bool FlightSequencer::FlightSM_isTBurnReached(
    const FwEnumStoreType stateMachineId) {

  Fw::Time currentTime = Fw::Time(timeCnt++, 0); // Get the current time
  Fw::Time burnDuration = Fw::Time(250, 0);      // Burn time (tburn) in seconds
  return currentTime >= burnDuration; // Check if burn time is reached
}

void FlightSequencer::FlightSM_checkLowAltReached(
    const FwEnumStoreType stateMachineId) {
  if (status.getaltitudeM() < 50) {
    Fw::SmSignalBuffer data;
    signalLock.lock();

    lastSignal = FlightSM_Signals::TERMINATE_SIG;
    flightSM.update(this->stateMachineId, lastSignal, data);

    signalLock.unLock();
  }
}

// Initialize flight status at the beginning of the flight using Fw::Time
void FlightSequencer::FlightSM_initFlightStatus(
    const FwEnumStoreType stateMachineId) {
  // Reset the flight status to start simulation
  Fw::Logger::log("Init flight status\n");
  timeCnt = 0;

  status.set(false, 0.0, 0.0,
             FlightSequencer_FlightSMStates::IDLE); // Reset time, engine state,
                                                    // altitude, and velocity
}

// Engage thrust (transition from Idle to Powered flight)
void FlightSequencer::FlightSM_engageThrust(
    const FwEnumStoreType stateMachineId) {
  Fw::Logger::log("Engaging thrust\n");
  status.setisEngineOn(true); // Set engine ON
}

// Disengage thrust (transition from Powered flight to Ballistic flight)
void FlightSequencer::FlightSM_disengageThrust(
    const FwEnumStoreType stateMachineId) {
  Fw::Logger::log("Disengaging thrust\n");
  status.setisEngineOn(false); // Set engine OFF
}

// Update flight status using Fw::Time for time intervals
void FlightSequencer::FlightSM_updateFlightStatus(
    const FwEnumStoreType stateMachineId) {
  // Get current status parameters
  F32 velocity = status.getvelocityMS(); // Get current velocity
  F32 altitude = status.getaltitudeM();  // Get current altitude

  Fw::Time dt = Fw::Time(1, 0); // Define a 1-second timestep

  if (status.getisEngineOn()) {
    // Powered flight phase
    velocity += (FlightSequencer_thrustN / FlightSequencer_massKg -
                 FlightSequencer_gravityMSS) *
                dt.getSeconds(); // Calculate velocity during powered flight
  } else {
    // Ballistic flight phase (free fall)
    velocity += (-FlightSequencer_gravityMSS) *
                dt.getSeconds(); // Calculate velocity during free fall
  }

  altitude +=
      velocity * dt.getSeconds(); // Update altitude based on new velocity

  // Update time, velocity, and altitude in the status
  // status.setflightTimeS(status.getflightTimeS() + dt.getSeconds()); //
  // Increment current time by dt
  status.setvelocityMS(velocity); // Set updated velocity
  status.setaltitudeM(altitude);  // Set updated altitude
}

void FlightSequencer ::init(const NATIVE_INT_TYPE queueDepth,
                            const NATIVE_INT_TYPE instance) {
  FlightSequencerComponentBase::init(queueDepth, instance);
  flightSM.init(this->stateMachineId);
  Fw::Logger::log("SM state on init %d\n", flightSM.state);
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

bool FlightSequencer ::updateTlms() {
  tlmWrite_flightStatus(status);
  return true;
}

void FlightSequencer ::run_handler(const NATIVE_INT_TYPE portNum,
                                   NATIVE_UINT_TYPE context) {

  FW_CHECK(updateTlms(), "Failed to update tlms");

  Fw::CmdResponse ret = Fw::CmdResponse::OK;

  Fw::SmSignalBuffer data;
  signalLock.lock();
  // Once every few calls, send the TBURN_CHECK_SIG signal
  if (signalCounter++ >= 2) {
    lastSignal = FlightSM_Signals::TBURN_CHECK_INTERVAL_SIG;
    signalCounter =
        0; // Reset the counter after sending the TBURN_CHECK_SIG signal
  } else {
    lastSignal = FlightSM_Signals::UPDATE_INTERVAL_SIG;
  }

  // FIXME Using static_cast to convert the integer to the enum type
  FlightSequencer_FlightSMStates::T stateEnum =
      static_cast<FlightSequencer_FlightSMStates::T>(flightSM.state);

  status.setcurrentState(stateEnum);

  signalLock.unlock();

  flightSM.update(this->stateMachineId, lastSignal, data);

  FW_CHECK(ret == Fw::CmdResponse::OK, "Run Failed, aborting",
           this->TERMINATE_cmdHandler(0, 10))
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void FlightSequencer ::IGNITE_cmdHandler(const FwOpcodeType opCode,
                                         const U32 cmdSeq) {
  signalLock.lock();

  Fw::SmSignalBuffer data;
  lastSignal = FlightSM_Signals::IGNITE_SIG;
  flightSM.update(this->stateMachineId, lastSignal, data);

  signalLock.unLock();

  cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void FlightSequencer ::TERMINATE_cmdHandler(const FwOpcodeType opCode,
                                            const U32 cmdSeq) {
  Fw::SmSignalBuffer data;
  signalLock.lock();

  lastSignal = FlightSM_Signals::TERMINATE_SIG;
  flightSM.update(this->stateMachineId, lastSignal, data);

  signalLock.unLock();

  cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

} // end namespace FlightComputer
