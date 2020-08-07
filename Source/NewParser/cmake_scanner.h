#ifndef CP_CMAKE_SCANNER_H
#define CP_CMAKE_SCANNER_H

#include <stdexcept>

#ifdef _WIN32
#include "cmsys/Encoding.h"
#endif

#include "scanner_ctx.h"
#define YY_NO_UNISTD_H 1
#include "scanner.h"

class CMakeScanner
{
public:
    CMakeScanner()
    {
        if(yylex_init(&yyscanner))
        {
            throw std::runtime_error{"yylex_init() error"};
        }

        if(yylex_init_extra(&scannerCtx, &yyscanner))
        {
            throw std::runtime_error{"yylex_init_extra() error"};
        }
    }

    ~CMakeScanner()
    {
        yylex_destroy(yyscanner);

        if(file)
        {
            fclose(file);
        }

        if(stringBuf)
        {
            yy_delete_buffer(stringBuf, yyscanner);
        }
    }

    void SetDebug(const bool enable)
    {
        yyset_debug(enable, yyscanner);
    }

    void SetInputFile(const std::string& path)
    {
#ifdef _WIN32
        wchar_t* wname = cmsysEncoding_DupToWide(name);
        file = _wfopen(wname, L"rb");
        free(wname);
#else
        file = fopen(path.c_str(), "rb");
#endif
        if(!file)
        {
            std::string error{"Cannot open file "};
            error += path;
            throw std::runtime_error{error};
        }
        scannerCtx.SetInputFileName(path);
        yyset_in(file, yyscanner);
    }

    void SetInputString(
        const std::string& str, const std::string& virtualFileName)
    {
        stringBuf = yy_scan_bytes(str.c_str(), str.size(), yyscanner);
        scannerCtx.SetInputFileName(virtualFileName);
    }

    yyscan_t Raw() const noexcept
    {
        return yyscanner;
    }

private:
    yyscan_t yyscanner{};
    ScannerCtx scannerCtx;
    FILE* file{};
    YY_BUFFER_STATE stringBuf{};
};

#endif // CP_CMAKE_SCANNER_H