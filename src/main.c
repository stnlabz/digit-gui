#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

#define APP_TITLE "Digit"
#define ID_OUTPUT 1001
#define ID_INPUT 1002
#define ID_SEND 1003
#define ID_STATUS 1004
#define BUFFER_MAX 8192
#define WM_DIGIT_RESULT (WM_APP + 1)
#define ASK_RECEIVE_TIMEOUT_MS 180000

static HWND main_window;
static HWND output_box;
static HWND input_box;
static HWND send_button;
static HWND status_text;
static char digit_host[256] = "127.0.0.1";
static INTERNET_PORT digit_port = 8081;

typedef struct {
    int ok;
    DWORD error;
    DWORD http_status;
    char answer[4096];
} digit_result_t;

typedef struct {
    char question[4096];
} digit_request_job_t;

static void trim_line(char *text){size_t n;if(!text)return;n=strlen(text);while(n>0&&(text[n-1]=='\r'||text[n-1]=='\n'||text[n-1]==' '||text[n-1]=='\t'))text[--n]='\0';}
static int config_path(char *path,size_t path_size){DWORD n=GetModuleFileNameA(NULL,path,(DWORD)path_size);char *slash;if(n==0||n>=path_size)return 0;slash=strrchr(path,'\\');if(!slash)return 0;slash[1]='\0';if(strlen(path)+strlen("digit.conf")+1>path_size)return 0;strcat_s(path,path_size,"digit.conf");return 1;}
static int load_config(void){FILE *stream;char path[MAX_PATH],line[512];int have_host=0,have_port=0;if(!config_path(path,sizeof(path)))return 0;stream=fopen(path,"rb");if(!stream)return 0;while(fgets(line,sizeof(line),stream)){char *value;trim_line(line);if(line[0]=='\0'||line[0]=='#')continue;value=strchr(line,'=');if(!value)continue;*value++='\0';if(strcmp(line,"host")==0){if(!value[0]||strlen(value)>=sizeof(digit_host)){fclose(stream);return 0;}strcpy_s(digit_host,sizeof(digit_host),value);have_host=1;}else if(strcmp(line,"port")==0){char *end=NULL;unsigned long port=strtoul(value,&end,10);if(!value[0]||!end||*end!='\0'||port==0||port>65535){fclose(stream);return 0;}digit_port=(INTERNET_PORT)port;have_port=1;}}fclose(stream);return have_host&&have_port;}
static void append_output(const char *speaker,const char *text){char line[BUFFER_MAX+64];int length=GetWindowTextLengthA(output_box);snprintf(line,sizeof(line),"%s: %s\r\n\r\n",speaker,text);SendMessageA(output_box,EM_SETSEL,(WPARAM)length,(LPARAM)length);SendMessageA(output_box,EM_REPLACESEL,FALSE,(LPARAM)line);}
static int extract_answer(const char *json,char *answer,size_t answer_size){const char *p=strstr(json,"\"answer\":\"");size_t o=0;if(!p||!answer||answer_size<2)return 0;p+=10;while(*p&&o+1<answer_size){if(*p=='"')break;if(*p=='\\'&&p[1]){++p;if(*p=='n')answer[o++]='\n';else if(*p=='r'){}else if(*p=='t')answer[o++]='\t';else answer[o++]=*p;++p;continue;}answer[o++]=*p++;}answer[o]='\0';return *p=='"';}

static int digit_request(const char *method,const char *path,const char *body,char *response,size_t response_size,DWORD receive_timeout,DWORD *error_out,DWORD *status_out)
{
    WCHAR host_w[256],path_w[256],method_w[16];HINTERNET session=NULL,connection=NULL,request=NULL;DWORD status=0,status_size=sizeof(status),available=0,read=0;size_t used=0;int ok=0;DWORD error=ERROR_SUCCESS;
    if(error_out)*error_out=ERROR_SUCCESS;if(status_out)*status_out=0;
    if(!MultiByteToWideChar(CP_UTF8,0,digit_host,-1,host_w,256)||!MultiByteToWideChar(CP_UTF8,0,path,-1,path_w,256)||!MultiByteToWideChar(CP_UTF8,0,method,-1,method_w,16)){error=GetLastError();goto done;}
    session=WinHttpOpen(L"Digit GUI/0.3",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session){error=GetLastError();goto done;}
    if(!WinHttpSetTimeouts(session,5000,5000,10000,(int)receive_timeout)){error=GetLastError();goto done;}
    connection=WinHttpConnect(session,host_w,digit_port,0);if(!connection){error=GetLastError();goto done;}
    request=WinHttpOpenRequest(connection,method_w,path_w,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);if(!request){error=GetLastError();goto done;}
    if(!WinHttpSendRequest(request,body?L"Content-Type: text/plain\r\n":WINHTTP_NO_ADDITIONAL_HEADERS,body?(DWORD)-1L:0,body?(LPVOID)body:WINHTTP_NO_REQUEST_DATA,body?(DWORD)strlen(body):0,body?(DWORD)strlen(body):0,0)){error=GetLastError();goto done;}
    if(!WinHttpReceiveResponse(request,NULL)){error=GetLastError();goto done;}
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&status_size,WINHTTP_NO_HEADER_INDEX)){error=GetLastError();goto done;}
    if(status_out)*status_out=status;if(status!=200)goto done;
    response[0]='\0';
    do{if(!WinHttpQueryDataAvailable(request,&available)){error=GetLastError();goto done;}if(!available)break;if(available>response_size-used-1)available=(DWORD)(response_size-used-1);if(!available){error=ERROR_INSUFFICIENT_BUFFER;goto done;}if(!WinHttpReadData(request,response+used,available,&read)){error=GetLastError();goto done;}used+=read;response[used]='\0';}while(read>0);ok=1;
