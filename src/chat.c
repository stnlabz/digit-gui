#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_TITLE "Digit"
#define ID_TRANSCRIPT 101
#define ID_PROMPT 102
#define ID_SEND 103
#define ID_STATUS 104
#define WM_CHAT_REPLY (WM_APP+1)
#define MAX_TEXT 4096

static HWND window, transcript, prompt, send_button, status_line;
static HFONT ui_font, heading_font;
static HBRUSH bg_brush, entry_brush;
static char server_host[256]="digit.stn-labz.com";
static INTERNET_PORT server_port=8081;
static LONG pending;

typedef struct {DWORD error,code;int ok;char answer[MAX_TEXT];} chat_result_t;
typedef struct {char question[MAX_TEXT];} chat_job_t;

static void append_chat(const char *label,const char *message){
 char text[8192];wchar_t wide[8192];int end;
 if(!transcript||!message)return;
 snprintf(text,sizeof text,"%s\n%s\n\n",label,message);
 if(!MultiByteToWideChar(CP_UTF8,0,text,-1,wide,(int)(sizeof wide/sizeof wide[0])))return;
 end=GetWindowTextLengthW(transcript);
 SendMessageW(transcript,EM_SETSEL,end,end);
 SendMessageW(transcript,EM_REPLACESEL,FALSE,(LPARAM)wide);
 SendMessageW(transcript,EM_SCROLLCARET,0,0);
}
static int extract_answer(const char *json,char *out,size_t size){
 const char *p=strstr(json,"\"answer\"");size_t n=0;
 if(!p||!size)return 0;
 p=strchr(p,':');if(!p)return 0;p++;
 while(*p==' '||*p=='\t')p++;
 if(*p++!='"')return 0;
 while(*p&&*p!='"'&&n+1<size){
  char c=*p++;
  if(c=='\\'&&*p){c=*p++;if(c=='n')c='\n';else if(c=='t')c='\t';else if(c=='r')c='\r';}
  out[n++]=c;
 }
 out[n]=0;return *p=='"';
}
static DWORD WINAPI send_worker(void *ctx){
 chat_job_t *job=(chat_job_t *)ctx;
 chat_result_t *result=(chat_result_t *)calloc(1,sizeof *result);
 HINTERNET session=NULL,connection=NULL,req=NULL;
 WCHAR hostname[256];char reply[8192]={0};
 DWORD code_size=sizeof(DWORD),available,read;size_t used=0;
 if(!result)goto cleanup;
 if(!MultiByteToWideChar(CP_UTF8,0,server_host,-1,hostname,256))goto fail;
 session=WinHttpOpen(L"Digit Chat/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                     WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
 if(!session)goto fail;
 WinHttpSetTimeouts(session,5000,5000,10000,15000);
 connection=WinHttpConnect(session,hostname,server_port,0);
 if(!connection)goto fail;
 req=WinHttpOpenRequest(connection,L"POST",L"/ask",NULL,WINHTTP_NO_REFERER,
                        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
 if(!req)goto fail;
 if(!WinHttpSendRequest(req,L"Content-Type: text/plain\r\n",(DWORD)-1L,
                       job->question,(DWORD)strlen(job->question),
                       (DWORD)strlen(job->question),0))goto fail;
 if(!WinHttpReceiveResponse(req,NULL))goto fail;
 if(!WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                         WINHTTP_HEADER_NAME_BY_INDEX,&result->code,&code_size,
                         WINHTTP_NO_HEADER_INDEX))goto fail;
 do{
  if(!WinHttpQueryDataAvailable(req,&available))goto fail;
  if(!available)break;
  if(available>=sizeof(reply)-used)goto fail;
  if(!WinHttpReadData(req,reply+used,available,&read))goto fail;
  used+=read;reply[used]=0;
 }while(read);
 if(result->code==200&&extract_answer(reply,result->answer,sizeof result->answer))
  result->ok=1;
 else if(result->code==200)
  snprintf(result->answer,sizeof result->answer,"Digit returned a response without an answer field.");
 else
  snprintf(result->answer,sizeof result->answer,"Digit returned HTTP %lu.",(unsigned long)result->code);
 goto finish;
fail:
 result->error=GetLastError();
 snprintf(result->answer,sizeof result->answer,
  "Digit is not available for chat yet (WinHTTP %lu). The Interface module currently provides TLS connections only; a chat protocol still needs to be connected through Router.",
  (unsigned long)result->error);
finish:
 if(req)WinHttpCloseHandle(req);
 if(connection)WinHttpCloseHandle(connection);
 if(session)WinHttpCloseHandle(session);
 if(!PostMessageA(window,WM_CHAT_REPLY,0,(LPARAM)result)){free(result);InterlockedExchange(&pending,0);}
cleanup:
 SecureZeroMemory(job,sizeof *job);free(job);
 return 0;
}
static void send_message(void){
 chat_job_t *job;HANDLE thread;
 if(InterlockedCompareExchange(&pending,1,0)!=0)return;
 job=(chat_job_t *)calloc(1,sizeof *job);
 if(!job){InterlockedExchange(&pending,0);return;}
 if(GetWindowTextA(prompt,job->question,sizeof job->question)<=0){
  free(job);InterlockedExchange(&pending,0);return;
 }
 append_chat("You",job->question);
 SetWindowTextW(prompt,L"");
 EnableWindow(send_button,FALSE);
 SetWindowTextA(status_line,"Waiting for Digit...");
 thread=CreateThread(NULL,0,send_worker,job,0,NULL);
 if(!thread){free(job);InterlockedExchange(&pending,0);EnableWindow(send_button,TRUE);
  SetWindowTextA(status_line,"Unable to start request.");return;}
 CloseHandle(thread);
}
static void relayout(int w,int h){
 int margin=24,bottom=24,composer_h=84,header_h=70;
 int composer_y=h-bottom-composer_h;
 if(w<420)margin=12;
 MoveWindow(transcript,margin,header_h,w-2*margin,composer_y-header_h-12,TRUE);
 MoveWindow(prompt,margin,composer_y,w-2*margin-104,composer_h-16,TRUE);
 MoveWindow(send_button,w-margin-92,composer_y+8,92,42,TRUE);
 MoveWindow(status_line,margin,h-20,w-2*margin,18,TRUE);
}
static LRESULT CALLBACK wndproc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
 switch(message){
 case WM_CREATE:{
  ui_font=CreateFontA(-19,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                      CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
  heading_font=CreateFontA(-27,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,
                           CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
  bg_brush=CreateSolidBrush(RGB(23,23,25));
  entry_brush=CreateSolidBrush(RGB(36,36,39));
  transcript=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|
    ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,0,0,0,hwnd,(HMENU)ID_TRANSCRIPT,NULL,NULL);
  prompt=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|
    ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,hwnd,(HMENU)ID_PROMPT,NULL,NULL);
  send_button=CreateWindowW(L"BUTTON",L"Send",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
    0,0,0,0,hwnd,(HMENU)ID_SEND,NULL,NULL);
  status_line=CreateWindowW(L"STATIC",L"Digit chat",WS_CHILD|WS_VISIBLE,
    0,0,0,0,hwnd,(HMENU)ID_STATUS,NULL,NULL);
  SendMessageW(transcript,WM_SETFONT,(WPARAM)ui_font,TRUE);
  SendMessageW(prompt,WM_SETFONT,(WPARAM)ui_font,TRUE);
  SendMessageW(send_button,WM_SETFONT,(WPARAM)ui_font,TRUE);
  SendMessageW(status_line,WM_SETFONT,(WPARAM)ui_font,TRUE);
  append_chat("Digit","Chat interface ready. Secure transport is available; conversation requests require a compatible Digit chat service.");
  return 0;}
 case WM_SIZE:relayout(LOWORD(lp),HIWORD(lp));return 0;
 case WM_COMMAND:
  if(LOWORD(wp)==ID_SEND&&HIWORD(wp)==BN_CLICKED){send_message();return 0;}
  return 0;
 case WM_CHAT_REPLY:{
  chat_result_t *r=(chat_result_t *)lp;
  append_chat("Digit",r->answer);
  SetWindowTextA(status_line,r->ok?"Ready":"Connection unavailable");
  free(r);InterlockedExchange(&pending,0);EnableWindow(send_button,TRUE);
  SetFocus(prompt);return 0;}
 case WM_CTLCOLOREDIT:
 case WM_CTLCOLORSTATIC:
  SetTextColor((HDC)wp,RGB(239,239,241));
  SetBkColor((HDC)wp,RGB(36,36,39));
  return (LRESULT)entry_brush;
 case WM_ERASEBKGND:{
  RECT rc;GetClientRect(hwnd,&rc);FillRect((HDC)wp,&rc,bg_brush);return 1;}
 case WM_PAINT:{
  PAINTSTRUCT ps;RECT title={24,16,500,60};
  HDC dc=BeginPaint(hwnd,&ps);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(241,241,243));
  SelectObject(dc,heading_font);DrawTextA(dc,"Digit",-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
  EndPaint(hwnd,&ps);return 0;}
 case WM_DESTROY:
  if(ui_font)DeleteObject(ui_font);if(heading_font)DeleteObject(heading_font);
  if(bg_brush)DeleteObject(bg_brush);if(entry_brush)DeleteObject(entry_brush);
  PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(hwnd,message,wp,lp);
}
int main(int argc,char **argv){
 WNDCLASSW wc={0};MSG msg;RECT rect={0,0,980,740};
 (void)argv;
 if(argc==2&&strcmp(argv[1],"--self-test")==0){
  char output[80];
  if(!extract_answer("{\"answer\":\"Hello Digit\"}",output,sizeof output)||
     strcmp(output,"Hello Digit")!=0)return 1;
  if(extract_answer("{\"message\":\"none\"}",output,sizeof output))return 1;
  puts("Digit Chat self-test: PASS");return 0;
 }
 wc.lpfnWndProc=wndproc;wc.hInstance=GetModuleHandleW(NULL);
 wc.lpszClassName=L"StnLabzDigitChat";wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 if(!RegisterClassW(&wc))return 1;
 AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
 window=CreateWindowW(wc.lpszClassName,L"Digit — Chat",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
  CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,
  NULL,NULL,wc.hInstance,NULL);
 if(!window)return 1;
 while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return (int)msg.wParam;
}
