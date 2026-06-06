#include <bits/stdc++.h>
#include <thread>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <ctime> 

using namespace std;
bool server_running=1;
int pasv_fd=-1;
int data_fd=-1;
bool logged_in=0;
//const string FTP_ROOT = "/home/guomengdi/NetWork/ftp_dir"; 

void signal_handler(int sig){
    cout<<"收到退出信号，正在关闭服务器\n";
    server_running=0;
    return ;
}

void send_response(int fd,const string& s){
    const char* buf=s.c_str();
    int len=s.size();
    int sent=0;
    while(sent<len){
        int n=send(fd,buf+sent,len-sent,MSG_NOSIGNAL);
        if(n<=0) return ;
        sent+=n;
    }
    return ;
}

string recv_command(int fd){
    string s="";
    char c;
    while(1){
        int n=recv(fd,&c,1,0);
        if(n<=0){
            return "";
        }s+=c;
        if(s.size()>=2&&s.substr(s.size()-2)=="\r\n"){
            s.resize(s.size()-2);
            break;
        }
    }
    while(!s.empty()&&s.back()==' '||s.back()=='\t') s.pop_back();

    return s;
}

string format_time(time_t t){
    struct tm *tm_info=localtime(&t);
    char buf[100];
    strftime(buf,100,"%b %d %H:%M",tm_info);
    return string(buf);
}

