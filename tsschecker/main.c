//
//  main.c
//  tsschecker
//
//  Created by tihmstar on 22.12.15.
//  Copyright © 2015 tihmstar. All rights reserved.
//

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include "download.h"
#include "tsschecker.h"
#include "all.h"


#define FLAG_LIST_IOS       1 << 0
#define FLAG_LIST_DEVICES   1 << 1
#define FLAG_OTA            1 << 2
#define FLAG_NO_BASEBAND    1 << 3
#define FLAG_BUILDMANIFEST  1 << 4
#define FLAG_BETA           1 << 5
#define FLAG_LATEST_IOS     1 << 6

int dbglog;
int idevicerestore_debug;


static struct option longopts[] = {
    { "list-devices",       no_argument,       NULL, '1' },
    { "list-ios",           no_argument,       NULL, '2' },
    { "build-manifest",     required_argument, NULL, 'm' },
    { "save-path",          required_argument, NULL, '3' },
    { "print-tss-request",  no_argument,       NULL, '4' },
    { "print-tss-response", no_argument,       NULL, '5' },
    { "beta",               no_argument,       NULL, '6' },
    { "nocache",            no_argument,       NULL, '7' },
    { "apnonce",            required_argument, NULL, '8' },
    { "sepnonce",           required_argument, NULL, '9' },
    { "device",             required_argument, NULL, 'd' },
    { "ios",                required_argument, NULL, 'i' },
    { "ecid",               required_argument, NULL, 'e' },
    { "help",               no_argument,       NULL, 'h' },
    { "no-baseband",        no_argument,       NULL, 'b' },
    { "ota",                no_argument,       NULL, 'o' },
    { "save",               no_argument,       NULL, 's' },
    { "latest",             no_argument,       NULL, 'l' },
    { "debug",              no_argument,       NULL, '0' },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    
    printf("  -d, --device MODEL\t\tspecific device by its MODEL (eg. iPhone4,1)\n");
    printf("  -i, --ios VERSION\t\tspecific iOS version (eg. 6.1.3)\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("  -o, --ota\t\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -b, --no-baseband\t\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("  -m, --build-manifest\t\tmanually specify buildmanifest. (can be used with -d)\n");
    printf("  -s, --save\t\t\tsave fetched shsh blobs (mostly makes sense with -e)\n");
    printf("  -l, --latest\t\t\tuse latest public iOS version instead of manually specifying one\n");
    printf("                 \t\tespecially useful with -s and -e for saving blobs\n");
    printf("  -e, --ecid ECID\t\tmanually specify ECID to be used for fetching blobs, instead of using random ones\n");
    printf("                 \t\tECID must be either dec or hex eg. 5482657301265 or ab46efcbf71\n");
    printf("      --apnonce NONCE\t\tmanually specify APNONCE instead of using random one (not required for saving blobs)\n");
    printf("      --sepnonce NONCE\t\tmanually specify SEPNONCE instead of using random one (not required for saving blobs)\n");
    printf("      --save-path PATH\t\tspecify path for saving blobs\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("      --beta\t\t\trequest ticket for beta instead of normal relase (use with -o)\n");
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-ios\t\tlist all known ios versions\n");
    printf("      --nocache \t\tignore caches and redownload required files\n");
    printf("      --print-tss-request\n");
    printf("      --print-tss-response\n");
    printf("\n");
}

int64_t parseECID(const char *ecid){
    const char *ecidBK = ecid;
    int isHex = 0;
    int64_t ret = 0;
    
    while (*ecid && !isHex) {
        char c = *(ecid++);
        if (c >= '0' && c<='9') {
            ret *=10;
            ret += c - '0';
        }else{
            isHex = 1;
            ret = 0;
        }
    }
    
    if (isHex) {
        while (*ecidBK) {
            char c = *(ecidBK++);
            ret *=16;
            if (c >= '0' && c<='9') {
                ret += c - '0';
            }else if (c >= 'a' && c <= 'f'){
                ret += 10 + c - 'a';
            }else if (c >= 'A' && c <= 'F'){
                ret += 10 + c - 'A';
            }else{
                return 0; //ERROR parsing failed
            }
        }
    }
    
    return ret;
}

char *parseNonce(const char *nonce){
    char *ret = malloc((noncelen+1)*sizeof(char));
    memset(ret, 0, noncelen+1);
    int nlen = 0;
    
    int next = strlen(nonce)%2 == 0;
    char tmp = 0;
    while (*nonce) {
        char c = *(nonce++);
        
        tmp *=16;
        if (c >= '0' && c<='9') {
            tmp += c - '0';
        }else if (c >= 'a' && c <= 'f'){
            tmp += 10 + c - 'a';
        }else if (c >= 'A' && c <= 'F'){
            tmp += 10 + c - 'A';
        }else{
            return 0; //ERROR parsing failed
        }
        if ((next =! next) && nlen < noncelen) ret[nlen++] = tmp,tmp=0;
    }
    
    return ret;
}

int main(int argc, const char * argv[]) {
    
    dbglog = 1;
    idevicerestore_debug = 0;
    save_shshblobs = 0;
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    char *device = 0;
    char *ios = 0;
    char *buildmanifest = 0;
    char *ecid = 0;
    
    char *apnonce = 0;
    char *sepnonce = 0;
    t_devicevals devVals;
    devVals.ecid = 0;
    devVals.apnonce = 0;
    devVals.sepnonce = 0;
    
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    while ((opt = getopt_long(argc, (char* const *)argv, "d:i:e:m:hslbo", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
            case 'd': // long option: "device"; can be called as short option
                device = optarg;
                break;
            case 'i': // long option: "ios"; can be called as short option
                ios = optarg;
                break;
            case 'e': // long option: "ecid"; can be called as short option
                ecid = optarg;
                break;
            case 'b': // long option: "no-baseband"; can be called as short option
                flags |= FLAG_NO_BASEBAND;
                break;
            case 'l': // long option: "latest"; can be called as short option
                flags |= FLAG_LATEST_IOS;
                break;
            case 's': // long option: "save"; can be called as short option
                save_shshblobs = 1;
                break;
            case 'o': // long option: "ota"; can be called as short option
                flags |= FLAG_OTA;
                break;
            case '0': // only long option: "debug"
                idevicerestore_debug = 1;
                break;
            case '1': // only long option: "list-devices"
                flags |= FLAG_LIST_DEVICES;
                break;
            case '2': // only long option: "list-ios"
                flags |= FLAG_LIST_IOS;
                break;
            case 'm': // long option: "build-manifest"; can be called as short option
                flags |= FLAG_BUILDMANIFEST;
                buildmanifest = optarg;
                break;
            case '3': // only long option: "save-path"
                shshSavePath = optarg;
                break;
            case '4': // only long option: "print-tss-request"
                print_tss_request = 1;
                break;
            case '5': // only long option: "print-tss-response"
                print_tss_response = 1;
                break;
            case '6': // only long option: "beta"
                flags |= FLAG_BETA;
                break;
            case '7': // only long option: "nocache"
                nocache = 1;
                break;
            case '8': // only long option: "apnonce"
                apnonce = optarg;
                break;
            case '9': // only long option: "sepnonce"
                sepnonce = optarg;
                break;
                
            default:
                cmd_help();
                return -1;
        }
    }
    int err = 0;
    int isSigned = 0;
    char *firmwareJson = NULL;
    jsmntok_t *firmwareTokens = NULL;
    
#define reterror(code,a ...) {error(a); err = code; goto error;}
    
    if (ecid) {
        if ((devVals.ecid = parseECID(ecid)) == 0){
            reterror(-7, "[TSSC] ERROR: manually specified ecid=%s, but parsing failed\n",ecid);
        }else{
            info("[TSSC] manually specified ecid to use, parsed \"%s\" to dec:%lld hex:%llx\n",ecid,devVals.ecid,devVals.ecid);
        }
    }
    if (apnonce) {
        if ((devVals.apnonce = parseNonce(apnonce)) ){
            info("[TSSC] manually specified apnonce to use, parsed \"%s\" to hex:",apnonce);
            unsigned char *tmp = (unsigned char*)devVals.apnonce;
            while (*tmp) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] ERROR: manually specified apnonce=%s, but parsing failed\n",apnonce);
        }
    }
    if (sepnonce) {
        if ((devVals.sepnonce = parseNonce(sepnonce)) ){
            info("[TSSC] manually specified sepnonce to use, parsed \"%s\" to hex:",sepnonce);
            unsigned char *tmp = (unsigned char*)devVals.sepnonce;
            while (*tmp) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] ERROR: manually specified sepnonce=%s, but parsing failed\n",sepnonce);
        }
    }
    
    firmwareJson = (flags & FLAG_OTA) ? getOtaJson() : getFirmwareJson();
    if (!firmwareJson) reterror(-6,"[TSSC] ERROR: could not get firmware.json\n");
    
    int cnt = parseTokens(firmwareJson, &firmwareTokens);
    if (cnt < 1) reterror(-2,"[TSSC] ERROR: parsing %s.json failed\n",(flags & FLAG_OTA) ? "ota" : "firmware");
    
    if (flags & FLAG_LATEST_IOS && !ios){
        int versionCnt = 0;
        int i = 0;
        char **versions = getListOfiOSForDevice(firmwareJson, firmwareTokens, device, (flags & FLAG_OTA), &versionCnt);
        if (!versionCnt) reterror(-8, "[TSSC] ERROR: failed finding latest iOS. ota=%d\n",((flags & FLAG_OTA) != 0));
        char *bpos = NULL;
        while((bpos = strstr(ios = strdup(versions[i++]),"[B]")) != 0){
            if (flags & FLAG_BETA) break;
            if (--versionCnt == 0) reterror(-9, "[TSSC] ERROR: automatic iOS selection couldn't find non-beta iOS\n");
        }
        info("[TSSC] selecting latest iOS: %s\n",ios);
        if (bpos) *bpos= '\0';
        if (versions) free(versions[versionCnt-1]),free(versions);
    }
    
    if (flags & FLAG_LIST_DEVICES) {
        printListOfDevices(firmwareJson, firmwareTokens);
    }else if (flags & FLAG_LIST_IOS){
        if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
        if (!checkDeviceExists(device, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-4,"[TSSC] ERROR: device %s could not be found in devicelist\n",device);
        
        printListOfiOSForDevice(firmwareJson, firmwareTokens, device, (flags & FLAG_OTA));
    }else{
        //request ticket
        if (buildmanifest) {
            if (device && !checkDeviceExists(device, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-4,"[TSSC] ERROR: device %s could not be found in devicelist\n",device);
            
            isSigned = isManifestSignedForDevice(buildmanifest, &device, devVals, !(flags & FLAG_NO_BASEBAND), &ios);

        }else{
            if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
            if (!ios) reterror(-5,"[TSSC] ERROR: please specify an iOS version for this option\n\tuse -h for more help\n");
            if (!checkFirmwareForDeviceExists(device, ios, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-6, "[TSSC] ERROR: either device %s does not exist, or there is no iOS %s for it.\n",device,ios);
            
            isSigned = isVersionSignedForDevice(firmwareJson, firmwareTokens, ios, device, devVals, (flags & FLAG_OTA), !(flags & FLAG_NO_BASEBAND), (flags & FLAG_BETA));
        }
        
        if (isSigned >=0) printf("\niOS %s for device %s %s being signed!\n",ios,device, (isSigned) ? "IS" : "IS NOT");
        else printf("\nERROR: checking tss status failed!\n");
    }
    
    
    
error:
    if (devVals.apnonce) free(devVals.apnonce);
    if (devVals.sepnonce) free(devVals.sepnonce);
    if (firmwareJson) free(firmwareJson);
    if (firmwareTokens) free(firmwareTokens);
    return err ? err : isSigned;
#undef reterror
}
