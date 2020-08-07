#ifndef CP_CMAKE_PARSER_H
#define CP_CMAKE_PARSER_H

#include "cmake_scanner.h"
#include "parser_ctx.h"
#include "parser.h"

class CMakeParser
{
public:
    CMakeParser() : parser{scanner.Raw(), parserCtx}
    {
    }

    int Parse()
    {
        const auto parseErr = parser.parse();
        return parseErr;
    }

    void SetInputFile(const std::string& path)
    {
        scanner.SetInputFile(path);
    }

    void SetInputString(
        const std::string& str, const std::string& virtualFileName)
    {
        scanner.SetInputString(str, virtualFileName);   
    }

    enum class DebugMode
    {
        Disabled,
        Parser,
        Scanner,
        Full
    };

    void SetDebugMode(const DebugMode mode)
    {
        switch(mode)
        {
        case DebugMode::Disabled:
            parser.set_debug_level(0);
            scanner.SetDebug(false);
            break;
        case DebugMode::Parser:
            parser.set_debug_level(1);
            break;
        case DebugMode::Scanner:
            scanner.SetDebug(true);
            break;
        case DebugMode::Full:
            parser.set_debug_level(1);
            scanner.SetDebug(true);
            break;
        default:
            break;
        }
    }

    ParserCtx& GetCtx()
    {
        return parserCtx;
    }

private:
    CMakeScanner scanner;
    ParserCtx parserCtx;
    yy::parser parser;
};

#endif // CP_CMAKE_PARSER_H