#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib,"winhttp.lib")

#define DIGIT_GUI_VERSION "1.6.8"
#define APP_TITLE "Digit GUI 1.6.8"
#define ID_CHANNELS 1001
#define ID_OUTPUT 1002
#define ID_INPUT 1003
#define ID_SEND 1004
#define ID_STATUS 1005
#define ID_NEW_CHANNEL 1006
#define ID_ALERTS 1007
#define ID_ACK_ALERT 1008
#define ID_REFRESH 1009
#define ID_LOGIN 1010
#define ID_LOGOUT 1011
#define ID_SA_CHECK 1012
#define ID_USERNAME 1013
#define ID_PASSWORD 1014
#define ID_NEW_PROJECT 1015
#define ID_BIND_SECURITY 1016
#define ID_LIST_PROJECTS 1017
#define ID_SECURITY_GRANT 1018
#define BUFFER_MAX 65536
#define CHANNEL_MAX 128
#define ALERT_MAX 256
#define WM_DIGIT_RESULT (WM_APP+1)
#define WM_DIGIT_REFRESH (WM_APP+2)
#define ID_SYNC_TIMER 2020
#define DIGIT_SYNC_INTERVAL_MS 3000
#define ASK_RECEIVE_TIMEOUT_MS 180000

typedef struct{char id[64];char name[128];char organization[64];char project[64];} channel_item_t;
typedef struct{char id[64];char severity[16];char summary[256];int acknowledged;} alert_item_t;
typedef struct{int ok;DWORD error;DWORD http_status;char answer[4096];char channel_id[64];} digit_result_t;
typedef struct{char question[4096];char channel_id[64];} digit_request_job_t;

/* [AI:GPT-6 | 2026-10-09] Native dark palette and readable UI typography. */
static HBRUSH surface_brush,input_brush;
static HFONT ui_font;
#define DIGIT_BG RGB(20,27,41)
#define DIGIT_SURFACE RGB(31,42,60)
#define DIGIT_TEXT RGB(234,241,250)
#define DIGIT_MUTED RGB(173,190,213)
static HWND main_window,channel_list,output_box,input_box,send_button,status_text,new_channel_button,alerts_list,ack_button,refresh_button,username_box,password_box,login_button,logout_button,sa_button,new_project_button,bind_security_button,list_projects_button,security_grant_button;
static char digit_host[256]="127.0.0.1";
static INTERNET_PORT digit_port=8081;
static channel_item_t channels[CHANNEL_MAX];static size_t channel_count=0;
static alert_item_t alerts[ALERT_MAX];static size_t alert_count=0;
/* [AI:GPT-6 | 2026-10-09] No invisible legacy General selection. */
static char active_channel[64]="";
/* [AI:GPT-6 | 2026-10-08] Digit GUI 1.6.8: in-process bearer only.
 * No password or session token is written to configuration or disk. */
static char session_token[129];
static char session_identity[64];
static int session_authenticated=0;
/* [AI:GPT-6 | 2026-10-09] At most one bounded reconciliation in flight. */
static volatile LONG sync_busy=0;
static unsigned int sync_generation=0;
static char shown_channel[64]="";
static unsigned long shown_hash=0;
static unsigned long last_channel_list_hash=0;
typedef struct {unsigned int generation;int channels_ok,messages_ok;char channel_id[64];char *channels_json,*messages_json;} digit_sync_result_t;
typedef struct {unsigned int generation;char channel_id[64];} digit_sync_job_t;
static void clear_session(void){SecureZeroMemory(session_token,sizeof(session_token));SecureZeroMemory(session_identity,sizeof(session_identity));session_authenticated=0;}
/* [AI:GPT-6 | 2026-10-09] Signed-out GUI exposes no administrative controls.
 * Server-side SA policy remains the authority for all operations. */
static void set_access_controls(int enabled){
ShowWindow(sa_button,enabled?SW_SHOW:SW_HIDE);
ShowWindow(new_project_button,enabled?SW_SHOW:SW_HIDE);
ShowWindow(list_projects_button,enabled?SW_SHOW:SW_HIDE);
EnableWindow(channel_list,enabled);EnableWindow(output_box,enabled);EnableWindow(input_box,enabled);EnableWindow(send_button,enabled);EnableWindow(new_channel_button,enabled);EnableWindow(alerts_list,enabled);EnableWindow(ack_button,enabled);EnableWindow(refresh_button,enabled);EnableWindow(logout_button,enabled);EnableWindow(sa_button,enabled);EnableWindow(new_project_button,enabled);EnableWindow(bind_security_button,FALSE);EnableWindow(list_projects_button,enabled);EnableWindow(security_grant_button,FALSE);}


