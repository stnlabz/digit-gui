#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

#define APP_TITLE "Digit"
#define DIGIT_CONFIG "config\\digit.conf"
#define ID_OUTPUT 1001
#define ID_INPUT 1002
#define ID_SEND 1003
#define ID_STATUS 1004
#define BUFFER_MAX 8192

static HWND output_box;
static HWND input_box;
static HWND status_text;
static char digit_host[256] = "127.0.0.1";
static INTERNET_PORT digit_port = 8081;

static void trim_line(char *text)
{
    size_t n;
    if (!text) return;
    n = strlen(text);
    while (n > 0 && (text[n - 1] == '\r' || text[n - 1] == '\n' || text[n - 1] == ' ' || text[n - 1] == '\t')) text[--n] = '\0';
}

static int load_config(void)
{
    FILE *stream;
    char line[512];
    int have_host = 0;
    int have_port = 0;

    stream = fopen(DIGIT_CONFIG, "rb");
    if (!stream) return 0;

    while (fgets(line, sizeof(line), stream))
    {
        char *value;
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        value = strchr(line, '=');
        if (!value) continue;
        *value++ = '\0';
        if (strcmp(line, "host") == 0)
        {
            if (!value[0] || strlen(value) >= sizeof(digit_host)) { fclose(stream); return 0; }
            strcpy_s(digit_host, sizeof(digit_host), value);
            have_host = 1;
        }
        else if (strcmp(line, "port") == 0)
        {
            char *end = NULL;
            unsigned long port = strtoul(value, &end, 10);
            if (!value[0] || !end || *end != '\0' || port == 0 || port > 65535) { fclose(stream); return 0; }
            digit_port = (INTERNET_PORT)port;
            have_port = 1;
        }
    }
    fclose(stream);
    return have_host && have_port;
}

