// iTunnel mod with restore mode comm support
// Based on iPhone_tunnel by novi (novi.mad@gmail.com ) http://novis.jimdo.com
// thanks
// http://i-funbox.com/blog/2008/09/itunesmobiledevicedll-changed-in-itunes-80/
// 2010 msftguy

#if WIN32 
#include <winsock2.h>
#include <stdio.h>
#else 
#import <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/ip.h>
#include <mach/error.h>
#endif 

#define ADNCI_MSG_CONNECTED     1
#define ADNCI_MSG_DISCONNECTED  2
#define ADNCI_MSG_UNKNOWN       3

typedef void* restore_dev_t;
typedef void* afc_conn_t;
typedef void* am_device_t;
typedef int muxconn_t;

struct am_device_notification_callback_info
{
        am_device_t dev;  /* 0    device */
        unsigned int msg;       /* 4    one of ADNCI_MSG_* */
};

typedef void(*am_device_notification_callback_t)(struct am_device_notification_callback_info *);

//typedef struct {
//    unsigned int unknown0;                      /* 0 */
//    unsigned int unknown1;                      /* 4 */
//    unsigned int unknown2;                      /* 8 */
//    am_device_notification_callback_t callback;   /* 12 */ 
//    unsigned int unknown3;                      /* 16 */
//} am_device_callbacks_t;

typedef void* am_device_callbacks_t;

///////////////////////// WIN32 //////////////////////////////
#if WIN32 
typedef void* CFStringRef;
#define nil NULL

typedef void* pthread_t;
typedef void* pthread_attr_t;

#define THREADPROCATTR WINAPI

typedef void *(THREADPROCATTR* thread_proc_t)(void *);

int pthread_create(pthread_t * __restrict pHandle,
                         const pthread_attr_t * __restrict,
                         thread_proc_t threadStart,
                         void* arg) 
{
	DWORD tid;
	*pHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadStart, arg, CREATE_SUSPENDED, &tid);
	return tid;
}

#define pthread_detach ResumeThread

#define ERR_SUCCESS 0

enum CFStringBuiltInEncodings {
   //kCFStringEncodingMacRoman = 0,
   //kCFStringEncodingWindowsLatin1 = 0x0500,
   //kCFStringEncodingISOLatin1 = 0x0201,
   //kCFStringEncodingNextStepLatin = 0x0B01,
   kCFStringEncodingASCII = 0x0600,
   //kCFStringEncodingUnicode = 0x0100,
   //kCFStringEncodingUTF8 = 0x08000100,
   //kCFStringEncodingNonLossyASCII = 0x0BFF
};
typedef enum CFStringBuiltInEncodings CFStringBuiltInEncodings;

typedef int socklen_t;

#define strcasecmp strcmpi

#define sleep(x) Sleep (1000 * (x))

typedef unsigned char uint8_t;

#define close closesocket

extern "C" bool CFStringGetCString(CFStringRef cfStr, char* buf, size_t cbBuf, CFStringBuiltInEncodings encoding);

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "iTunesMobileDevice")
#pragma comment(lib, "CoreFoundation")

#else ////////////////////////// OS X ///////////////////

#define THREADPROCATTR 

#define Sleep(ms) usleep(ms*1000)

#endif

extern "C" {
        
int AMDeviceNotificationSubscribe(am_device_notification_callback_t notificationCallback, int , int, int , am_device_callbacks_t *callbacks);
int AMDeviceConnect(am_device_t am_device);
int AMDeviceIsPaired(am_device_t am_device);
int AMDeviceValidatePairing(am_device_t am_device);
int AMDeviceStartSession(am_device_t am_device);
int AMDeviceStartService(am_device_t am_device, CFStringRef service_name, int *handle, unsigned int *unknown );
int AFCConnectionOpen(int handle, unsigned int io_timeout, afc_conn_t* afc_connection);
int AMDeviceDisconnect(am_device_t am_device);
int AMDeviceStopSession(am_device_t am_device);

CFStringRef AMDeviceCopyDeviceIdentifier(am_device_t device);

muxconn_t AMDeviceGetConnectionID(am_device_t device);
muxconn_t AMRestoreModeDeviceGetDeviceID(restore_dev_t restore_device);
int AMRestoreModeDeviceReboot(restore_dev_t restore_device);
int USBMuxConnectByPort(muxconn_t muxConn, short netPort, int* sockHandle);


restore_dev_t AMRestoreModeDeviceCreate(int arg1_is_0, int connId, int arg3_is_0);

}

