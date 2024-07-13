#define STATUS_TEXT(code) ( \
	code == 100 ? "Continue" : \
	code == 200 ? "OK" : \
	code == 206 ? "Partial Content" : \
	code == 301 ? "Moved Permanently" : \
	code == 302 ? "Found" : \
	code == 304 ? "Not Modified" : \
	code == 400 ? "Bad Request" : \
	code == 401 ? "Unauthorized" : \
	code == 403 ? "Forbidden" : \
	code == 404 ? "Not Found" : \
	code == 500 ? "Internal Server Error" : \
	code == 503 ? "Service Unavailable" : \
	"Unknown Status")
