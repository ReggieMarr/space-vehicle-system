// ======================================================================
// \title  CCSDSTester.hpp
// \author Reginald Marr
// \brief  hpp file for CCSDSTester component implementation class
// ======================================================================

#ifndef FlightComputer_CCSDSTester_HPP
#define FlightComputer_CCSDSTester_HPP

#include "Drv/ByteStreamDriverModel/SendStatusEnumAc.hpp"
#include "Drv/Ip/IpSocket.hpp"
#include "FlightComputer/CCSDSTester/CCSDSTesterComponentAc.hpp"
#include "Fw/Cmd/CmdString.hpp"
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
  Fw::ComBuffer com;
  void bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                            Fw::Buffer &fwBuffer);

  //!  \brief component command buffer handler
  //!
  //!  The command buffer handler is called to submit a new
  //!  command packet to be decoded
  //!
  //!  \param portNum the number of the incoming port.
  //!  \param data the buffer containing the command.
  //!  \param context a user value returned with the status
  void seqCmdBuff_handler(NATIVE_INT_TYPE portNum, Fw::ComBuffer &data,
                          U32 context);

  void comStatusIn_handler(FwIndexType portNum,   //!< The port number
                           Fw::Success &condition //!< Condition success/failure
  );

  // Commands
  void PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq);
  void MESSAGE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq,
                          const Fw::CmdStringArg &str1);
};
} // namespace FlightComputer

#endif