static void append_output(const char *speaker, const char *text)
{
    char line[BUFFER_MAX + 64];
    int length = GetWindowTextLengthA(output_box);
    snprintf(line, sizeof(line), "%s: %s\r\n\r\n", speaker, text);
    SendMessageA(output_box, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    SendMessageA(output_box, EM_REPLACESEL, FALSE, (LPARAM)line);
}

static int extract_answer(const char *json, char *answer, size_t answer_size)
{
    const char *p = strstr(json, "\"answer\":\"");
    size_t o = 0;
    if (!p || !answer || answer_size < 2) return 0;
    p += 10;
    while (*p && o + 1 < answer_size)
    {
        if (*p == '"') break;
        if (*p == '\\' && p[1])
        {
            ++p;
            if (*p == 'n') answer[o++] = '\n';
            else if (*p == 'r') { }
            else if (*p == 't') answer[o++] = '\t';
            else answer[o++] = *p;
            ++p;
            continue;
        }
        answer[o++] = *p++;
    }
    answer[o] = '\0';
    return *p == '"';
}

static int digit_request(const char *method, const char *path, const char *body, char *response, size_t response_size)
{
    WCHAR host_w[256], path_w[256], method_w[16];
    HINTERNET session = NULL, connection = NULL, request = NULL;
    DWORD status = 0, status_size = sizeof(status), available = 0, read = 0;
    size_t used = 0;
    int ok = 0;

    if (!MultiByteToWideChar(CP_UTF8, 0, digit_host, -1, host_w, 256)) return 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, 256)) return 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, method, -1, method_w, 16)) return 0;
    session = WinHttpOpen(L"Digit GUI/0.2", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto done;
    WinHttpSetTimeouts(session, 3000, 3000, 5000, 30000);
    connection = WinHttpConnect(session, host_w, digit_port, 0);
    if (!connection) goto done;
    request = WinHttpOpenRequest(connection, method_w, path_w, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!request) goto done;
    if (!WinHttpSendRequest(request, body ? L"Content-Type: text/plain\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS, body ? (DWORD)-1L : 0, body ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA, body ? (DWORD)strlen(body) : 0, body ? (DWORD)strlen(body) : 0, 0)) goto done;
    if (!WinHttpReceiveResponse(request, NULL)) goto done;
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) || status != 200) goto done;
    response[0] = '\0';
    do
    {
        if (!WinHttpQueryDataAvailable(request, &available)) goto done;
        if (!available) break;
        if (available > response_size - used - 1) available = (DWORD)(response_size - used - 1);
        if (!available || !WinHttpReadData(request, response + used, available, &read)) goto done;
        used += read;
        response[used] = '\0';
    } while (read > 0);
    ok = 1;
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static void check_health(void)
{
    char response[1024];
    if (digit_request("GET", "/health", NULL, response, sizeof(response)) && strstr(response, "READY")) SetWindowTextA(status_text, "Connected");
    else SetWindowTextA(status_text, "Offline");
}

static void send_question(void)
{
    char question[4096], response[BUFFER_MAX], answer[4096];
    GetWindowTextA(input_box, question, sizeof(question));
    if (!question[0]) return;
    append_output("You", question);
    SetWindowTextA(input_box, "");
    SetWindowTextA(status_text, "Waiting...");
    if (!digit_request("POST", "/ask", question, response, sizeof(response))) { append_output("Digit", "Unable to reach Digit."); SetWindowTextA(status_text, "Offline"); return; }
    if (!extract_answer(response, answer, sizeof(answer))) append_output("Digit", "Invalid response received."); else append_output("Digit", answer);
    SetWindowTextA(status_text, "Connected");
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_CREATE:
            output_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,12,660,390,hwnd,(HMENU)ID_OUTPUT,NULL,NULL);
            input_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,12,414,560,28,hwnd,(HMENU)ID_INPUT,NULL,NULL);
            CreateWindowA("BUTTON","Send",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,582,414,90,28,hwnd,(HMENU)ID_SEND,NULL,NULL);
            status_text=CreateWindowA("STATIC","Checking...",WS_CHILD|WS_VISIBLE,12,452,300,20,hwnd,(HMENU)ID_STATUS,NULL,NULL);
            check_health(); return 0;
        case WM_COMMAND: if(LOWORD(wparam)==ID_SEND){send_question();SetFocus(input_box);return 0;} break;
        case WM_SIZE:{int w=LOWORD(lparam),h=HIWORD(lparam);MoveWindow(output_box,12,12,w-24,h-92,TRUE);MoveWindow(input_box,12,h-68,w-124,28,TRUE);MoveWindow(GetDlgItem(hwnd,ID_SEND),w-102,h-68,90,28,TRUE);MoveWindow(status_text,12,h-32,300,20,TRUE);return 0;}
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wparam,lparam);
}

static int self_test(void)
{
    char answer[256]; int failures=0;
    if(!extract_answer("{\"answered\":true,\"evidence_count\":1,\"answer\":\"I will enter safe mode.\"}",answer,sizeof(answer))||strcmp(answer,"I will enter safe mode.")!=0)++failures;
    if(extract_answer("{\"answered\":false}",answer,sizeof(answer)))++failures;
    printf("Digit GUI self-test: %s\n",failures?"FAIL":"PASS"); return failures?1:0;
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command_line,int show)
{
    WNDCLASSA wc={0};HWND hwnd;MSG msg;(void)previous;(void)command_line;
    if(!load_config()){MessageBoxA(NULL,"Unable to load config\\digit.conf. Expected host=<server> and port=<port>.",APP_TITLE,MB_OK|MB_ICONERROR);return 1;}
    wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName="DigitGuiWindow";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassA(&wc))return 1;
    hwnd=CreateWindowExA(0,wc.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,700,530,NULL,NULL,instance,NULL);
    if(!hwnd)return 1;ShowWindow(hwnd,show);UpdateWindow(hwnd);while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return(int)msg.wParam;
}

int main(int argc,char **argv)
{
    if(argc==2&&strcmp(argv[1],"--self-test")==0)return self_test();
    return WinMain(GetModuleHandle(NULL),NULL,GetCommandLineA(),SW_SHOWDEFAULT);
}
