// The tests' one assertion: a PASS/FAIL line each, exit code = failures.

#pragma once

#include <cstdio>
#include <string>

inline int g_failures = 0;

inline void Check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

inline void CheckEq(const std::string& actual, const std::string& expected, const char* what)
{
    if (actual != expected)
        std::printf("      expected: %s\n      actual:   %s\n", expected.c_str(), actual.c_str());
    Check(actual == expected, what);
}

inline void CheckEq(long long actual, long long expected, const char* what)
{
    if (actual != expected)
        std::printf("      expected: %lld\n      actual:   %lld\n", expected, actual);
    Check(actual == expected, what);
}
