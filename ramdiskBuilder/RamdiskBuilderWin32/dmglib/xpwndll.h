#if WIN32 

#define DLLEXPORT __declspec(dllexport)

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int _cdecl  xpwntool_enc_dec(char* srcName, char* destName, char* templateFileName, char* ivStr, char* keyStr);

#ifdef __cplusplus
}
#endif

#endif //WIN32