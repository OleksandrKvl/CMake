/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmMacroCommand.h"

#include <cstdio>
#include <utility>

#include <cm/memory>
#include <cm/string_view>
#include <cmext/algorithm>
#include <cmext/string_view>

#include "cmExecutionStatus.h"
#include "cmFunctionBlocker.h"
#include "cmListFileCache.h"
#include "cmMakefile.h"
#include "cmPolicies.h"
#include "cmRange.h"
#include "cmState.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"

namespace {

// define the class for macro commands
class cmMacroHelperCommand
{
public:
  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  bool operator()(std::vector<cmListFileArgument> const& args,
                  cmExecutionStatus& inStatus) const;

  std::vector<std::string> Args;
  std::vector<cmListFileFunctionExpr> Functions;
  cmPolicies::PolicyMap Policies;
  std::string FilePath;
};

bool cmMacroHelperCommand::operator()(
  std::vector<cmListFileArgument> const& args,
  cmExecutionStatus& inStatus) const
{
  cmMakefile& makefile = inStatus.GetMakefile();

  // make sure the number of arguments passed is at least the number
  // required by the signature
  if (args.size() < this->Args.size() - 1) {
    std::string errorMsg =
      cmStrCat("Macro invoked with incorrect arguments for macro named: ",
               this->Args[0]);
    inStatus.SetError(errorMsg);
    return false;
  }

  cmMakefile::MacroPushPop macroScope(&makefile, this->FilePath,
                                      this->Policies);

  // set the value of argc
  std::string argcDef = std::to_string(args.size());

  auto getArgValue = [](const auto& arg){
    return arg.Value;
  };
  auto eit = args.begin() + (this->Args.size() - 1);
  std::string expandedArgn = cmJoin(
    cmMakeRange(eit, args.end()), ";", getArgValue);
  std::string expandedArgv = cmJoin(args, ";", getArgValue);
  
  rpn::RPNExpression::vars_map macroVars;

  // create vars for formal arguments, ARGC, ARGV, ARGN, ARGV#
  for(std::size_t i{}; i != args.size(); i++)
  {
    const auto& argValue = args[i].Value;
    
    std::string argvName{"ARGV"};
    argvName += std::to_string(i);
    macroVars.emplace(argvName, argValue);

    if(i+1 < this->Args.size())
    {
      const auto& argName = this->Args[i + 1];
      macroVars.emplace(argName, argValue);
    }
  }

  macroVars.emplace("ARGC", argcDef);
  macroVars.emplace("ARGV", expandedArgv);
  macroVars.emplace("ARGN", expandedArgn);

  // Invoke all the functions that were collected in the block.
  for (auto const& funcExpr : this->Functions) {
    auto exprCopy = funcExpr;
    exprCopy.rpnExpr.ResolveNormalVarRefs(macroVars);
    cmExecutionStatus status(makefile);
    if (!makefile.ExecuteCommand(exprCopy, status) || status.GetNestedError()) {
      // The error message should have already included the call stack
      // so we do not need to report an error here.
      macroScope.Quiet();
      inStatus.SetNestedError();
      return false;
    }
    if (status.GetReturnInvoked()) {
      inStatus.SetReturnInvoked();
      inStatus.SetReturnValue(status.ReleaseReturnValue());
      return true;
    }
    if (status.GetBreakInvoked()) {
      inStatus.SetBreakInvoked();
      return true;
    }
  }

  return true;
}

class cmMacroFunctionBlocker : public cmFunctionBlocker
{
public:
  cm::string_view StartCommandName() const override { return "macro"_s; }
  cm::string_view EndCommandName() const override { return "endmacro"_s; }

  bool ArgumentsMatch(cmListFileFunction const&,
                      cmMakefile& mf) const override;

  bool Replay(std::vector<cmListFileFunctionExpr> functions,
              cmExecutionStatus& status) override;

  std::vector<std::string> Args;
};

bool cmMacroFunctionBlocker::ArgumentsMatch(cmListFileFunction const& lff,
                                            cmMakefile& mf) const
{
  return lff.Arguments.empty() || lff.Arguments[0].Value == this->Args[0];
}

bool cmMacroFunctionBlocker::Replay(std::vector<cmListFileFunctionExpr> functions,
                                    cmExecutionStatus& status)
{
  cmMakefile& mf = status.GetMakefile();
  mf.AppendProperty("MACROS", this->Args[0]);
  // create a new command and add it to cmake
  cmMacroHelperCommand f;
  f.Args = this->Args;
  f.Functions = std::move(functions);
  f.FilePath = this->GetStartingContext().FilePath;
  mf.RecordPolicies(f.Policies);
  mf.GetState()->AddScriptedCommand(this->Args[0], std::move(f));
  return true;
}
}

bool cmMacroCommand(std::vector<std::string> const& args,
                    cmExecutionStatus& status)
{
  if (args.empty()) {
    status.SetError("called with incorrect number of arguments");
    return false;
  }

  // create a function blocker
  {
    auto fb = cm::make_unique<cmMacroFunctionBlocker>();
    cm::append(fb->Args, args);
    status.GetMakefile().AddFunctionBlocker(std::move(fb));
  }
  return true;
}
