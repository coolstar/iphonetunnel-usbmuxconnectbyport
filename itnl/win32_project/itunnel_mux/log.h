enum LOG_LEVEL 
{
	LOG_FATAL,
	LOG_ERROR,
	LOG_INFO,
	LOG_DEBUG,
};
void Log(LOG_LEVEL severity, const char* fmt...);