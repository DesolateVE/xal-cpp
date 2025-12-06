#pragma once

#ifdef MSLOGIN_EXPORTS
#define MSLOGIN_API __declspec(dllexport)
#else
#define MSLOGIN_API __declspec(dllimport)
#endif