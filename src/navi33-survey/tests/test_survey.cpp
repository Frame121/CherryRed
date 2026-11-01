#include "Navi33Survey.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static bool enabled=true;
static std::string logs;
static void check(bool ok) { if(!ok) std::abort(); }
int PE_parse_boot_argn(const char* name, void* out, int size) {
 check(std::strcmp(name,"navi33survey")==0 && size==4);
 uint32_t value=enabled?1:0; std::memcpy(out,&value,4); return 1;
}
void IOLog(const char* fmt, ...) {
 char buffer[512]; va_list args; va_start(args,fmt);
 std::vsnprintf(buffer,sizeof(buffer),fmt,args); va_end(args); logs+=buffer;
}
int main() {
 Navi33Survey survey; IOPCIDevice pci;
 enabled=false;
 check(!survey.start(&pci) && pci.reads.empty() && survey.starts==0);
 enabled=true;
 pci.values[0xb]=4;
 check(!survey.start(&pci) && survey.starts==0);
 pci.values[0xb]=3; pci.values[2]=0x7550;
 check(!survey.start(&pci) && survey.starts==0);
 pci.values[2]=0x7480;
 check(!survey.start(nullptr));
 // Two distinct BAR entries at extended offsets; second entry must be read.
 pci.values[0x204]=1u<<12; pci.values[0x208]=(2u<<5)|(8u<<8);
 pci.values[0x20c]=1u<<13; pci.values[0x210]=(9u<<8)|2;
 check(survey.start(&pci) && survey.registrations==1);
 check(logs.find("BAR0 supported")!=std::string::npos);
 check(logs.find("BAR2 supported")!=std::string::npos);
 for(unsigned count: {0u,7u}) {
   logs.clear(); pci.values[0x208]=count<<5;
   check(survey.start(&pci));
   check(logs.find("Invalid ReBAR entry count")!=std::string::npos);
 }
 logs.clear(); pci.values[0x208]=(2u<<5)|(28u<<8);
 check(survey.start(&pci));
 check(logs.find("Invalid or unsupported ReBAR entry")!=std::string::npos);
 logs.clear(); pci.rebar=0xff8;
 check(survey.start(&pci));
 check(logs.find("not found or invalid")!=std::string::npos);
 logs.clear(); pci.rebar=0x200; pci.header=(0x208u<<20)|0x10015;
 check(survey.start(&pci));
 check(logs.find("Invalid ReBAR capability boundary")!=std::string::npos);
 Navi33Survey failed; failed.startResult=false;
 check(!failed.start(&pci) && failed.registrations==0);
}