enum exit_status {
        EXIT_SUCCESS_I = 0x00,
        EXIT_DISCONNECTED = 0x1,
        EXIT_CONNECT_ERROR = 0x02,
        EXIT_SERVICE_ERROR = 0x03,
        EXIT_BIND_ERROR = 0x04,
        EXIT_GENERAL_ERROR = 0x05,
		EXIT_OS_TOO_FUCKING_OLD = 0x6,
		EXIT_REGISTRY_ERROR = 0x07,
		EXIT_LOAD_ERROR  = 0x08,
};

#pragma mark Prototype definition

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 256
#define MAX_SOCKETS 512

#define SOCKET_ERROR -1

struct connection
{
        int from_handle;
        int to_handle;
};

void* THREADPROCATTR wait_for_device(void*);
void wait_connections();
void notification(struct am_device_notification_callback_info*);
void*THREADPROCATTR conn_forwarding_thread(void* arg);

// 接続数
static int threadCount = 0;

// Mac側のソケット
static int  sock;

static muxconn_t muxConn = 0;

// デバイスID指定
static const char* target_device_id = nil;

// ターゲットデバイス
static am_device_t target_device = NULL;

#if !WIN32
void recv_signal(int sig)
{
        printf("Info: Signal received. (%d)\n", sig);
        
        // 標準関数へシグナルを投げる
        fflush(stdout);
        signal(sig, SIG_DFL);
        raise(sig);
}
#endif

#pragma mark Main function

#if WIN32 
	const unsigned short default_local_port = 22;
#else
	const unsigned short default_local_port = 2022;
#endif

unsigned short g_iphone_port = 22;
unsigned short g_local_port = default_local_port;

void print_error(int error = 0) {
	int err = error != 0 ? error :
#if WIN32
		GetLastError();
#else
		errno;
#endif
	printf("Error 0x%X (%i): '%s'\n", err, err, strerror(err));
}

int main (int argc, char *argv [])
{
        // パラメータ確認
        // ヘルプ表示
        if ( !(argc >= 3 && argc <= 4))
        {
                printf(
                           "\niphone_tunnel v2.0 for Mac\n"
                                "Created by novi. (novi.mad@gmail.com)\n"
                                "Restore mode hack by msft.guy\n"
								"\nUsage: iphone_tunnel [<iPhone port> <Local port> [Device ID, 40 digit]]\n"
                                "Example: iphone_tunnel 22 9876 0123456...abcdef\n"
								"Default ports are 22 %hu\n", default_local_port
                           );
        }
              
#if WIN32
		WSADATA useless;
		WSAStartup(WINSOCK_VERSION, &useless);

		WCHAR wbuf[MAX_PATH];
		DWORD cbBuf = sizeof(wbuf);

		//for necrophiles - XP support
		typedef LSTATUS
			(APIENTRY
			*RegGetValue_t) (
				__in HKEY    hkey,
				__in_opt LPCWSTR  lpSubKey,
				__in_opt LPCWSTR  lpValue,
				__in_opt DWORD    dwFlags,
				__out_opt LPDWORD pdwType,
				__out_bcount_part_opt(*pcbData,*pcbData) PVOID   pvData,
				__inout_opt LPDWORD pcbData
				);

		RegGetValue_t rgv = (RegGetValue_t)GetProcAddress(LoadLibraryW(L"advapi32"), "RegGetValueW");
		if  (rgv == nil) {
			rgv = (RegGetValue_t)GetProcAddress(LoadLibraryW(L"Shlwapi"), "SHRegGetValueW");
		}
		if  (rgv == nil) {
			printf("Your OS is too fucking old!\n");
			exit(EXIT_OS_TOO_FUCKING_OLD);
		}
		//End XP support 
		HRESULT error = S_OK;

		error = rgv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Apple Inc.\\Apple Application Support", L"InstallDir", RRF_RT_REG_SZ|RRF_ZEROONFAILURE, NULL, (LPBYTE)wbuf, &cbBuf);
		if (ERROR_SUCCESS != error) {
			print_error(error);
			printf("Could not locate 'Apple Application Support' folder path in registry: ABORTING\n");
			exit(EXIT_REGISTRY_ERROR);
		}

		SetDllDirectoryW(wbuf);
		
		if (!LoadLibraryW(L"ASL")) {
			print_error();
			printf("WARNING: Could not load ASL from '%ws'\n", wbuf);
		}
		
		if (!LoadLibraryW(L"CoreFoundation")) {
			print_error();
			printf("Could not load CoreFoundation from '%ws': ABORTING\n", wbuf);
			exit(EXIT_LOAD_ERROR);
		}

		cbBuf = sizeof(wbuf);
		error = rgv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Apple Inc.\\Apple Mobile Device Support", L"InstallDir", RRF_RT_REG_SZ|RRF_ZEROONFAILURE, NULL, (LPBYTE)wbuf, &cbBuf);
		if (ERROR_SUCCESS != error) {
			print_error(error);
			printf("Could not locate 'Apple Mobile Device Support' folder path in registry: ABORTING\n");
			exit(EXIT_REGISTRY_ERROR);
		}
		wcscat_s<MAX_PATH>(wbuf, L"\\iTunesMobileDevice.dll");
		if (!LoadLibraryW(wbuf)) {
			print_error();
			printf("Could not load %ws: ABORTING\n", wbuf);
			exit(EXIT_LOAD_ERROR);
		}
		
#endif


        if (argc >= 3) {
			// デバイス側ポートを取得
			// バイトオーダーを変換
			sscanf(argv[1], "%hu", &g_iphone_port);
        
			// Mac側ポートを取得
			sscanf(argv[2], "%hu", &g_local_port);
		}
        
        
#if !WIN32
		// シグナル受信の動作を登録
        signal(SIGABRT, recv_signal);
        signal(SIGILL, recv_signal);
        signal(SIGINT, recv_signal);
        signal(SIGSEGV, recv_signal);
        signal(SIGTERM, recv_signal);
#endif        
        if (argc == 4) {
                target_device_id = argv[3];
                printf("Info: Target %s\n", argv[3]);
        } else {
                target_device_id = NULL;
        }
        
        // Mac側のクライアントからの待ち受け開始
        wait_connections ();

        return 0;
}

