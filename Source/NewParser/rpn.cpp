#include "rpn.h"

#include <algorithm>

#include "cmStringAlgorithms.h"
#include "cmMakefile.h"
#include "cmake.h"
#include "cmSystemTools.h"
#include "cmExecutionStatus.h"

namespace rpn
{

struct EvaluationContext
{
    EvaluationContext(cmMakefile& makefile, cmListFileFunction& function) 
        : makefile{makefile}, function{function}
    {
    }

    cmMakefile& makefile;
    cmListFileFunction& function;

    // stack of expression results
    results_type results;

    // stack of lengths of expression results
    std::vector<std::size_t> resultsCount;
};

RPNExpression::RPNExpression(const RPNExpression& other)
{
    rpnExprList.reserve(other.rpnExprList.size());

    for(const auto& expr: other.rpnExprList)
    {
        rpnExprList.push_back(std::unique_ptr<IExpression>(expr->Clone()));
    }
}

RPNExpression& RPNExpression::operator=(const RPNExpression& other)
{
    auto tmp{other};
    rpnExprList.swap(tmp.rpnExprList);
    return *this;
}

void RPNExpression::Push(expression_ptr expr)
{
    rpnExprList.push_back(std::move(expr));
}

bool RPNExpression::Evaluate(
    cmMakefile& makefile, cmListFileFunction& function) const
{
    EvaluationContext context{makefile, function};

    return std::all_of(
        std::cbegin(rpnExprList),
        std::cend(rpnExprList),
        [&context](const auto& expr){
            return expr->Evaluate(context);
        });
}

void RPNExpression::Clear() noexcept
{
    rpnExprList.clear();
}

void RPNExpression::ResolveNormalVarRefs(const vars_map& vars)
{
    if(rpnExprList.empty())
    {
        return;
    }

    auto b = std::begin(rpnExprList);
    auto begin = std::begin(rpnExprList) + 1;
    // std::end() is required here because it's invalidated after erase()
    for(; begin != std::end(rpnExprList); ++begin)
    {
        auto &expr = *begin;
        if((expr->GetType() == ExprType::NormalVarRef)
            && (expr->GetArity() == 1))
        {
            auto prev = begin - 1;
            if((*prev)->GetType() == ExprType::String)
            {
                assert((*prev)->GetString());

                const auto& varName = *((*prev)->GetString());
                const auto search = vars.find(varName);
                if(search != std::cend(vars))
                {
                    *prev = std::unique_ptr<StringExpression>(
                        new StringExpression(search->second));

                    // Effective iteration step will be 2, which is intended.
                    // Otherwise we can get situation: ${${var}}, if var = var,
                    // we need to replace it only once.
                    begin = rpnExprList.erase(begin);
                    if(begin == std::end(rpnExprList))
                    {
                        --begin;
                    }
                }
            }
        }
    }
}

StringExpression::StringExpression(std::string str)
    : str{std::move(str)}
{
}

bool StringExpression::Evaluate(EvaluationContext& context) const
{
    context.results.emplace_back(str, cmListFileArgument::Unquoted, 0);
    context.resultsCount.push_back(1);
    return true;
}

ExprType StringExpression::GetType() const noexcept
{
    return ExprType::String;
}

const std::string* StringExpression::GetString() const
{
    return &str;
}

BracketArgExpression::BracketArgExpression(std::string str, const line_t line)
    : str{std::move(str)}, line{line}
{
}

bool BracketArgExpression::Evaluate(EvaluationContext& context) const
{
    context.results.emplace_back(str, cmListFileArgument::Bracket, line);
    context.resultsCount.push_back(1);
    return true;
}

template<typename T>
ConcatExpression<T>::ConcatExpression(const std::size_t arity) : arity{arity}
{
}

std::string::size_type GetTotalLength(
    const_results_iterator begin, const_results_iterator end)
{
    std::string::size_type result{};
    std::for_each(begin, end, [&result](const auto& str){
        result += str.Value.size();
    });
    return result;
}

template<typename T>
bool ConcatExpression<T>::Evaluate(EvaluationContext& context) const
{
    if(arity > 1)
    {
        const auto concatBegin = std::end(context.results) - arity;
        const auto concatEnd = std::end(context.results);
        
        std::string result;
        result.reserve(GetTotalLength(concatBegin, concatEnd));

        std::for_each(
            concatBegin,
            concatEnd,
            [&result](const auto& str) {
                result += str.Value;
            });

        context.results.erase(concatBegin, concatEnd);
        context.resultsCount.erase(
            std::end(context.resultsCount) - arity,
            std::end(context.resultsCount));

        context.results.emplace_back(
            std::move(result), cmListFileArgument::Unquoted, 0);
        context.resultsCount.push_back(1);
    }

    return true;
}

template<typename T>
std::size_t ConcatExpression<T>::GetArity() const noexcept
{
    return arity;
}

QuotedArgExpression::QuotedArgExpression(
    const std::size_t arity, const line_t line)
        : ConcatExpression{arity}, line{line}
{
}

bool QuotedArgExpression::Evaluate(EvaluationContext& context) const
{
    ConcatExpression::Evaluate(context);
    context.results.back().Delim = cmListFileArgument::Quoted;
    context.results.back().Line = line;
    return true;
}

UnquotedArgExpression::UnquotedArgExpression(
    const std::size_t arity, const line_t line)
    : ConcatExpression{arity}, line{line}
{
}

bool UnquotedArgExpression::Evaluate(EvaluationContext& context) const
{
    ConcatExpression::Evaluate(context);
    
    const auto prev = std::move(context.results.back());
    context.results.pop_back();
    context.resultsCount.pop_back();
    
    auto expandedArgs = cmExpandedList(prev.Value);
    context.resultsCount.push_back(expandedArgs.size());
    for (auto& arg : expandedArgs) {
        context.results.emplace_back(
            std::move(arg), cmListFileArgument::Unquoted, line);
    }

    return true;
}

template<typename T>
VarRefExpression<T>::VarRefExpression(const std::size_t arity) : base_t{arity}
{
}

template<typename T>
bool VarRefExpression<T>::Evaluate(EvaluationContext& context) const
{
    base_t::Evaluate(context);

    const auto& variableName = context.results.back().Value;
    auto variableValue =
        static_cast<const T&>(*this).GetVariableValue(
            context, variableName);

    context.results.pop_back();
    context.results.emplace_back(
        std::move(variableValue), cmListFileArgument::Unquoted, 0);
    context.resultsCount.back() = 1;

    return true;
}

NormalVarRefExpression::NormalVarRefExpression(
    const std::size_t arity, const line_t line)
    : VarRefExpression{arity}, line{line}
{
}

ExprType NormalVarRefExpression::GetType() const noexcept
{
    return ExprType::NormalVarRef;
}

std::string NormalVarRefExpression::GetVariableValue(
    EvaluationContext& context, const std::string& name) const
{
    static const std::string currentLine{"CMAKE_CURRENT_LIST_LINE"};
    // CMake also checks filename here
    if(name == currentLine)
    {
        return std::to_string(line);
    }

    const auto value = context.makefile.GetDef(name);
    if(value)
    {
        return *value;
    }
    return {};
}

CacheVarRefExpression::CacheVarRefExpression(const std::size_t arity)
    : VarRefExpression{arity}
{
}

std::string CacheVarRefExpression::GetVariableValue(
    EvaluationContext& context, const std::string& name) const
{
    auto state = context.makefile.GetCMakeInstance()->GetState();
    const auto value = state->GetCacheEntryValue(name);

    if(value)
    {
        return *value;
    }
    return {};
}

EnvVarRefExpression::EnvVarRefExpression(const std::size_t arity)
    : VarRefExpression{arity}
{
}

std::string EnvVarRefExpression::GetVariableValue(
    EvaluationContext& /*context*/, const std::string& name) const
{
    std::string value;
    cmSystemTools::GetEnv(name, value);
    return value;
}

CommandCallExpression::CommandCallExpression(const std::size_t arity, const line_t line)
    : arity{arity}, line{line}
{
}

bool CommandCallExpression::Evaluate(EvaluationContext& context) const
{
    const auto evaluatedArity = EvaluateArity(context.resultsCount);
    auto argsEnd = std::end(context.results);
    auto argsBegin = argsEnd - evaluatedArity;

    const auto res = CallCommand(
        argsBegin, argsEnd, context);

    context.results.erase(argsBegin, argsEnd);
    return res;
}

std::size_t CommandCallExpression::EvaluateArity(
    std::vector<std::size_t>& resultsCount) const
{
    std::size_t evaluatedArity{};
    for(std::size_t i{}; i != arity; i++)
    {
        evaluatedArity += resultsCount.back();
        resultsCount.pop_back();
    }
    return evaluatedArity;
}

bool CommandCallExpression::CallCommand(
    results_iterator argsBegin,
    results_iterator argsEnd,
    EvaluationContext& context) const
{
    const auto& name = *argsBegin;
    std::advance(argsBegin, 1);

    auto& lff = context.function;
    lff.Arguments.clear();
    lff.Arguments.reserve(std::distance(argsBegin, argsEnd));
    
    lff.Name = name.Value;
    lff.Line = line;
    std::for_each(
        argsBegin, argsEnd, [&lff](const auto& arg) {
            lff.Arguments.emplace_back(arg);
        });

    return true;
}

bool CommandRefExpression::Evaluate(EvaluationContext& context) const
{
    cmExecutionStatus status{context.makefile};
    const auto success = context.makefile.ExecuteCommand(
        context.function, status);
    if(success)
    {
        context.results.emplace_back(
            status.ReleaseReturnValue(), cmListFileArgument::Unquoted, 0);
        context.resultsCount.push_back(1);
    }

    return success;
}

} // namespace rpn