void handle_client(int ctrl_fd){
    send_response(ctrl_fd,"220 Simple FTP Server Ready\r\n");
    
    while(server_running){
        string s=recv_command(ctrl_fd);
        if(s.empty()){
            cout<<"Client disconnected (command empty).\n";
            break;
        }
    string st,command;
    auto pos=s.find(' ');
    if(pos==string::npos){
        st=s;
        command="";
    }else{
        st=s.substr(0,pos);
        command=s.substr(pos+1);
    }
    transform(st.begin(),st.end(),st.begin(),::toupper);
    if(st=="USER"){
        send_response(ctrl_fd,"331 Please specify password.\r\n");
    }else if(st=="PASS"){
        logged_in=1;
        send_response(ctrl_fd,"230 Login successful.\r\n");
    }else if(st=="QUIT"){
        send_response(ctrl_fd,"221 Goodbye.\r\n");
        break;
    }else if(!logged_in){
        send_response(ctrl_fd,"530 Not logged in.\r\n");
        continue;
    }else if(st=="PASV"){
        if(pasv_fd!=-1){
            close(pasv_fd);
            pasv_fd=-1;
        }
        //1socket
        pasv_fd=socket(AF_INET,SOCK_STREAM,0);
        if(pasv_fd<=0){
            send_response(ctrl_fd,"425 Can't open data connection.\r\n");
            continue;
        }
        //2bind
        sockaddr_in addr;
        addr.sin_family=AF_INET;
        addr.sin_port=htons(0);
        addr.sin_addr.s_addr=INADDR_ANY;
        if(bind(pasv_fd,(sockaddr*)&addr,sizeof(addr))<0){
            close(pasv_fd);
            pasv_fd=-1;
            send_response(ctrl_fd,"425 Can't bind data socket.\r\n");
            continue;
        }
        //3listen
        if(listen(pasv_fd,1)<0){
            close(pasv_fd);
            pasv_fd = -1;
            send_response(ctrl_fd,"425 Can't open data connection.\r\n");
            continue;
        }
        socklen_t len=sizeof(addr);
        getsockname(pasv_fd,(sockaddr*)&addr,&len);
        int port=ntohs(addr.sin_port);
        int p1=port/256;
        int p2=port%256;

        string res="227 Entering Passive Mode (127,0,0,1,"+to_string(p1)+","+to_string(p2)+").\r\n";
        send_response(ctrl_fd,res);
    }else if(st=="LIST"){
        if(pasv_fd==-1){
            send_response(ctrl_fd,"425 Use PASV first.\r\n");
            continue;
        }send_response(ctrl_fd,"150 Here comes the directory listing.\r\n");
        
        string res="";
        if((data_fd=accept(pasv_fd,NULL,NULL))>=0){
            DIR* dir=opendir(".");
            if(dir){
                dirent* entry;
                struct stat statbuf;
                while((entry=readdir(dir))!=NULL){
                    string path=entry->d_name;
                    if(stat(path.c_str(),&statbuf)==0){
                        string perms =(S_ISDIR(statbuf.st_mode)?"d":"-");
                            perms += (statbuf.st_mode & S_IRUSR ? "r" : "-");
                            perms += (statbuf.st_mode & S_IWUSR ? "w" : "-");
                            perms += (statbuf.st_mode & S_IXUSR ? "x" : "-");
                            perms += (statbuf.st_mode & S_IRGRP ? "r" : "-");
                            perms += (statbuf.st_mode & S_IWGRP ? "w" : "-");
                            perms += (statbuf.st_mode & S_IXGRP ? "x" : "-");
                            perms += (statbuf.st_mode & S_IROTH ? "r" : "-");
                            perms += (statbuf.st_mode & S_IWOTH ? "w" : "-");
                            perms += (statbuf.st_mode & S_IXOTH ? "x" : "-");    

                            string size=to_string(statbuf.st_size);
                            string date=format_time(statbuf.st_mtime);
                            string name=entry->d_name;

                            res+=perms+" 1 user group "+size+" "+date+" "+name +"\r\n";
                    }
                }
                closedir(dir);
            }else{
               send_response(ctrl_fd,"550 Failed to open directory.\r\n");
               close(data_fd);
               close(pasv_fd);
               data_fd=-1,pasv_fd=-1;
               continue; 
            }
            send(data_fd,res.c_str(),res.size(),0);
            close(data_fd);
            data_fd=-1;
        }else{
            perror("accept (LIST)");
            send_response(ctrl_fd,"425 Data connection error.\r\n");
            close(pasv_fd);
            pasv_fd=-1;
            continue;
        }
        close(pasv_fd);
        pasv_fd=-1;
        send_response(ctrl_fd,"226 Directory send OK.\r\n");
    }else if(st=="RETR"){
        if (pasv_fd==-1){
        send_response(ctrl_fd,"425 Use PASV first.\r\n");
        continue;
    }
    string filename=command;
    while(!filename.empty()&&(filename.back()=='\r'||filename.back()=='\n'||filename.back()==' '))
        filename.pop_back();
    cout<<"RETR file: "<< filename<<endl;
    int fd=open(filename.c_str(),O_RDONLY);
    if(fd<0){
        perror("open");
        send_response(ctrl_fd,"550 File not found.\r\n");
        close(pasv_fd);
        pasv_fd=-1;
        continue;
    }
    send_response(ctrl_fd,"150 Opening BINARY mode data connection.\r\n");
    if((data_fd=accept(pasv_fd,NULL,NULL))<0){
        perror("accept");
        close(fd);
        close(pasv_fd);
        pasv_fd=-1;
        send_response(ctrl_fd,"425 Can't open data connection.\r\n");
        continue;    
    }
    char buf[1024*4];
    int n;
    while((n=read(fd,buf,1024*4))>0){
        int sent=0;
        while(sent<n){
            int q=send(data_fd,buf+sent,n-sent,MSG_NOSIGNAL);
            if(q<=0){
                perror("send");
                close(fd);
                close(data_fd);
                data_fd=-1;
                close(pasv_fd);
                pasv_fd=-1;
                send_response(ctrl_fd,"426 Connection closed; transfer aborted.\r\n");
                goto a;
            }
            sent+=q;
       }
    }
    close(fd);
    close(data_fd);
    close(pasv_fd);
    data_fd=-1;
    pasv_fd=-1;

    send_response(ctrl_fd,"226 Transfer complete.\r\n");
a:
    ;
    }else if(st=="STOR"){
        if (pasv_fd==-1){
        send_response(ctrl_fd,"425 Use PASV first.\r\n");
        continue;
    }
    string filename=command;
    while(!filename.empty()&&(isspace(filename.back())))
        filename.pop_back();

    int fd=open(filename.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0666);
            if(fd<0){
                send_response(ctrl_fd,"550 Cannot create file.\r\n");
                close(pasv_fd); 
                pasv_fd=-1;
                continue;
            }
            send_response(ctrl_fd,"150 Opening BINARY mode data connection.\r\n");
            data_fd=accept(pasv_fd, NULL, NULL);
            if(data_fd<0){
                close(fd);
                send_response(ctrl_fd,"425 Data connection failed.\r\n");
                close(pasv_fd); 
                pasv_fd=-1;
                continue;
            }
            char buf[1024*4];
            int n;
            while((n=recv(data_fd,buf,1024*4,0))>0)
                write(fd, buf, n);

            close(fd);
            close(data_fd);
            close(pasv_fd);
            data_fd=-1;
            pasv_fd=-1;

            send_response(ctrl_fd, "226 Transfer complete.\r\n");

        }else if(st=="TYPE"){
                while(!command.empty()&&isspace(command[0])) command.erase(0,1);
                while(!command.empty()&&(command.back()=='\r'||command.back()=='\n'||isspace(command.back())))
                   command.pop_back();
        
                transform(command.begin(),command.end(),command.begin(),::toupper);
                if(command=="I"||command=="A"){
                    send_response(ctrl_fd,"200 Type set to "+command+".\r\n");
                }else{
                    send_response(ctrl_fd,"504 Command not implemented for that parameter.\r\n");
                }
            }else{
                send_response(ctrl_fd,"500 Unknown command.\r\n");
            }
        }
        if(pasv_fd!=-1) close(pasv_fd);
        if(data_fd!=-1) close(data_fd);
        data_fd-1,pasv_fd=-1;
        close(ctrl_fd);
        cout << "Client disconnected.\n";
}
int main(){
    //chdir(FTP_ROOT.c_str());
    signal(SIGINT,signal_handler);

    //1socket
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd<0){
        cout<<"Failed to create socket\n";
        return 1;
    }

    int opt=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //2bind
    sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(2100);
    addr.sin_addr.s_addr=INADDR_ANY; 

    if((bind(listen_fd,(sockaddr*)&addr,sizeof(addr)))<0){
        cout<<"Failed to bind port 2100\n";
        close(listen_fd);
        return 1;
    }
    //3listen
    if(listen(listen_fd,10)<0){
        cout<<"Failed to listen on port 2100\n";
        return 1;
    }
    cout<<"FTP Server Started On Port 2100\n";

    while(server_running){
        //1accept
        int client_fd=accept(listen_fd,NULL,NULL);
        if(client_fd<0){
            if(server_running) perror("accept");
            continue;
        }
        if(!server_running){
            close(client_fd);
            break;
        }
        cout<<"New client connected\n";

        //thread
        thread t(handle_client,client_fd);
        t.detach();
    }
    close(listen_fd);
    cout<<"服务器已安全关闭\n";
    return 0;
}