static void trim_line(char *text){size_t n;if(!text)return;n=strlen(text);while(n>0&&(text[n-1]=='\r'||text[n-1]=='\n'||text[n-1]==' '||text[n-1]=='\t'))text[--n]=0;}
static int config_path(char *path,size_t path_size){DWORD n=GetModuleFileNameA(NULL,path,(DWORD)path_size);char *slash;if(n==0||n>=path_size)return 0;slash=strrchr(path,'\\');if(!slash)return 0;slash[1]=0;if(strlen(path)+strlen("digit.conf")+1>path_size)return 0;strcat_s(path,path_size,"digit.conf");return 1;}
static int load_config(void){FILE *stream;char path[MAX_PATH],line[512];int have_host=0,have_port=0;if(!config_path(path,sizeof(path)))return 0;stream=fopen(path,"rb");if(!stream)return 0;while(fgets(line,sizeof(line),stream)){char *value;trim_line(line);if(!line[0]||line[0]=='#')continue;value=strchr(line,'=');if(!value)continue;*value++=0;if(strcmp(line,"host")==0){if(!value[0]||strlen(value)>=sizeof(digit_host)){fclose(stream);return 0;}strcpy_s(digit_host,sizeof(digit_host),value);have_host=1;}else if(strcmp(line,"port")==0){char *end=NULL;unsigned long port=strtoul(value,&end,10);if(!value[0]||!end||*end||port==0||port>65535){fclose(stream);return 0;}digit_port=(INTERNET_PORT)port;have_port=1;}}fclose(stream);return have_host&&have_port;}
/* [AI:GPT-6 | 2026-10-09] Win32 EDIT requires CRLF, not Unix LF. */
static void append_output(const char *speaker,const char *text){
    char line[8192];WCHAR wide[8192];size_t n=0,i;int length;
    const char *parts[2]={speaker,text};
    for(i=0;i<2;i++){
        const char *p=parts[i];
        if(i==0){while(*p&&n+2<sizeof(line))line[n++]=*p++;line[n++]=':';line[n++]=' ';}
        else while(*p&&n+4<sizeof(line)){
            if(*p=='\r'){line[n++]='\r';if(p[1]=='\n'){line[n++]='\n';++p;}}
            else if(*p=='\n'){line[n++]='\r';line[n++]='\n';}
            else line[n++]=*p;
            ++p;
        }
    }
    line[n++]='\r';line[n++]='\n';line[n++]='\r';line[n++]='\n';line[n]=0;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line,-1,wide,(int)(sizeof(wide)/sizeof(wide[0]))))return;
    length=GetWindowTextLengthW(output_box);
    SendMessageW(output_box,EM_SETSEL,(WPARAM)length,(LPARAM)length);
    SendMessageW(output_box,EM_REPLACESEL,FALSE,(LPARAM)wide);
}
static int json_string_after(const char *start,const char *key,char *out,size_t n){char pattern[128];const char *p;size_t o=0;snprintf(pattern,sizeof(pattern),"\"%s\":\"",key);p=strstr(start,pattern);if(!p)return 0;p+=strlen(pattern);while(*p&&o+1<n){if(*p=='"')break;if(*p=='\\'&&p[1]){++p;if(*p=='n')out[o++]='\n';else if(*p=='t')out[o++]='\t';else if(*p!='r')out[o++]=*p;++p;continue;}out[o++]=*p++;}out[o]=0;return *p=='"';}
static int extract_answer(const char *json,char *answer,size_t n){return json_string_after(json,"answer",answer,n);}
static int digit_request(const char *method,const char *path,const char *body,char *response,size_t response_size,DWORD receive_timeout,DWORD *error_out,DWORD *status_out){WCHAR host_w[256],path_w[512],method_w[16];HINTERNET session=NULL,connection=NULL,request=NULL;DWORD status=0,status_size=sizeof(status),available=0,read=0;size_t used=0;int ok=0;DWORD error=ERROR_SUCCESS;if(error_out)*error_out=0;if(status_out)*status_out=0;if(!MultiByteToWideChar(CP_UTF8,0,digit_host,-1,host_w,256)||!MultiByteToWideChar(CP_UTF8,0,path,-1,path_w,512)||!MultiByteToWideChar(CP_UTF8,0,method,-1,method_w,16)){error=GetLastError();goto done;}session=WinHttpOpen(L"Digit GUI/1.6.8",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session){error=GetLastError();goto done;}if(!WinHttpSetTimeouts(session,5000,5000,10000,(int)receive_timeout)){error=GetLastError();goto done;}connection=WinHttpConnect(session,host_w,digit_port,0);if(!connection){error=GetLastError();goto done;}request=WinHttpOpenRequest(connection,method_w,path_w,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request){error=GetLastError();goto done;}/* [AI:GPT-6 | 2026-10-08] Attach bearer only for authenticated API calls;
 * login never transmits a previous token. */
if(session_authenticated && strcmp(path,"/session/login")!=0){
    WCHAR auth_header[240];
    if(swprintf_s(auth_header,sizeof(auth_header)/sizeof(auth_header[0]),L"Authorization: Bearer %hs\r\n",session_token)<0 ||
       !WinHttpAddRequestHeaders(request,auth_header,(DWORD)-1L,WINHTTP_ADDREQ_FLAG_ADD)){
        error=GetLastError();goto done;
    }
}
if(!WinHttpSendRequest(request,body?L"Content-Type: text/plain\r\n":WINHTTP_NO_ADDITIONAL_HEADERS,body?(DWORD)-1L:0,body?(LPVOID)body:WINHTTP_NO_REQUEST_DATA,body?(DWORD)strlen(body):0,body?(DWORD)strlen(body):0,0)){error=GetLastError();goto done;}if(!WinHttpReceiveResponse(request,NULL)){error=GetLastError();goto done;}if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&status_size,WINHTTP_NO_HEADER_INDEX)){error=GetLastError();goto done;}if(status_out)*status_out=status;/* [AI:GPT-6 | 2026-10-09] Read bounded JSON error bodies as well as 200 replies for operator diagnostics. */
if(!response||response_size<2){error=ERROR_INSUFFICIENT_BUFFER;goto done;}response[0]=0;do{if(!WinHttpQueryDataAvailable(request,&available)){error=GetLastError();goto done;}if(!available)break;if(available>response_size-used-1)available=(DWORD)(response_size-used-1);if(!available){error=ERROR_INSUFFICIENT_BUFFER;goto done;}if(!WinHttpReadData(request,response+used,available,&read)){error=GetLastError();goto done;}used+=read;response[used]=0;}while(read>0);ok=(status==200);done:if(error_out)*error_out=error;if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);return ok;}
static void refresh_all(void);
/* [AI:GPT-6 | 2026-10-08] GUI requires validated WinHTTP HTTPS transport;
 * certificates must match the configured host and trusted issuer. */
