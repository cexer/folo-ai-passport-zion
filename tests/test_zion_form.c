#include "zion_form.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool parse(const char *form,char *s,char *p,char *t) {return zion_wifi_form(form,strlen(form),s,p,t);}
int main(void) {
    char s[33],p[65],t[17];
    assert(parse("ssid=test+network&password=test%2Bpass&token=0123456789abcdef",s,p,t));
    assert(!strcmp(s,"test network") && !strcmp(p,"test+pass"));
    assert(parse("token=0123456789abcdef&ssid=%E6%B5%8B%E8%AF%95&password=",s,p,t));
    assert(!strcmp(s,"\xe6\xb5\x8b\xe8\xaf\x95"));
    const char *bad[]={"ssid=x&ssid=y&password=12345678&token=0123456789abcdef",
        "ssid=%00&password=12345678&token=0123456789abcdef","ssid=%2&password=12345678&token=0123456789abcdef",
        "ssid=x&password=short&token=0123456789abcdef","ssid=x&password=12345678&token=short",
        "ssid=&password=12345678&token=0123456789abcdef","ssid=x&password=12345678&other=0123456789abcdef"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) assert(!parse(bad[i],s,p,t));
    puts("Zion provisioning form: PASS (UTF-8, spaces, escaped plus, open network, duplicate/NUL/truncated escape/short password/CSRF fields rejected)");
}
