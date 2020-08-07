#ifndef CP_PARSER_CTX_H
#define CP_PARSER_CTX_H

#include "rpn.h"
#include "cmListFileCache.h"

struct ParserCtx
{
    cmListFileFunctionExpr function;
    std::vector<cmListFileFunctionExpr> functionList;
    std::string message;

    // used for non-strict argument separation handling
    // enum class ArgToken
    // {
    //     Separation,
    //     BracketArg,
    //     QuotedArg,
    //     UnquotedArg
    // };

    // ArgToken lastArgToken{ArgToken::Separation};
};

#endif // CP_PARSER_CTX_H