done:if(error_out)*error_out=error;if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);return ok;
}

static void check_health(void){char response[1024];DWORD error,status;if(digit_request("GET","/health",NULL,response,sizeof(response),10000,&error,&status)&&strstr(response,"READY"))SetWindowTextA(status_text,"Connected");else SetWindowTextA(status_text,"Offline");}

static DWORD WINAPI ask_worker(LPVOID parameter)
{
    digit_request_job_t *job=(digit_request_job_t *)parameter;digit_result_t *result=(digit_result_t *)calloc(1,sizeof(*result));char response[BUFFER_MAX];
    if(!result){free(job);return 1;}
    result->ok=digit_request("POST","/ask",job->question,response,sizeof(response),ASK_RECEIVE_TIMEOUT_MS,&result->error,&result->http_status);
    if(result->ok&&!extract_answer(response,result->answer,sizeof(result->answer))){result->ok=0;result->error=ERROR_INVALID_DATA;}
    free(job);PostMessageA(main_window,WM_DIGIT_RESULT,0,(LPARAM)result);return 0;
}

static void send_question(void)
{
    digit_request_job_t *job;HANDLE thread;char question[4096];
    GetWindowTextA(input_box,question,sizeof(question));if(!question[0])return;
    job=(digit_request_job_t *)calloc(1,sizeof(*job));if(!job){append_output("Digit GUI","Unable to allocate request.");return;}
    strcpy_s(job->question,sizeof(job->question),question);append_output("You",question);SetWindowTextA(input_box,"");SetWindowTextA(status_text,"Waiting for Digit...");EnableWindow(send_button,FALSE);
    thread=CreateThread(NULL,0,ask_worker,job,0,NULL);if(!thread){DWORD error=GetLastError();char message[256];free(job);EnableWindow(send_button,TRUE);snprintf(message,sizeof(message),"Unable to start request thread. Windows error %lu.",(unsigned long)error);append_output("Digit GUI",message);return;}CloseHandle(thread);
}

static LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
    switch(msg){
        case WM_CREATE:main_window=hwnd;output_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,12,660,390,hwnd,(HMENU)ID_OUTPUT,NULL,NULL);input_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,12,414,560,28,hwnd,(HMENU)ID_INPUT,NULL,NULL);send_button=CreateWindowA("BUTTON","Send",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,582,414,90,28,hwnd,(HMENU)ID_SEND,NULL,NULL);status_text=CreateWindowA("STATIC","Checking...",WS_CHILD|WS_VISIBLE,12,452,500,20,hwnd,(HMENU)ID_STATUS,NULL,NULL);check_health();return 0;
        case WM_COMMAND:if(LOWORD(wparam)==ID_SEND){send_question();SetFocus(input_box);return 0;}break;
        case WM_DIGIT_RESULT:{digit_result_t *result=(digit_result_t *)lparam;char message[512];EnableWindow(send_button,TRUE);if(result){if(result->ok){append_output("Digit",result->answer);SetWindowTextA(status_text,"Connected");}else if(result->http_status){snprintf(message,sizeof(message),"Digit returned HTTP status %lu.",(unsigned long)result->http_status);append_output("Digit GUI",message);SetWindowTextA(status_text,"Connected - request failed");}else{snprintf(message,sizeof(message),"Windows network error %lu while waiting for Digit.",(unsigned long)result->error);append_output("Digit GUI",message);SetWindowTextA(status_text,"Request failed");}free(result);}SetFocus(input_box);return 0;}
        case WM_SIZE:{int w=LOWORD(lparam),h=HIWORD(lparam);MoveWindow(output_box,12,12,w-24,h-92,TRUE);MoveWindow(input_box,12,h-68,w-124,28,TRUE);MoveWindow(send_button,w-102,h-68,90,28,TRUE);MoveWindow(status_text,12,h-32,w-24,20,TRUE);return 0;}
        case WM_DESTROY:main_window=NULL;PostQuitMessage(0);return 0;
    }return DefWindowProcA(hwnd,msg,wparam,lparam);
}

static int self_test(void){char answer[256];int failures=0;if(!extract_answer("{\"answered\":true,\"evidence_count\":1,\"answer\":\"I will enter safe mode.\"}",answer,sizeof(answer))||strcmp(answer,"I will enter safe mode.")!=0)++failures;if(extract_answer("{\"answered\":false}",answer,sizeof(answer)))++failures;return failures?1:0;}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command_line,int show){WNDCLASSA wc={0};HWND hwnd;MSG msg;(void)previous;(void)command_line;if(!load_config()){MessageBoxA(NULL,"Unable to load digit.conf beside digit-gui.exe. Expected host=<server> and port=<port>.",APP_TITLE,MB_OK|MB_ICONERROR);return 1;}wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName="DigitGuiWindow";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassA(&wc))return 1;hwnd=CreateWindowExA(0,wc.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,700,530,NULL,NULL,instance,NULL);if(!hwnd)return 1;ShowWindow(hwnd,show);UpdateWindow(hwnd);while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return(int)msg.wParam;}
int main(int argc,char **argv){if(argc==2&&strcmp(argv[1],"--self-test")==0)return self_test();return WinMain(GetModuleHandle(NULL),NULL,GetCommandLineA(),SW_SHOWDEFAULT);}
