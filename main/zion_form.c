#include "zion_form.h"
#include <string.h>
static int hex(char c) {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static bool decode(const char *src,size_t n,char *out,size_t cap) {
    size_t used=0;
    for(size_t i=0;i<n;i++) {
        unsigned char ch=(unsigned char)src[i];
        if(ch=='+') ch=' ';
        else if(ch=='%') {
            if(i+2>=n || hex(src[i+1])<0 || hex(src[i+2])<0) return false;
            ch=(unsigned char)((hex(src[i+1])<<4)|hex(src[i+2]));i+=2;
        }
        if(!ch || ch<0x20 || ch==0x7f || used+1>=cap) return false;
        out[used++]=(char)ch;
    }
    out[used]=0;return true;
}
bool zion_wifi_form(const char *body,size_t size,char ssid[33],char password[65],char token[17]) {
    ssid[0]=password[0]=token[0]=0;unsigned seen=0;
    for(size_t at=0;at<size;) {
        size_t end=at;while(end<size && body[end]!='&') end++;
        size_t eq=at;while(eq<end && body[eq]!='=') eq++;
        if(eq==end) return false;
        unsigned bit;char *dst;size_t cap;
        if(eq-at==4 && !memcmp(body+at,"ssid",4)) {bit=1;dst=ssid;cap=33;}
        else if(eq-at==8 && !memcmp(body+at,"password",8)) {bit=2;dst=password;cap=65;}
        else if(eq-at==5 && !memcmp(body+at,"token",5)) {bit=4;dst=token;cap=17;}
        else return false;
        if((seen&bit) || !decode(body+eq+1,end-eq-1,dst,cap)) return false;
        seen|=bit;at=end+1;
    }
    size_t pass=strlen(password);
    if(seen!=7 || !ssid[0] || strlen(token)!=16 || (pass && pass<8)) return false;
    if(pass==64) for(unsigned i=0;i<64;i++) if(hex(password[i])<0) return false;
    return true;
}
