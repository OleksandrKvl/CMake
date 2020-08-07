/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmReturnCommand.h"

#include "cmExecutionStatus.h"
#include "cmRange.h"
#include "cmStringAlgorithms.h"

// cmReturnCommand
bool cmReturnCommand(std::vector<std::string> const& args,
                     cmExecutionStatus& status)
{
  status.SetReturnValue(cmJoin(cmMakeRange(args), ";"));
  status.SetReturnInvoked();
  return true;
}
