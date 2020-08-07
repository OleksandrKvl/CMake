#ifndef CP_RPN_H
#define CP_RPN_H

#include <memory>
#include <vector>

#include "cmListFileArgument.h"

class cmMakefile;
struct cmListFileFunction;

namespace rpn
{
using results_type = std::vector<cmListFileArgument>;
using results_iterator = results_type::iterator;
using const_results_iterator = results_type::const_iterator;

struct EvaluationContext;

class IExpression
{
public:
    virtual ~IExpression() = default;
    virtual bool Evaluate(EvaluationContext& context) const = 0;
    virtual IExpression* Clone() const = 0;
};

template<typename T>
class CloneableExpression : public IExpression
{
public:
    IExpression* Clone() const override
    {
        return new T(static_cast<const T&>(*this));
    }
};

class RPNExpression
{
public:
    using expression_ptr = std::unique_ptr<IExpression>;

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

private:
    std::vector<expression_ptr> rpnExprList;
};

class StringExpression : public CloneableExpression<StringExpression>
{
public:
    explicit StringExpression(std::string str);

    bool Evaluate(EvaluationContext& context) const final;

private:
    const std::string str;
};

class BracketArgExpression : public CloneableExpression<BracketArgExpression>
{
public:
    bool Evaluate(EvaluationContext& context) const final;
};

template<typename T>
class ConcatExpression : public CloneableExpression<T>
{
public:
    explicit ConcatExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const override;

protected:
    std::size_t GetArity() const noexcept;

private:
    const std::size_t arity{};
};

class QuotedArgExpression : public ConcatExpression<QuotedArgExpression>
{
public:
    explicit QuotedArgExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const final;
};

class UnquotedArgExpression : public ConcatExpression<UnquotedArgExpression>
{
public:
    explicit UnquotedArgExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const final;
};

template<typename T>
class VarRefExpression : public ConcatExpression<VarRefExpression<T>>
{
public:
    using base_t = ConcatExpression<VarRefExpression<T>>;

    explicit VarRefExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const override;
};

class NormalVarRefExpression : public VarRefExpression<NormalVarRefExpression>
{
public:
    explicit NormalVarRefExpression(const std::size_t arity);

    std::string GetVariableValue(
        EvaluationContext& context, const std::string& name) const;
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

class CommandRefExpression : public CloneableExpression<CommandRefExpression>
{
public:
    bool Evaluate(EvaluationContext& context) const final;
};

class CommandCallExpression : public CloneableExpression<CommandCallExpression>
{
public:
    explicit CommandCallExpression(const std::size_t arity);

    bool Evaluate(EvaluationContext& context) const final;

    std::size_t EvaluateArity(std::vector<std::size_t>& resultsCount) const;

    bool CallCommand(
        results_iterator argsBegin,
        results_iterator argsEnd,
        EvaluationContext& context) const;

private:
    const std::size_t arity{};
};
} // namespace rpn

#endif // CP_RPN_H