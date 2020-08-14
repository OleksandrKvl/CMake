#ifndef CP_RPN_H
#define CP_RPN_H

#include <memory>
#include <vector>
#include <unordered_map>

#include "cmListFileArgument.h"

class cmMakefile;
struct cmListFileFunction;

namespace rpn
{
using results_type = std::vector<cmListFileArgument>;
using results_iterator = results_type::iterator;
using const_results_iterator = results_type::const_iterator;

using line_t = decltype(cmListFileArgument::Line);

struct EvaluationContext;

// Since this is used only for macro var replacement hack, we don't need all
// types for now
enum class ExprType
{
    String,
    NormalVarRef,
    Other
};

class IExpression
{
public:
    virtual ~IExpression() = default;
    virtual bool Evaluate(EvaluationContext& context) const = 0;
    virtual IExpression* Clone() const = 0;
    virtual ExprType GetType() const noexcept = 0;
    virtual const std::string* GetString() const = 0;
    virtual std::size_t GetArity() const noexcept = 0;
};

template<typename T>
class ExpressionBase : public IExpression
{
public:
    IExpression* Clone() const override
    {
        return new T(static_cast<const T&>(*this));
    }
    
    // These functions are used for a macro vars replacement hack.
    // They're implemented only partially.
    ExprType GetType() const noexcept override
    {
        return ExprType::Other;
    }

    const std::string* GetString() const override
    {
        return {};
    }

    std::size_t GetArity() const noexcept override
    {
        return 0;
    }
};

class RPNExpression
{
public:
    using expression_ptr = std::unique_ptr<IExpression>;
    using vars_map = std::unordered_map<std::string, std::string>;

    RPNExpression() = default;
    RPNExpression(const RPNExpression& other);
    RPNExpression& operator=(const RPNExpression& other);
    RPNExpression(RPNExpression&&) = default;
    RPNExpression& operator=(RPNExpression&&) = default;

    void Push(expression_ptr expr);

    template<typename ExprT, typename... ExprArgs>
    void Push(ExprArgs&&... args)
    {
        // Push(std::make_unique<ExprT>(std::forward<ExprArgs>(args)...));
        Push(std::unique_ptr<ExprT>(new ExprT(std::forward<ExprArgs>(args)...)));
    }

    bool Evaluate(cmMakefile& makefile, cmListFileFunction& function) const;

    void Clear() noexcept;

    void ResolveNormalVarRefs(const vars_map& vars);

private:
    std::vector<expression_ptr> rpnExprList;
};

class StringExpression : public ExpressionBase<StringExpression>
{
public:
    explicit StringExpression(std::string str);

    bool Evaluate(EvaluationContext& context) const final;

    ExprType GetType() const noexcept final;

    const std::string* GetString() const final;

private:
    const std::string str;
};

class BracketArgExpression : public ExpressionBase<BracketArgExpression>
{
public:
    explicit BracketArgExpression(std::string str, const line_t line);

    bool Evaluate(EvaluationContext& context) const final;

private:
    const std::string str;
    const line_t line{};
};

template<typename T>
class ConcatExpression : public ExpressionBase<T>
{
public:
    explicit ConcatExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const override;

    std::size_t GetArity() const noexcept final;

private:
    const std::size_t arity{};
};

class QuotedArgExpression : public ConcatExpression<QuotedArgExpression>
{
public:
    explicit QuotedArgExpression(const std::size_t arity, const line_t line);

    bool Evaluate(EvaluationContext& context) const final;

private:
    const line_t line{};
};

class UnquotedArgExpression : public ConcatExpression<UnquotedArgExpression>
{
public:
    explicit UnquotedArgExpression(const std::size_t arity, const line_t line);

    bool Evaluate(EvaluationContext& context) const final;

private:
    const line_t line{};
};

template<typename T>
class VarRefExpression : public ConcatExpression<T>
{
public:
    using base_t = ConcatExpression<T>;

    explicit VarRefExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const override;
};

class NormalVarRefExpression : public VarRefExpression<NormalVarRefExpression>
{
public:
    explicit NormalVarRefExpression(const std::size_t arity, const line_t line);

    ExprType GetType() const noexcept final;

    std::string GetVariableValue(
        EvaluationContext& context, const std::string& name) const;

private:
    const line_t line{};
};

class CacheVarRefExpression : public VarRefExpression<CacheVarRefExpression>
{
public:
    explicit CacheVarRefExpression(const std::size_t arity);

    std::string GetVariableValue(
        EvaluationContext& context, const std::string& name) const;
};

class EnvVarRefExpression : public VarRefExpression<EnvVarRefExpression>
{
public:
    explicit EnvVarRefExpression(const std::size_t arity);

    std::string GetVariableValue(
        EvaluationContext& /*context*/, const std::string& name) const;
};

class CommandRefExpression : public ExpressionBase<CommandRefExpression>
{
public:
    bool Evaluate(EvaluationContext& context) const final;
};

class CommandCallExpression : public ExpressionBase<CommandCallExpression>
{
public:
    explicit CommandCallExpression(const std::size_t arity, const line_t line);

    bool Evaluate(EvaluationContext& context) const final;

    std::size_t EvaluateArity(std::vector<std::size_t>& resultsCount) const;

    bool CallCommand(
        results_iterator argsBegin,
        results_iterator argsEnd,
        EvaluationContext& context) const;

private:
    const std::size_t arity{};
    const line_t line{};
};
} // namespace rpn

#endif // CP_RPN_H