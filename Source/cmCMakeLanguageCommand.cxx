/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmCMakeLanguageCommand.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include <cm/string_view>
#include <cmext/string_view>

#include "cmExecutionStatus.h"
#include "cmListFileCache.h"
#include "cmMakefile.h"
#include "cmRange.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"

namespace {
std::array<cm::static_string_view, 12> InvalidCommands{
  { // clang-format off
  "function"_s, "endfunction"_s,
  "macro"_s, "endmacro"_s,
  "if"_s, "elseif"_s, "else"_s, "endif"_s,
  "while"_s, "endwhile"_s,
  "foreach"_s, "endforeach"_s
  } // clang-format on
};
}

bool cmCMakeLanguageCommand(std::vector<cmListFileArgument> const& args,
                            cmExecutionStatus& status)
{
  if (args.empty()) {
    status.SetError("called with incorrect number of arguments");
    return false;
  }

  cmMakefile& makefile = status.GetMakefile();
  cmListFileContext context = makefile.GetExecutionContext();

  bool result = false;

  if (args.empty()) {
    status.SetError("called with incorrect number of arguments");
    return false;
  }

  if (args[0].Value == "CALL") {
    if(args.size() == 1){
      status.SetError("called with incorrect number of arguments");
      return false;
    }

    // First argument is the name of the function to call
    const auto& callCommand = args[1].Value;

    // ensure specified command is valid
    // start/end flow control commands are not allowed
    auto cmd = cmSystemTools::LowerCase(callCommand);
    if (std::find(InvalidCommands.cbegin(), InvalidCommands.cend(), cmd) !=
        InvalidCommands.cend()) {
      status.SetError(cmStrCat("invalid command specified: "_s, callCommand));
      return false;
    }

    cmListFileFunction func;
    func.Name = callCommand;
    func.Line = context.Line;

    // The rest of the arguments are passed to the function call above
    func.Arguments.reserve(args.size() - 2);
    for (size_t i = 2; i < args.size(); ++i) {
      func.Arguments.emplace_back(args[i].Value, args[i].Delim, context.Line);
    }

    result = makefile.ExecuteCommand(func, status);
  } else if (args[0].Value == "EVAL") {
    if (args.size() < 2) {
      status.SetError("called with incorrect number of arguments");
      return false;
    }

    if (args[1].Value != "CODE") {
      auto code_iter =
        std::find_if(args.begin() + 2, args.end(), [](const auto& arg){
          return (arg.Value == "CODE");
        });
      if (code_iter == args.end()) {
        status.SetError("called without CODE argument");
      } else {
        status.SetError(
          "called with unsupported arguments between EVAL and CODE arguments");
      }
      return false;
    }

    const std::string code =
      cmJoin(
        cmMakeRange(
          args.begin() + 2,
          args.end()), " ",
          [](const auto& arg){
            return arg.Value;
        }
      );
    result = makefile.ReadListFileAsString(
      code, cmStrCat(context.FilePath, ":", context.Line, ":EVAL"));
  } else {
    status.SetError("called with unknown meta-operation");
  }

  return result;
}