void wait_connections()
{
        struct sockaddr_in saddr;
        int ret = 0;
        
        // Socket を作成
        // Mac側
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        saddr.sin_port = htons(g_local_port);     
        sock = socket(AF_INET, SOCK_STREAM, 0);
        
        // トンネルを強制終了して再起動したときのバインド失敗を回避
        // http://homepage3.nifty.com/owl_h0h0/unix/job/UNIX/network/socket.html
        int temp = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&temp, sizeof(temp))) {
			print_error();
			printf("setsockopt() failed - ignorable\n");
        }
        
        // バインド
        ret = bind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr));
        
        // バインドエラーチェック
        if ( ret == SOCKET_ERROR ) {
                fflush(stdout);
                printf("bind error %i !\n", ret);
                fflush(stdout);
                exit(EXIT_BIND_ERROR);
        }
        
        // リクエスト受信受付
        listen(sock, 0);
                
        // 接続待ち用のスレッドを作成
        int lpThreadId;
        pthread_t socket_thread;
        lpThreadId = pthread_create(&socket_thread, NULL, wait_for_device, NULL);
        pthread_detach(socket_thread);
        
        printf("Waiting for device...\n");
        fflush(stdout);
        
        while (1) {
                // デバイスのコールバックを登録
                am_device_callbacks_t callbacks; 
                ret = AMDeviceNotificationSubscribe(notification, 0, 0, 0, &callbacks);
                if (ret != ERR_SUCCESS) {
                        printf("AMDeviceNotificationSubscribe = %i\n", ret);
                        exit(EXIT_GENERAL_ERROR);
                }
                
#if WIN32
				Sleep(-1);
#else
				CFRunLoopRun();
#endif
                printf("RUN LOOP EXIT\n");
                Sleep(1000);
        }
}

/****************************************************************************/

void notification(struct am_device_notification_callback_info* info)
{
        char deviceName[BUFFER_SIZE];
        CFStringRef devId = AMDeviceCopyDeviceIdentifier(info->dev);
        CFStringGetCString(devId, deviceName, sizeof(deviceName), kCFStringEncodingASCII);
        
        if (target_device_id != nil) {
                if (0 != strcasecmp(deviceName, target_device_id)) {
                        printf("Ignoring device %s (need %s)\n", deviceName, target_device_id);
                        return;
                }
        }
        
        if (info -> msg == ADNCI_MSG_CONNECTED) {
                target_device = info->dev;
                printf("Device connected: %s\n", deviceName);
        } else {
                printf("Device disconnected: %s\n", deviceName);
                target_device = NULL;
                muxConn = 0;
        }
}

