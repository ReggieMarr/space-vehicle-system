// ======================================================================
// \title  CCSDSTester.hpp
// \author user
// \brief  hpp file for CCSDSTester component implementation class
// ======================================================================

#ifndef FlightComputer_CCSDSTester_HPP
#define FlightComputer_CCSDSTester_HPP

#include "FlightComputer/CCSDSTester/CCSDSTesterComponentAc.hpp"
#include "Fw/Logger/Logger.hpp"

namespace FlightComputer {

class CCSDSTester : public CCSDSTesterComponentBase {

public:
  // ----------------------------------------------------------------------
  // Component construction and destruction
  // ----------------------------------------------------------------------

  //! Construct CCSDSTester object
  CCSDSTester(const char *const compName //!< The component name
  );

  //! Destroy CCSDSTester object
  ~CCSDSTester();

private:
  void bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                                          Fw::Buffer &fwBuffer) {
    Fw::Logger::log("Handling ccsds buffer");
    // Pass through for now
    this->bufferSendOut_out(0, fwBuffer);
  }
  //!  \brief component command buffer handler
  //!
  //!  The command buffer handler is called to submit a new
  //!  command packet to be decoded
  //!
  //!  \param portNum the number of the incoming port.
  //!  \param data the buffer containing the command.
  //!  \param context a user value returned with the status
  void seqCmdBuff_handler(NATIVE_INT_TYPE portNum, Fw::ComBuffer &data,
                          U32 context) {};
};

} // namespace FlightComputer

#endif