static int local_transport(void){return digit_host[0]!=0;}
static void do_login(void){
    char username[64],password[256],body[384],response[1024],token[129];
    DWORD error=0,status=0;
    if(!local_transport()){
        MessageBoxA(main_window,"A Digit server hostname is required for HTTPS.",APP_TITLE,MB_OK|MB_ICONWARNING);
        return;
    }
    if(GetWindowTextA(username_box,username,sizeof(username))<=0 || GetWindowTextA(password_box,password,sizeof(password))<=0){
        MessageBoxA(main_window,"Enter an account identity and password.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;
    }
    if(strchr(username,'\t')||strchr(username,'\r')||strchr(username,'\n')||strchr(password,'\t')||strchr(password,'\r')||strchr(password,'\n')){
        SecureZeroMemory(password,sizeof(password));return;
    }
    snprintf(body,sizeof(body),"%s\t%s",username,password);
    SecureZeroMemory(password,sizeof(password));
    clear_session();
    {int ok=digit_request("POST","/session/login",body,response,sizeof(response),10000,&error,&status);
     SecureZeroMemory(body,sizeof(body));
     if(!ok){
         char diagnostic[160];
         if(status!=0)
             snprintf(diagnostic,sizeof(diagnostic),"Login rejected: HTTP %lu",(unsigned long)status);
         else
             snprintf(diagnostic,sizeof(diagnostic),"HTTPS request failed: WinHTTP %lu",(unsigned long)error);
         SecureZeroMemory(token,sizeof(token));
         SetWindowTextA(status_text,diagnostic);
         MessageBoxA(main_window,diagnostic,APP_TITLE,MB_OK|MB_ICONWARNING);
         return;
     }
     if(!json_string_after(response,"token",token,sizeof(token))||strlen(token)!=64){
         SecureZeroMemory(token,sizeof(token));
         SetWindowTextA(status_text,"Login HTTP 200 but session token invalid");
         MessageBoxA(main_window,"Login succeeded but GUI could not parse the session token.",APP_TITLE,MB_OK|MB_ICONWARNING);
         return;
     }}
    strcpy_s(session_token,sizeof(session_token),token);
    SecureZeroMemory(token,sizeof(token));
    strcpy_s(session_identity,sizeof(session_identity),username);
    session_authenticated=1;
    ++sync_generation;
    shown_channel[0]=0;last_channel_list_hash=0;
    SetTimer(main_window,ID_SYNC_TIMER,DIGIT_SYNC_INTERVAL_MS,NULL);
    SetWindowTextA(password_box,"");
    EnableWindow(login_button,FALSE);
    set_access_controls(TRUE);
    SetWindowTextA(status_text,"Authenticated with Digit");
    refresh_all();
}
static void do_logout(void){
    char response[256];DWORD error,status;
    if(session_authenticated)(void)digit_request("POST","/session/logout","",response,sizeof(response),10000,&error,&status);
    KillTimer(main_window,ID_SYNC_TIMER);++sync_generation;shown_channel[0]=0;last_channel_list_hash=0;
    clear_session();set_access_controls(FALSE);EnableWindow(login_button,TRUE);
    SendMessageA(channel_list,LB_RESETCONTENT,0,0);SendMessageA(alerts_list,LB_RESETCONTENT,0,0);
    active_channel[0]=0;channel_count=0;alert_count=0;
    SetWindowTextW(output_box,L"");SetWindowTextA(status_text,"Signed out");
}
/* [AI:GPT-6 | 2026-10-08] Interface 1.6.8 read-only SA dashboard.
 * Show server-returned aggregates; never infer authority from the GUI. */
static void check_sa(void){
    char response[1024],dialog[512];
    DWORD error=0,status=0;
    unsigned long channels=0,alerts=0,unacknowledged=0;
    if(!session_authenticated)return;
    if(!digit_request("GET","/admin/dashboard",NULL,response,sizeof(response),10000,&error,&status)){
        if(status==403)MessageBoxA(main_window,"Digit Core denied SA dashboard access.",APP_TITLE,MB_OK|MB_ICONWARNING);
        else if(status==401)MessageBoxA(main_window,"Session expired. Log in again.",APP_TITLE,MB_OK|MB_ICONWARNING);
        else {
            snprintf(dialog,sizeof(dialog),"Dashboard unavailable (HTTP %lu, WinHTTP %lu).",(unsigned long)status,(unsigned long)error);
            MessageBoxA(main_window,dialog,APP_TITLE,MB_OK|MB_ICONWARNING);
        }
        return;
    }
    if(!strstr(response,"\"authorized\":true")||!strstr(response,"\"scope\":\"digit-operations-read\"")||
       sscanf(strstr(response,"\"channels\":")?strstr(response,"\"channels\":"):"", "\"channels\":%lu",&channels)!=1 ||
       sscanf(strstr(response,"\"alerts\":")?strstr(response,"\"alerts\":"):"", "\"alerts\":%lu",&alerts)!=1 ||
       sscanf(strstr(response,"\"unacknowledged_alerts\":")?strstr(response,"\"unacknowledged_alerts\":"):"", "\"unacknowledged_alerts\":%lu",&unacknowledged)!=1){
        MessageBoxA(main_window,"Invalid dashboard response.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    snprintf(dialog,sizeof(dialog),"Digit SA Operational Dashboard (read-only)\n\nChannels: %lu\nAlerts: %lu\nUnacknowledged alerts: %lu",channels,alerts,unacknowledged);
    MessageBoxA(main_window,dialog,APP_TITLE,MB_OK|MB_ICONINFORMATION);
}
static void check_health(void){char response[1024];DWORD e,s;if(digit_request("GET","/health",NULL,response,sizeof(response),10000,&e,&s)&&strstr(response,"READY"))SetWindowTextA(status_text,"Connected");else SetWindowTextA(status_text,"Offline");}
/* [AI:GPT-6 | 2026-10-09] Build organization > project > channel
 * navigation using server-verified owner metadata. Heading rows never route. */
static void channel_navigation(const char *json){
 const char *p=json;
 size_t count=0,i,j,selection=(size_t)-1;
 char id[64],name[128],scope[64],project[64],record[1024],display[240];
 SendMessageA(channel_list,LB_RESETCONTENT,0,0);
 while(count<CHANNEL_MAX&&(p=strstr(p,"\"id\":\""))!=NULL){
  const char *end=strchr(p,'}');
  size_t n;
  if(!end||end<p||((n=(size_t)(end-p))>=sizeof(record)))break;
  memcpy(record,p,n);record[n]=0;
  if(!json_string_after(record,"id",id,sizeof(id))||
     !json_string_after(record,"name",name,sizeof(name)))break;
  scope[0]=project[0]=0;
  json_string_after(record,"organization",scope,sizeof(scope));
  json_string_after(record,"project",project,sizeof(project));
  /* [AI:GPT-6 | 2026-10-09] Legacy unscoped Alerts is a
   * STN-LABZ-only presentation. Never associate it with Team ChAoS.
   * Scope authority remains entirely server-side. */
  if(!scope[0]&&(!strcmp(name,"Alerts")||
      !strcmp(name,"alerts-stn-labz-operations"))){
   const char *orgs=strstr(json,"\"organizations\":[");
   const char *end=orgs?strchr(orgs,']'):NULL;
   const char *stn=orgs?strstr(orgs,"\"stn-labz\""):NULL;
   if(stn&&end&&stn<end){
    strcpy_s(scope,sizeof(scope),"stn-labz");
    strcpy_s(project,sizeof(project),"operations");
   }
  }
  strcpy_s(channels[count].id,sizeof(channels[count].id),id);
  strcpy_s(channels[count].name,sizeof(channels[count].name),name);
  strcpy_s(channels[count].organization,sizeof(channels[count].organization),scope);
  strcpy_s(channels[count].project,sizeof(channels[count].project),project);
  ++count;p=end+1;
 }
 /* [AI:GPT-6 | 2026-10-09] An old unscoped Alerts channel can
  * coexist with the server-bound STN-LABZ Alerts projection. Display only
  * the bound channel when it exists; never misclassify the orphan by name.
  * This is display filtering only: no channel or ACL is modified. */
 {
  int bound_alerts=0;
  size_t read_index,write_index=0;
  for(i=0;i<count;i++)
   if(!strcmp(channels[i].organization,"stn-labz")&&
      !strcmp(channels[i].project,"operations")&&
      (!strcmp(channels[i].name,"Alerts")||
       !strncmp(channels[i].name,"alerts-",7)))bound_alerts=1;
  for(read_index=0;read_index<count;read_index++){
   int legacy_alert=!channels[read_index].organization[0]&&
      (!strcmp(channels[read_index].name,"Alerts")||
       !strcmp(channels[read_index].name,"alerts")||
       !strncmp(channels[read_index].name,"alerts-",7));
   if(bound_alerts&&legacy_alert){
    if(!strcmp(active_channel,channels[read_index].id))active_channel[0]=0;
    continue;
   }
   if(write_index!=read_index)channels[write_index]=channels[read_index];
   ++write_index;
  }
  count=write_index;
 }
 channel_count=count;
 /* Project group headers are displayed only for authoritative channel owners.
  * Unbound legacy channels remain visible in a separate neutral section. */
 for(i=0;i<count;i++){
  int already=0;const char *org=channels[i].organization;
  for(j=0;j<i;j++)if(strcmp(org,channels[j].organization)==0){already=1;break;}
  if(already)continue;
  snprintf(display,sizeof(display),"%s",!strcmp(org,"stn-labz")?"STN-LABZ":
           !strcmp(org,"team-chaos")?"Team ChAoS":org[0]?org:"Other Channels");
  {LRESULT row=SendMessageA(channel_list,LB_ADDSTRING,0,(LPARAM)display);
   SendMessageA(channel_list,LB_SETITEMDATA,(WPARAM)row,(LPARAM)-1);}
  for(j=0;j<count;j++){
   size_t k;
   const char *proj=channels[j].project;
   if(strcmp(channels[j].organization,org)!=0)continue;
   for(k=0;k<j;k++)
    if(!strcmp(channels[k].organization,org)&&!strcmp(channels[k].project,proj))break;
   if(k!=j)continue;
   if(proj[0]){
    snprintf(display,sizeof(display),"   %s",proj);
    {LRESULT row=SendMessageA(channel_list,LB_ADDSTRING,0,(LPARAM)display);
     SendMessageA(channel_list,LB_SETITEMDATA,(WPARAM)row,(LPARAM)-1);}
   }
   for(k=0;k<count;k++){
    LRESULT row;
    const char *label;
    if(strcmp(channels[k].organization,org)||strcmp(channels[k].project,proj))continue;
    label=strncmp(channels[k].name,"security-",9)==0?"Security":
          strncmp(channels[k].name,"alerts-",7)==0?"Alerts":channels[k].name;
    snprintf(display,sizeof(display),"%s%s",proj[0]?"      ":"   ",label);
    row=SendMessageA(channel_list,LB_ADDSTRING,0,(LPARAM)display);
    if(row==LB_ERR||row==LB_ERRSPACE)continue;
    SendMessageA(channel_list,LB_SETITEMDATA,(WPARAM)row,(LPARAM)k);
    if(strcmp(channels[k].id,active_channel)==0)selection=(size_t)row;
   }
  }
 }
 /* Show independently authorized organizations even with no bound channels. */
 {
  const char *start=strstr(json,"\"organizations\":[");
  if(start){
   const char *end=strchr(start,']');
   const char *orgs[2]={"stn-labz","team-chaos"};
   size_t oi;
   for(oi=0;oi<2;oi++){
    char token[80];int shown=0;
    snprintf(token,sizeof(token),"\"%s\"",orgs[oi]);
    if(!end||!strstr(start,token)||strstr(start,token)>end)continue;
    for(i=0;i<count;i++)if(strcmp(channels[i].organization,orgs[oi])==0){shown=1;break;}
    if(!shown){
     LRESULT row=SendMessageA(channel_list,LB_ADDSTRING,0,
                              (LPARAM)(oi==0?"STN-LABZ":"Team ChAoS"));
     if(row!=LB_ERR&&row!=LB_ERRSPACE){
      SendMessageA(channel_list,LB_SETITEMDATA,(WPARAM)row,(LPARAM)-1);
      row=SendMessageA(channel_list,LB_ADDSTRING,0,(LPARAM)"   No channels provisioned");
      if(row!=LB_ERR&&row!=LB_ERRSPACE)
       SendMessageA(channel_list,LB_SETITEMDATA,(WPARAM)row,(LPARAM)-1);
     }
    }
   }
  }
 }
 if(selection==(size_t)-1){
  active_channel[0]=0;
  for(i=0;i<(size_t)SendMessageA(channel_list,LB_GETCOUNT,0,0);i++){
   LRESULT index=SendMessageA(channel_list,LB_GETITEMDATA,(WPARAM)i,0);
   if(index!=LB_ERR&&index>=0&&(size_t)index<count){
    selection=i;strcpy_s(active_channel,sizeof(active_channel),channels[index].id);break;
   }
  }
 }
 if(selection!=(size_t)-1)SendMessageA(channel_list,LB_SETCURSEL,(WPARAM)selection,0);
}
static void load_channels(void){
 char response[BUFFER_MAX];DWORD e,s;
 if(digit_request("GET","/channels",NULL,response,sizeof(response),10000,&e,&s))
  channel_navigation(response);
}
static void load_history(void){char path[256],response[BUFFER_MAX],origin[64],body[4096];DWORD e,s;const char *p;if(!active_channel[0])return;snprintf(path,sizeof(path),"/channels/%s/messages",active_channel);SetWindowTextW(output_box,L"");if(!digit_request("GET",path,NULL,response,sizeof(response),10000,&e,&s))return;p=response;while((p=strstr(p,"\"origin\":\""))!=NULL){if(!json_string_after(p,"origin",origin,sizeof(origin))||!json_string_after(p,"body",body,sizeof(body)))break;append_output(strcmp(origin,"digit")==0?"Digit":strcmp(origin,"operator")==0?"You":origin,body);p+=10;}}
static void load_alerts(void){char response[BUFFER_MAX],id[64],severity[16],summary[256],display[384];DWORD e,s;const char *p;size_t count=0;SendMessageA(alerts_list,LB_RESETCONTENT,0,0);if(!digit_request("GET","/alerts",NULL,response,sizeof(response),10000,&e,&s))return;p=response;while(count<ALERT_MAX&&(p=strstr(p,"\"id\":\""))!=NULL){if(!json_string_after(p,"id",id,sizeof(id))||!json_string_after(p,"severity",severity,sizeof(severity))||!json_string_after(p,"summary",summary,sizeof(summary)))break;strcpy_s(alerts[count].id,sizeof(alerts[count].id),id);strcpy_s(alerts[count].severity,sizeof(alerts[count].severity),severity);strcpy_s(alerts[count].summary,sizeof(alerts[count].summary),summary);{
const char *next=strstr(p+6,"\"id\":\"");
const char *ack=strstr(p,"\"acknowledged\":true");
alerts[count].acknowledged=(ack && (!next || ack<next))?1:0;
}
snprintf(display,sizeof(display),"[%s]%s %s",severity,alerts[count].acknowledged?" [ACK]":"",summary);SendMessageA(alerts_list,LB_ADDSTRING,0,(LPARAM)display);++count;p+=6;}alert_count=count;/* [AI:GPT-6 | 2026-10-09] Alert retrieval does not override connection status. */}
/* [AI:GPT-6 | 2026-10-09] Poll only authenticated state on a worker.
 * UI changes are applied on the owning window thread, without disturbing input. */
static unsigned long sync_hash(const char *s){
    unsigned long h=2166136261UL;
    while(*s){h^=(unsigned char)*s++;h*=16777619UL;}return h;
}
static DWORD WINAPI sync_worker(LPVOID param){
    digit_sync_job_t *job=(digit_sync_job_t *)param;
    digit_sync_result_t *result=(digit_sync_result_t *)calloc(1,sizeof(*result));
    DWORD error=0,status=0;
    char path[256];
    if(result){
        result->generation=job->generation;
        strcpy_s(result->channel_id,sizeof(result->channel_id),job->channel_id);
        result->channels_json=(char *)calloc(1,BUFFER_MAX);
        result->messages_json=(char *)calloc(1,BUFFER_MAX);
        if(result->channels_json&&result->messages_json){
            result->channels_ok=digit_request("GET","/channels",NULL,result->channels_json,BUFFER_MAX,10000,&error,&status);
            if(result->channels_ok&&job->channel_id[0]){
                snprintf(path,sizeof(path),"/channels/%s/messages",job->channel_id);
                result->messages_ok=digit_request("GET",path,NULL,result->messages_json,BUFFER_MAX,10000,&error,&status);
            }
        }
        if(!PostMessageA(main_window,WM_DIGIT_REFRESH,0,(LPARAM)result)){
            free(result->channels_json);free(result->messages_json);free(result);
            InterlockedExchange(&sync_busy,0);
        }
    }else InterlockedExchange(&sync_busy,0);
    free(job);
    return 0;
}
static void start_sync(void){
    digit_sync_job_t *job;HANDLE worker;
    if(!session_authenticated||InterlockedCompareExchange(&sync_busy,1,0)!=0)return;
    job=(digit_sync_job_t *)calloc(1,sizeof(*job));
    if(!job){InterlockedExchange(&sync_busy,0);return;}
    job->generation=sync_generation;
    strcpy_s(job->channel_id,sizeof(job->channel_id),active_channel);
    worker=CreateThread(NULL,0,sync_worker,job,0,NULL);
    if(!worker){free(job);InterlockedExchange(&sync_busy,0);return;}
    CloseHandle(worker);
}
static void apply_sync(digit_sync_result_t *result){
    if(result->generation==sync_generation&&session_authenticated){
        if(result->channels_ok){
            unsigned long current=sync_hash(result->channels_json);
            if(current!=last_channel_list_hash){
                channel_navigation(result->channels_json);
                last_channel_list_hash=current;
            }
        }
        if(result->messages_ok&&strcmp(result->channel_id,active_channel)==0){
            unsigned long hash=sync_hash(result->messages_json);
            if(strcmp(shown_channel,active_channel)!=0||hash!=shown_hash){
                const char *p=result->messages_json;
                char origin[64],body[4096];
                SetWindowTextW(output_box,L"");
                while((p=strstr(p,"\"origin\":\""))!=NULL){
                    if(!json_string_after(p,"origin",origin,sizeof(origin))||
                       !json_string_after(p,"body",body,sizeof(body)))break;
                    append_output(strcmp(origin,"digit")==0?"Digit":
                                  strcmp(origin,"operator")==0?"You":origin,body);
                    p+=10;
                }
                shown_hash=hash;
                strcpy_s(shown_channel,sizeof(shown_channel),active_channel);
            }
        }
    }
    free(result->channels_json);free(result->messages_json);free(result);
    InterlockedExchange(&sync_busy,0);
}
static void refresh_all(void){if(!session_authenticated)return;check_health();load_channels();load_history();start_sync();}
static DWORD WINAPI ask_worker(LPVOID parameter){digit_request_job_t *job=(digit_request_job_t *)parameter;digit_result_t *result=(digit_result_t *)calloc(1,sizeof(*result));char response[BUFFER_MAX],path[256];if(!result){free(job);return 1;}strcpy_s(result->channel_id,sizeof(result->channel_id),job->channel_id);snprintf(path,sizeof(path),"/channels/%s/ask",job->channel_id);result->ok=digit_request("POST",path,job->question,response,sizeof(response),ASK_RECEIVE_TIMEOUT_MS,&result->error,&result->http_status);if(result->ok&&!extract_answer(response,result->answer,sizeof(result->answer))){result->ok=0;result->error=ERROR_INVALID_DATA;}free(job);PostMessageA(main_window,WM_DIGIT_RESULT,0,(LPARAM)result);return 0;}
/* [AI:GPT-6 | 2026-10-09] Alert acknowledgement from the existing chat
 * input, with an exact Core-issued ID and server-side validation. */
static int chat_acknowledge(const char *question){
    const char *id=question+5;
    char path[256],reply[1024]={0},diagnostic[256];
    DWORD error=0,status=0;
    size_t i,n;
    if(strncmp(question,"/ack ",5)!=0)return 0;
    n=strlen(id);
    if(!n||n>=128){append_output("Digit GUI","Usage: /ack <alert-id>");return 1;}
    for(i=0;i<n;++i){
        char c=id[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='-'||c=='_')){
            append_output("Digit GUI","Invalid alert ID.");return 1;
        }
    }
    snprintf(path,sizeof(path),"/alerts/%s/acknowledge",id);
    if(digit_request("POST",path,"",reply,sizeof(reply),10000,&error,&status)&&
       strstr(reply,"\"acknowledged\":true")){
        SetWindowTextA(input_box,"");
        load_history();
    }else{
        snprintf(diagnostic,sizeof(diagnostic),"Acknowledgement failed: HTTP %lu / WinHTTP %lu.",
                 (unsigned long)status,(unsigned long)error);
        append_output("Digit GUI",diagnostic);
    }
    return 1;
}
/* [AI:GPT-6 | 2026-10-09] Operator SA commands use the exact server
 * authorization boundary. The GUI does not grant or infer SA authority. */
/* [AI:GPT-6 | 2026-10-09] Human-readable SA projection; server remains authoritative. */
static void sa_pretty(const char *command,const char *org,const char *user,const char *reply){
    char msg[1024],name[128],role[32];
    const char *cursor;
    if(!strcmp(command,"list")){
        snprintf(msg,sizeof(msg),"SECURITY ADMINISTRATION - %s\n----------------------------------------",org);
        append_output("Digit",msg);
        cursor=strstr(reply,"\"assignments\":[");
        if(!cursor){append_output("Digit","Unable to display the administrator roster.");return;}
        cursor=strchr(cursor,'[')+1;
        if(*cursor==']'){append_output("Digit","No administrator records found.");return;}
        while((cursor=strstr(cursor,"\"user\":\""))!=NULL){
            const char *end=strchr(cursor,'}');
            if(!end)break;
            if(!json_string_after(cursor,"user",name,sizeof(name))||
               !json_string_after(cursor,"role",role,sizeof(role)))break;
            {
                char item[1024];
                size_t length=(size_t)(end-cursor);
                int qualified=0,assigned=0,mission=0;
                char record[1024];
                if(length>=sizeof(record))break;
                memcpy(record,cursor,length);record[length]=0;
                qualified=strstr(record,"\"qualified\":true")!=NULL;
                assigned=strstr(record,"\"assigned\":true")!=NULL;
                mission=strstr(record,"\"mission_qualified\":true")!=NULL;
                snprintf(item,sizeof(item),
                         "User: %s\nRole: %s\nQualification: %s\nAssignment: %s\nMission qualification: %s",
                         name,!strcmp(role,"SA")?"Security Administrator":role,
                         qualified?"Verified":"Not verified",
                         assigned?"Active":"Inactive",
                         mission?"Verified":"Not verified");
                append_output("Digit",item);
            }
            cursor=end+1;
            if(*cursor==']')break;
        }
    }else if(strstr(reply,"\"bootstrapped\":true")||strstr(reply,"\"updated\":true")){
        snprintf(msg,sizeof(msg),"%s: %s - %s (%s).",
            !strcmp(command,"revoke")?"Access revoked":"Success",
            user,org,!strcmp(command,"revoke")?"administrator assignment removed":
            "security administrator assignment active");
        append_output("Digit",msg);
    }else append_output("Digit","Administrator request completed; response was not recognized.");
}
static int chat_sa_command(const char *text){
    char command[12],org[64],user[64]="",body[160],reply[8192]={0},msg[256];
    DWORD error=0,status=0;
    int count;
    if(strncmp(text,"/sa ",4)!=0)return 0;
    count=sscanf_s(text+4,"%11s %63s %63s",
       command,(unsigned)sizeof(command),org,(unsigned)sizeof(org),
       user,(unsigned)sizeof(user));
    if(count<2||count>3||
       (!strcmp(command,"list")&&count!=2)||
       ((!strcmp(command,"assign")||!strcmp(command,"revoke")||!strcmp(command,"bootstrap"))&&count!=3)||
       (strcmp(command,"list")&&strcmp(command,"assign")&&strcmp(command,"revoke")&&strcmp(command,"bootstrap"))){
        append_output("Digit GUI","Usage: /sa list org | /sa assign org user | /sa revoke org user | /sa bootstrap team-chaos poemei");
        return 1;
    }
    {
        const char *parts[2]={org,user};
        size_t j;
        for(j=0;j<2;j++){const char *p=parts[j];while(*p){
            char c=*p++;
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                 (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')){
                append_output("Digit GUI","Invalid SA command identifier.");return 1;
            }
        }}
    }
    snprintf(body,sizeof(body),"%s\t%s\t%s",command,org,user);
    if(digit_request("POST","/admin/sa",body,reply,sizeof(reply),10000,&error,&status)){
        SetWindowTextA(input_box,"");
        sa_pretty(command,org,user,reply);
    }else{
        snprintf(msg,sizeof(msg),"SA command denied: HTTP %lu / WinHTTP %lu. %.120s",
                 (unsigned long)status,(unsigned long)error,reply);
        append_output("Digit GUI",msg);
    }
    SecureZeroMemory(body,sizeof(body));
    return 1;
}
static void send_question(void){digit_request_job_t *job;HANDLE thread;char question[4096];GetWindowTextA(input_box,question,sizeof(question));if(!question[0])return;if(strncmp(question,"/ack ",5)==0){if(chat_acknowledge(question))return;}if(strncmp(question,"/sa ",4)==0){if(chat_sa_command(question))return;}if(!active_channel[0])return;
job=(digit_request_job_t *)calloc(1,sizeof(*job));if(!job){append_output("Digit GUI","Unable to allocate request.");return;}strcpy_s(job->question,sizeof(job->question),question);strcpy_s(job->channel_id,sizeof(job->channel_id),active_channel);append_output("You",question);SetWindowTextA(input_box,"");SetWindowTextA(status_text,"Waiting for Digit...");EnableWindow(send_button,FALSE);thread=CreateThread(NULL,0,ask_worker,job,0,NULL);if(!thread){free(job);EnableWindow(send_button,TRUE);append_output("Digit GUI","Unable to start request thread.");return;}CloseHandle(thread);}
static void create_channel(void){char name[128],response[2048];DWORD e,s;if(DialogBoxParamA(NULL,NULL,main_window,NULL,0))return;(void)name;(void)response;(void)e;(void)s;}
static void prompt_new_channel(void){char name[128]="";if(GetWindowTextA(input_box,name,sizeof(name))<=0){MessageBoxA(main_window,"Type the new channel name in the input box, then click New Channel.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;}if(name[0]){char response[2048];DWORD e,s;if(digit_request("POST","/channels",name,response,sizeof(response),10000,&e,&s)){SetWindowTextA(input_box,"");load_channels();MessageBoxA(main_window,"Channel created. Channel visibility requires a separately authorized ACL assignment.",APP_TITLE,MB_OK|MB_ICONINFORMATION);}else {char msg[180];snprintf(msg,sizeof(msg),"Channel creation failed: HTTP %lu, WinHTTP %lu.",(unsigned long)s,(unsigned long)e);MessageBoxA(main_window,msg,APP_TITLE,MB_OK|MB_ICONERROR);}}}
/* [AI:GPT-6 | 2026-10-09] GUI 1.6.8: SA provisioning.
 * User enters organization/project; the server authenticates identity
 * and verifies organization-specific SA assignment. */
/* [AI:GPT-6 | 2026-10-09] One-action operator workflow.
 * Core IDs and protected TSV records never appear in the input.
 * The server validates org SA, project, binding, and final ACL. */
static void setup_project_security(void){
    char scope[160],body[160],reply[512]={0},message[384];
    char *slash;size_t i;DWORD error=0,status=0;
    if(!session_authenticated)return;
    if(GetWindowTextA(input_box,scope,sizeof(scope))<=0){
        MessageBoxA(main_window,"Enter organization/project (for example stn-labz/operations).",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;
    }
    slash=strchr(scope,'/');
    if(!slash||slash==scope||!slash[1]||strchr(slash+1,'/')){
        MessageBoxA(main_window,"Expected organization/project.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    for(i=0;scope[i];i++){
        unsigned char c=(unsigned char)scope[i];
        if(c=='/')continue;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='-'||c=='_')){
            MessageBoxA(main_window,"Invalid project identifier.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
        }
    }
    *slash=0;
    if(snprintf(body,sizeof(body),"%s\t%s",scope,slash+1)<=0)return;
    if(digit_request("POST","/admin/security/setup",body,reply,sizeof(reply),10000,&error,&status) &&
       strstr(reply,"\"ready\":true")!=NULL){
        SetWindowTextA(input_box,"");
        refresh_all();
        MessageBoxA(main_window,"Security and Alerts ready.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
    }else{
        snprintf(message,sizeof(message),"Security setup incomplete: HTTP %lu / WinHTTP %lu.\n%.180s",
                 (unsigned long)status,(unsigned long)error,reply[0]?reply:"No response body");
        MessageBoxA(main_window,message,APP_TITLE,MB_OK|MB_ICONWARNING);
    }
    SecureZeroMemory(body,sizeof(body));
}
static void prompt_new_project(void){
    char scope[160],body[160],response[512],message[256];
    char *slash;
    DWORD error=0,status=0;
    if(!session_authenticated)return;
    if(GetWindowTextA(input_box,scope,sizeof(scope))<=0){
        MessageBoxA(main_window,"Enter organization/project in the input box.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;
    }
    slash=strchr(scope,'/');
    if(!slash || slash==scope || !slash[1] || strchr(slash+1,'/')){
        MessageBoxA(main_window,"Use organization/project (example: stn-labz/operations).",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    *slash=0;
    if(snprintf(body,sizeof(body),"%s\t%s",scope,slash+1)<=0)return;
    if(digit_request("POST","/admin/projects",body,response,sizeof(response),10000,&error,&status)){
        SetWindowTextA(input_box,"");
        MessageBoxA(main_window,"Project security workspace created. Channel binding and access grants remain separate protected operations.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
    }else{
        snprintf(message,sizeof(message),"Project provisioning failed: HTTP %lu / WinHTTP %lu.",(unsigned long)status,(unsigned long)error);
        MessageBoxA(main_window,message,APP_TITLE,MB_OK|MB_ICONWARNING);
    }
    SecureZeroMemory(body,sizeof(body));
}
/* [AI:GPT-6 | 2026-10-09] Explicit SA security binding uses
 * the already qualified server-side project membership checks. */
static void prompt_bind_security(void){
    char scope[160],path[240],response[512],message[256],*slash;
    DWORD error=0,status=0;
    if(!session_authenticated)return;
    if(GetWindowTextA(input_box,scope,sizeof(scope))<=0)return;
    slash=strchr(scope,'/');
    if(!slash || slash==scope || !slash[1] || strchr(slash+1,'/')){
        MessageBoxA(main_window,"Enter organization/project in the input box.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    if(snprintf(path,sizeof(path),"/projects/%s/security/bind",scope)<=0)return;
    if(digit_request("POST",path,"",response,sizeof(response),10000,&error,&status))
        MessageBoxA(main_window,"Project Security channel bound. Channel visibility still requires a separate authorized grant.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
    else{
        snprintf(message,sizeof(message),"Security binding failed: HTTP %lu / WinHTTP %lu.",(unsigned long)status,(unsigned long)error);
        MessageBoxA(main_window,message,APP_TITLE,MB_OK|MB_ICONWARNING);
    }
}
/* [AI:GPT-6 | 2026-10-09] 1.6.8: enumerate one explicitly
 * selected organization, with authorization enforced on the server. */
static void prompt_list_projects(void){
    char org[64],path[200],response[8192],message[256];
    DWORD error=0,status=0;
    size_t i;
    if(!session_authenticated)return;
    if(GetWindowTextA(input_box,org,sizeof(org))<=0){
        MessageBoxA(main_window,"Enter an organization ID, e.g. stn-labz, in the input box.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;
    }
    for(i=0;org[i];i++)
        if(!((org[i]>='a'&&org[i]<='z')||(org[i]>='A'&&org[i]<='Z')||
             (org[i]>='0'&&org[i]<='9')||org[i]=='-'||org[i]=='_')){
            MessageBoxA(main_window,"Invalid organization identifier.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
        }
    snprintf(path,sizeof(path),"/admin/projects?organization=%s",org);
    if(digit_request("GET",path,NULL,response,sizeof(response),10000,&error,&status)){
        append_output("Projects",response);
    }else{
        snprintf(message,sizeof(message),"Project list rejected: HTTP %lu / WinHTTP %lu.",(unsigned long)status,(unsigned long)error);
        MessageBoxA(main_window,message,APP_TITLE,MB_OK|MB_ICONWARNING);
    }
}
/* [AI:GPT-6 | 2026-10-09] 1.6.8: Operator explicitly specifies
 * org/project/security-channel-id/user. Authority stays on the server.
 * This operation never provisions an arbitrary non-Security channel. */
static void prompt_security_grant(void){
    char scope[260],body[260],response[512]={0},message[256];
    size_t i,part=0,out=0;
    DWORD error=0,status=0;
    if(!session_authenticated)return;
    if(GetWindowTextA(input_box,scope,sizeof(scope))<=0){
        MessageBoxA(main_window,"Enter org/project/channel-id/user in the input box.",APP_TITLE,MB_OK|MB_ICONINFORMATION);return;
    }
    for(i=0;scope[i];i++){
        char c=scope[i];
        if(c=='/'){
            if(i==0||scope[i-1]=='/'||part>=3)return;
            body[out++]=9;part++;
        }else{
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return;
            body[out++]=c;
        }
    }
    if(part!=3||i==0||scope[i-1]=='/') {
        MessageBoxA(main_window,"Required: org/project/channel-id/user.",APP_TITLE,MB_OK|MB_ICONWARNING);return;
    }
    body[out]=0;
    if(digit_request("POST","/admin/security-grants",body,response,sizeof(response),10000,&error,&status)){
        MessageBoxA(main_window,"Explicit project Security-channel access granted.",APP_TITLE,MB_OK|MB_ICONINFORMATION);
        load_channels();
    }else{
        snprintf(message,sizeof(message),"Security grant denied: HTTP %lu / WinHTTP %lu.\nServer: %.140s",(unsigned long)status,(unsigned long)error,response[0]?response:"(no response body)");
        MessageBoxA(main_window,message,APP_TITLE,MB_OK|MB_ICONWARNING);
    }
    SecureZeroMemory(body,sizeof(body));
}
static void acknowledge_alert(void){LRESULT sel=SendMessageA(alerts_list,LB_GETCURSEL,0,0);char path[256],response[4096];DWORD e,s;if(sel==LB_ERR||(size_t)sel>=alert_count)return;snprintf(path,sizeof(path),"/alerts/%s/acknowledge",alerts[sel].id);if(digit_request("POST",path,"",response,sizeof(response),10000,&e,&s))load_alerts();}
static LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam){switch(msg){case WM_CREATE:main_window=hwnd;
surface_brush=CreateSolidBrush(DIGIT_BG);input_brush=CreateSolidBrush(DIGIT_SURFACE);
ui_font=CreateFontA(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
username_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_TABSTOP,12,12,138,25,hwnd,(HMENU)ID_USERNAME,NULL,NULL);
password_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP,160,12,138,25,hwnd,(HMENU)ID_PASSWORD,NULL,NULL);
login_button=CreateWindowA("BUTTON","Log In",WS_CHILD|WS_VISIBLE|WS_TABSTOP,308,12,78,25,hwnd,(HMENU)ID_LOGIN,NULL,NULL);
logout_button=CreateWindowA("BUTTON","Log Out",WS_CHILD|WS_VISIBLE|WS_TABSTOP,396,12,78,25,hwnd,(HMENU)ID_LOGOUT,NULL,NULL);
sa_button=CreateWindowA("BUTTON","SA Access",WS_CHILD|WS_VISIBLE|WS_TABSTOP,484,12,90,25,hwnd,(HMENU)ID_SA_CHECK,NULL,NULL);
new_project_button=CreateWindowA("BUTTON","Setup Channels",WS_CHILD|WS_VISIBLE|WS_TABSTOP,584,12,105,25,hwnd,(HMENU)ID_NEW_PROJECT,NULL,NULL);
bind_security_button=CreateWindowA("BUTTON","Bind Security",WS_CHILD,699,12,112,25,hwnd,(HMENU)ID_BIND_SECURITY,NULL,NULL);
list_projects_button=CreateWindowA("BUTTON","List Projects",WS_CHILD|WS_VISIBLE|WS_TABSTOP,821,12,112,25,hwnd,(HMENU)ID_LIST_PROJECTS,NULL,NULL);
security_grant_button=CreateWindowA("BUTTON","Grant Security",WS_CHILD,943,12,112,25,hwnd,(HMENU)ID_SECURITY_GRANT,NULL,NULL);
{ HWND controls[]={username_box,password_box,login_button,logout_button,sa_button,new_project_button,bind_security_button,list_projects_button,security_grant_button};size_t ci;for(ci=0;ci<sizeof(controls)/sizeof(controls[0]);++ci)SendMessageA(controls[ci],WM_SETFONT,(WPARAM)ui_font,TRUE);}

channel_list=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY|WS_TABSTOP,12,12,170,310,hwnd,(HMENU)ID_CHANNELS,NULL,NULL);new_channel_button=CreateWindowA("BUTTON","New Channel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,12,328,170,28,hwnd,(HMENU)ID_NEW_CHANNEL,NULL,NULL);output_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_TABSTOP,194,12,560,390,hwnd,(HMENU)ID_OUTPUT,NULL,NULL);alerts_list=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",WS_CHILD|WS_VSCROLL|LBS_NOTIFY,766,12,250,310,hwnd,(HMENU)ID_ALERTS,NULL,NULL);ack_button=CreateWindowA("BUTTON","Acknowledge",WS_CHILD,766,328,120,28,hwnd,(HMENU)ID_ACK_ALERT,NULL,NULL);refresh_button=CreateWindowA("BUTTON","Refresh",WS_CHILD|WS_VISIBLE|WS_TABSTOP,896,328,120,28,hwnd,(HMENU)ID_REFRESH,NULL,NULL);input_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_TABSTOP,194,414,460,28,hwnd,(HMENU)ID_INPUT,NULL,NULL);send_button=CreateWindowA("BUTTON","Send",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP,664,414,90,28,hwnd,(HMENU)ID_SEND,NULL,NULL);status_text=CreateWindowA("STATIC","Checking...",WS_CHILD|WS_VISIBLE,12,452,1000,20,hwnd,(HMENU)ID_STATUS,NULL,NULL);
{ HWND controls[]={channel_list,new_channel_button,output_box,input_box,send_button,alerts_list,ack_button,refresh_button,status_text};size_t ci;for(ci=0;ci<sizeof(controls)/sizeof(controls[0]);++ci)if(controls[ci])SendMessageA(controls[ci],WM_SETFONT,(WPARAM)ui_font,TRUE);}
set_access_controls(FALSE);SetWindowTextA(status_text,"Sign in to Digit");SendMessageA(hwnd,WM_SIZE,0,MAKELPARAM(1180,560));return 0;case WM_ERASEBKGND:{RECT rc;GetClientRect(hwnd,&rc);FillRect((HDC)wparam,&rc,surface_brush);return 1;}
case WM_CTLCOLORSTATIC:
case WM_CTLCOLOREDIT:
case WM_CTLCOLORLISTBOX:{
 HDC dc=(HDC)wparam;HWND control=(HWND)lparam;
 SetTextColor(dc,control==status_text?DIGIT_MUTED:DIGIT_TEXT);
 SetBkColor(dc,msg==WM_CTLCOLORSTATIC?DIGIT_BG:DIGIT_SURFACE);
 return (LRESULT)(msg==WM_CTLCOLORSTATIC?surface_brush:input_brush);
}
case WM_COMMAND:switch(LOWORD(wparam)){case ID_LOGIN:do_login();return 0;case ID_LOGOUT:do_logout();return 0;case ID_SA_CHECK:check_sa();return 0;case ID_NEW_PROJECT:setup_project_security();return 0;case ID_BIND_SECURITY:prompt_bind_security();return 0;case ID_LIST_PROJECTS:prompt_list_projects();return 0;case ID_SECURITY_GRANT:prompt_security_grant();return 0;case ID_SEND:send_question();SetFocus(input_box);return 0;case ID_CHANNELS:if(HIWORD(wparam)==LBN_SELCHANGE){LRESULT sel=SendMessageA(channel_list,LB_GETCURSEL,0,0);if(sel!=LB_ERR){LRESULT index=SendMessageA(channel_list,LB_GETITEMDATA,(WPARAM)sel,0);if(index!=LB_ERR&&index>=0&&(size_t)index<channel_count){strcpy_s(active_channel,sizeof(active_channel),channels[index].id);load_history();}}}return 0;case ID_NEW_CHANNEL:prompt_new_channel();return 0;case ID_ACK_ALERT:acknowledge_alert();return 0;case ID_REFRESH:refresh_all();return 0;}break;case WM_TIMER:if(wparam==ID_SYNC_TIMER){start_sync();return 0;}break;
case WM_DIGIT_REFRESH:if(lparam){apply_sync((digit_sync_result_t *)lparam);}return 0;
case WM_DIGIT_RESULT:{digit_result_t *result=(digit_result_t *)lparam;char message[512];EnableWindow(send_button,TRUE);if(result){if(result->ok){if(strcmp(result->channel_id,active_channel)==0)append_output("Digit",result->answer);}else if(result->http_status){snprintf(message,sizeof(message),"Digit returned HTTP status %lu.",(unsigned long)result->http_status);append_output("Digit GUI",message);}else{snprintf(message,sizeof(message),"Windows network error %lu while waiting for Digit.",(unsigned long)result->error);append_output("Digit GUI",message);}free(result);}SetFocus(input_box);return 0;}case WM_SIZE:{int w=LOWORD(lparam),h=HIWORD(lparam),left=170,right=0,center=w-left-48;
MoveWindow(username_box,12,12,138,25,TRUE);MoveWindow(password_box,160,12,138,25,TRUE);
MoveWindow(login_button,308,12,78,25,TRUE);MoveWindow(logout_button,396,12,78,25,TRUE);
MoveWindow(sa_button,484,12,90,25,TRUE);MoveWindow(new_project_button,584,12,105,25,TRUE);MoveWindow(bind_security_button,699,12,112,25,TRUE);MoveWindow(list_projects_button,821,12,112,25,TRUE);MoveWindow(security_grant_button,943,12,112,25,TRUE);
MoveWindow(channel_list,12,48,left,h-166,TRUE);MoveWindow(new_channel_button,12,h-112,left,28,TRUE);
MoveWindow(output_box,194,48,center,h-128,TRUE);MoveWindow(input_box,194,h-68,center-102,28,TRUE);
MoveWindow(send_button,194+center-90,h-68,90,28,TRUE);
MoveWindow(alerts_list,w-right-12,48,right,h-166,TRUE);
MoveWindow(ack_button,w-right-12,h-112,120,28,TRUE);
MoveWindow(refresh_button,w-132,h-112,120,28,TRUE);
MoveWindow(status_text,12,h-32,w-24,20,TRUE);return 0;}case WM_DESTROY:KillTimer(hwnd,ID_SYNC_TIMER);if(ui_font)DeleteObject(ui_font);if(surface_brush)DeleteObject(surface_brush);if(input_brush)DeleteObject(input_brush);++sync_generation;clear_session();main_window=NULL;PostQuitMessage(0);return 0;}return DefWindowProcA(hwnd,msg,wparam,lparam);}
static int self_test(void){char answer[256],value[256];int failures=0;if(!extract_answer("{\"answered\":true,\"answer\":\"Ready.\"}",answer,sizeof(answer))||strcmp(answer,"Ready.")!=0)++failures;if(!json_string_after("{\"id\":\"general\",\"name\":\"General\"}","name",value,sizeof(value))||strcmp(value,"General")!=0)++failures;if(!json_string_after("{\"severity\":\"ERROR\",\"summary\":\"Module rejected\"}","summary",value,sizeof(value))||strcmp(value,"Module rejected")!=0)++failures;return failures?1:0;}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command_line,int show){WNDCLASSA wc={0};HWND hwnd;MSG msg;(void)previous;(void)command_line;if(!load_config()){MessageBoxA(NULL,"Unable to load digit.conf beside digit-gui.exe. Expected host=<server> and port=<port>.",APP_TITLE,MB_OK|MB_ICONERROR);return 1;}wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.lpszClassName="DigitGuiWindow";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassA(&wc))return 1;hwnd=CreateWindowExA(0,wc.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1180,560,NULL,NULL,instance,NULL);if(!hwnd)return 1;ShowWindow(hwnd,show);UpdateWindow(hwnd);/* [AI:GPT-6 | 2026-10-09] Native keyboard workflow: Tab navigates enabled visible controls; Enter submits the focused login or message field. */
while(GetMessageA(&msg,NULL,0,0)>0){
    HWND focus=GetFocus();
    if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN &&
       (focus==username_box||focus==password_box||focus==input_box)){
        SendMessageA(hwnd,WM_COMMAND,MAKEWPARAM(focus==input_box?ID_SEND:ID_LOGIN,BN_CLICKED),0);
        continue;
    }
    if(!IsDialogMessageA(hwnd,&msg)){
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}return(int)msg.wParam;}
int main(int argc,char **argv){if(argc==2&&strcmp(argv[1],"--self-test")==0)return self_test();return WinMain(GetModuleHandle(NULL),NULL,GetCommandLineA(),SW_SHOWDEFAULT);}