void* THREADPROCATTR wait_for_device(void* arg)
{
        int ret;
        int handle = -1;
        restore_dev_t restore_dev;
                
        while (1) {
                
                //printf("waiting for device...\n");
                if (target_device == NULL) {
                        sleep(1);
                        continue;
                }
                
                printf("Info: Waiting for new TCP connection on port %hu\n", g_local_port);
                                
                // Mac側クライアントからの待ち
                struct sockaddr_in sockAddrin;
                socklen_t len = sizeof(sockAddrin);
                int new_sock = accept(sock, (struct sockaddr*) &sockAddrin , &len);
                
                
                if (new_sock == -1) {
                        printf("accept error\n");
                        continue;
                }
                
                printf("Info: New connection...\n");
                
                // ターゲットが準備完了
                // デバイスへ接続

                if (muxConn == 0)
                {
                        ret = AMDeviceConnect(target_device);
                        if (ret == ERR_SUCCESS) {
                                muxConn = AMDeviceGetConnectionID(target_device);
                        } else if (ret == -402653144) {
                                restore_dev = AMRestoreModeDeviceCreate(0, AMDeviceGetConnectionID(target_device), 0);
                                printf("restore_dev = %p\n", restore_dev);
                                muxConn = AMRestoreModeDeviceGetDeviceID(restore_dev);
                                printf("muxConn = %X\n", muxConn);
                                AMRestoreModeDeviceReboot(restore_dev);
                                sleep(10);
                        } else {
                                printf("AMDeviceConnect = %i\n", ret);
                                goto error_connect;
                        }
                }                               
                puts("Info: Device connected.");
                                
                ret = USBMuxConnectByPort(muxConn, htons(g_iphone_port), &handle);
                if (ret != ERR_SUCCESS) {
                        printf("USBMuxConnectByPort = %x, handle=%x\n", ret, handle);
                        goto error_service;
                }
                                
                puts("Info: Service started.");
                
                struct connection* connection1;
                struct connection* connection2;
                
                // 接続ソケット構造体を作る
                connection1 = new connection;
                if (!connection1) {
                        exit(EXIT_GENERAL_ERROR);
                }
                connection2 = new connection;    
                if (!connection2) {
                        exit(EXIT_GENERAL_ERROR);
                }
                
                // 送受信用のスレッドへ値を渡す
                connection1->from_handle = new_sock;
                connection1->to_handle = handle;
                connection2->from_handle = handle;
                connection2->to_handle = new_sock;
                
                printf("sock handle newsock:%d iphone:%d\n", new_sock, handle);
                fflush(stdout);
                
                // 送受信用のスレッドを作成
                int lpThreadId;
                int lpThreadId2;
                pthread_t thread1;
                pthread_t thread2;
                
                lpThreadId = pthread_create(&thread1, NULL, conn_forwarding_thread, (void*)connection1);
                lpThreadId2 = pthread_create(&thread2, NULL, conn_forwarding_thread, (void*)connection2);
                
                pthread_detach(thread2);
                pthread_detach(thread1);
        
                Sleep(100);
                
                continue;

                // 接続失敗時の後始末
        error_connect:
                printf("Error: Device Connect\n");
                AMDeviceStopSession(target_device);
                AMDeviceDisconnect(target_device);
                sleep(1);
                
                continue;
                
        error_service:
                printf("Error: Device Service\n");
                AMDeviceStopSession(target_device);
                AMDeviceDisconnect(target_device);
                sleep(1);
                continue;
                
        }
}


/****************************************************************************/

// Mac からのデータを iPhone へ転送
void* THREADPROCATTR conn_forwarding_thread(void* arg)
{
        connection* con = (connection*)arg;
        uint8_t buffer[BUFFER_SIZE];
        int bytes_recv, bytes_send;
        
        // スレッドカウントを増やす
        threadCount++;
        fflush(stdout);
        printf("threadcount=%d\n",threadCount);
        fflush(stdout);
        
        while (1) {
                // Mac からのデータを受信
                bytes_recv = recv(con->from_handle, (char*)buffer, BUFFER_SIZE, 0);
                
                // それを iPhone へ送る
                bytes_send = send(con->to_handle, (char*)buffer, bytes_recv, 0);
                
                // エラー発生
                if (bytes_recv == 0 || bytes_recv == SOCKET_ERROR || bytes_send == 0 || bytes_send == SOCKET_ERROR) {
                        // スレッドカウントを減らす
                        threadCount--;
                        fflush(stdout);
                        printf("threadcount=%d\n", threadCount);
                        fflush(stdout);
                        
                        // コネクションを閉じる
                        close(con->from_handle);
                        close(con->to_handle);
                                                
                        delete con;
                        
                        // スレッドを終了
                        break;
                }
        }
        return nil;
}
