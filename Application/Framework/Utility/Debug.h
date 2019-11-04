#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <crtdbg.h>
#include <assert.h>
#include <wtypes.h>
#include "StringUtil.h"
#include "HrException.h"

#ifdef _DEBUG
/**
* @def MY_ASSERTION
* @brief アサーション
* @param[in] expr 条件式
* @param[in] format 書式設定
* @details exprがfalseの時、発生した関数名と行数、エラーメッセージが表示される
*/
#define MY_ASSERTION(expr,format ,...) do{ Framework::Utility::assertion(expr, std::string("error an occurred %s %d:\n") + format, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
/**
* @def MY_DEBUG_LOG
* @brief デバッグログ
* @param[in] format 書式設定
*/
#define MY_DEBUG_LOG(format, ...) do{ Framework::Utility::debugLog(format, __VA_ARGS__);} while(0)
#else
#define MY_ASSERTION(expr,format ,...)
#define MY_DEBUG_LOG(format,...) 
#endif
/**
* @def MY_THROW_IF_FALSE
* @brief 失敗していたら例外を投げる
* @param[in] expr 条件式
* @paran format 書式設定
*/
#define MY_THROW_IF_FALSE(expr, format, ...) do{ Framework::Utility::throwIfFalse(expr, format, __VA_ARGS__); }while(0)
/**
* @def MY_THROW_IF_FAILED
* @brief 失敗していたら例外を投げる
* @param[in] hr 処理結果
* @param format 書式設定
*/
#define MY_THROW_IF_FAILED(hr,format,...) do{ Framework::Utility::throwIfFailed(hr, format, __VA_ARGS__); } while(0)

namespace Framework::Utility {
    /**
    * @brief アサーション関数
    * @param expr 条件式
    * @param format フォーマット
    */
    template <class ... Args>
    inline void assertion(bool expr, const std::string& fmt, Args ... args) {
        std::string mes = format(fmt, args...);
        std::wstring wmes = toWString(mes);
        _ASSERT_EXPR(expr, wmes.c_str());
    }
    /**
    * @brief 出力ウィンドウにログを出力する
    * @param fmt フォーマット
    */
    template <class ... Args>
    inline void debugLog(const std::string& fmt, Args ... args) {
        _RPTN(_CRT_WARN, fmt.c_str(), args...);
    }

    /**
    * @brief 失敗していたら例外を投げる
    */
    template <class ... Args>
    inline void throwIfFalse(bool expr, const std::string& fmt, Args ... args) {
        if (!expr) {
            std::string mes = format(fmt, args...);
            throw std::exception(mes.c_str());
        }
    }

    /**
    * @brief 失敗していたら例外を投げる
    */
    template <class ... Args>
    inline void throwIfFailed(HRESULT hr, const std::string& fmt, Args... args) {
        if (FAILED(hr)) {
            std::string mes = format(fmt, args...);
            throw HrException(hr, mes);
        }
    }
    /**
    * @brief エラーウィンドウを表示する
    */
    template <class ... Args>
    inline void errorWindow(bool expr, const std::string& fmt, Args... args) {
        if (!expr) {
            std::string mes = format(fmt, args...);
            MessageBoxA(nullptr, mes.c_str(), "エラー", MB_APPLMODAL);
        }
    }
} //Framework::Utility 