#include <bits/stdc++.h>
#include <thread>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

using namespace std;

int ctrl_fd=-1;
int data_fd=-1;

void sendCommand(int fd,const string& s){
    string s1=s+"\r\n";
    const char* buf=s1.c_str();
    int len=s1.size();
    int sent=0;
    while(sent<len){
        int n=send(fd,buf+sent,len-sent,MSG_NOSIGNAL);
        if(n<=0) return;
        sent+=n;
    }
}
string recvResponse(int fd){
    string s="";
    char c;
    while(1){
        int n=recv(fd,&c,1,0);
        if(n<=0) return "";
        s+=c;
        if(s.size()>=2&&s.substr(s.size()-2)=="\r\n") break; 
    }
    return s;  
}
bool login(){
    sendCommand(ctrl_fd,"USER A");
    string s=recvResponse(ctrl_fd);
    cout<<s;
    if(s.substr(0,3)!="331") return 0;

    sendCommand(ctrl_fd,"PASS 123");
    s=recvResponse(ctrl_fd);
    cout<<s;
    if(s.substr(0,3)!="230") return 0;
    return 1;
}
bool connectServer(){
    //1socket
    ctrl_fd=socket(AF_INET,SOCK_STREAM,0);
    //2connect
    sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(2100);
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    if (connect(ctrl_fd,(sockaddr*)&addr,sizeof(addr))<0){
        perror("connect");
        return 0;
    }
    string res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="220"){
        close(ctrl_fd);
        return 0;
    }
    if(login()) return 1;
    close(ctrl_fd);
    return 0;
}
bool enterPasv(){
    sendCommand(ctrl_fd,"PASV");
    string s=recvResponse(ctrl_fd);
    cout<<s;
    if(s.substr(0,3)!="227"){
        cout<<"PASV command failed.\n";
        return 0;
    }
    int h1,h2,h3,h4,p1,p2;
    if(sscanf(s.c_str(),"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",&h1,&h2,&h3,&h4,&p1,&p2)!=6){
        cout<<"PASV parse error\n";
        return 0;
    } int port=p1*256+p2;
    //1socket
    data_fd=socket(AF_INET,SOCK_STREAM,0);
    //2connect
    sockaddr_in addr;
    addr.sin_port=htons(port);
    addr.sin_family=AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    if(connect(data_fd,(sockaddr*)&addr,sizeof(addr))<0){
        perror("connect (data)");
        return 0;
    }
    return 1;
}
void list_(){
    if(!enterPasv()) return;
    sendCommand(ctrl_fd,"LIST");
    string res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="150"){
        cout << "LIST command failed.\n";
        close(data_fd);
        data_fd=-1;
        return;
    }
    char buf[1024*4];
    int n;
    while((n=recv(data_fd,buf,1024*4,0))>0){
        cout.write(buf,n);
    }close(data_fd);
    data_fd=-1;
    cout<<recvResponse(ctrl_fd);
}
void get_(){
    string filename;
    cout<<"filename:\n";
    cin>>filename;
    if(!enterPasv()) return;
    sendCommand(ctrl_fd,"TYPE I");
    string res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="200"){
        cout<<"Failed to set binary mode.\n";
        close(data_fd);
        data_fd=-1;
        return;
    }
    sendCommand(ctrl_fd,"RETR "+filename);
    res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="150"){
        cout<<"Failed to set binary mode.\n";
        close(data_fd);
        data_fd=-1;
        return;
    }
    int fd=open(filename.c_str(),O_CREAT|O_WRONLY|O_TRUNC,0666);
    if(fd<0){
        perror("open (GET)");
        close(data_fd);
        data_fd=-1;
        recvResponse(ctrl_fd);   
    }
    char buf[1024*4];
    int n;
    while((n=recv(data_fd,buf,1024*4,0))>0)
        write(fd,buf,n);
    close(fd);
    close(data_fd);
    data_fd=-1;
    cout<<recvResponse(ctrl_fd);
}
void put_(){
    string filename;
    cout<<"filename:\n";
    cin>>filename;
    int fd=open(filename.c_str(),O_RDONLY);
    if(fd<0){
        cout<<"file not found\n";
        return;
    }  
    if(!enterPasv()){ close(fd); return;}
    sendCommand(ctrl_fd,"TYPE I");
    string res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="200"){
        cout<<"Failed to set binary mode.\n";
        close(data_fd);
        data_fd=-1;
        return;
    }
    sendCommand(ctrl_fd,"STOR "+filename);
    res=recvResponse(ctrl_fd);
    cout<<res;
    if(res.substr(0,3)!="150"){
        cout<<"Failed to set binary mode.\n";
        close(data_fd);
        data_fd=-1;
        return;
    }
    char buf[1024*4];
    int n;
    while((n=read(fd,buf,sizeof(buf)))>0){
        int sent=0;
        while(sent<n){
            int q=send(data_fd, buf + sent, n - sent, 0);
            if(q<0){
                perror("send (PUT)");
                close(fd);
                close(data_fd);
                data_fd=-1;
                recvResponse(ctrl_fd); // Try to consume final response if any
                return;
            }
            sent+=q;
        }
    }  
    close(fd);
    shutdown(data_fd, SHUT_WR); // Gracefully close write side
    cout<<recvResponse(ctrl_fd); // Get final response from server
    close(data_fd);
    data_fd=-1;

}
void quit_(){
    sendCommand(ctrl_fd,"QUIT");
    cout<<recvResponse(ctrl_fd);
    close(ctrl_fd);
    ctrl_fd=-1;   
}
int main(){
    if(!connectServer()) return 0;
    while(1){
        cout << "===== FTP CLIENT =====\n1. LIST\n2. GET\n3. PUT\n4. QUIT\n\nchoice:";
        int N; cin>>N;
        if(N==1) list_();
        else if(N==2) get_();
        else if(N==3) put_();
        else if(N==4) {quit_();break; }
        else continue;
    }
    return 0;
}