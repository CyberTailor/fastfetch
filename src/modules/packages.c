#include "fastfetch.h"

#include <string.h>
#include <dirent.h>

#define FF_PACKAGES_MODULE_NAME "Packages"
#define FF_PACKAGES_NUM_FORMAT_ARGS 9

#ifdef FF_HAVE_RPM
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlog.h>

static uint32_t getRpmPackageCount(FFinstance* instance)
{
    FF_LIBRARY_LOAD(rpm, instance->config.librpm, 0, "librpm.so", "librpm.so.4")
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmReadConfigFiles, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmtsCreate, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmtsInitIterator, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmdbGetIteratorCount, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmdbFreeIterator, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmtsFree, 0)
    FF_LIBRARY_LOAD_SYMBOL(rpm, rpmlogSetMask, 0)

    // Don't print any error messages
    ffrpmlogSetMask(RPMLOG_MASK(RPMLOG_EMERG));

    uint32_t count = 0;
    rpmts ts = NULL;
    rpmdbMatchIterator mi = NULL;

    if (ffrpmReadConfigFiles(NULL, NULL)) goto exit;
    if (!(ts = ffrpmtsCreate())) goto exit;
    if (!(mi = ffrpmtsInitIterator(ts, RPMDBI_LABEL, NULL, 0))) goto exit;
    count = (uint32_t) ffrpmdbGetIteratorCount(mi);

exit:
    if (mi) ffrpmdbFreeIterator(mi);
    if (ts) ffrpmtsFree(ts);
    dlclose(rpm);
    return count;
}

#endif

static uint32_t getNumElements(const char* dirname, unsigned char type)
{
    DIR* dirp = opendir(dirname);
    if(dirp == NULL)
        return 0;

    uint32_t num_elements = 0;

    struct dirent *entry;
    while((entry = readdir(dirp)) != NULL) {
        if(entry->d_type == type)
            ++num_elements;
    }

    if(type == DT_DIR)
        num_elements -= 2; // accounting for . and ..

    closedir(dirp);

    return num_elements;
}

static uint32_t getNumStrings(const char* filename, const char* needle)
{
    FILE* file = fopen(filename, "r");
    if(file == NULL)
        return 0;

    uint32_t count = 0;

    char* line = NULL;
    size_t len = 0;

    while(getline(&line, &len, file) != EOF)
    {
        if(strstr(line, needle) != NULL)
            ++count;
    }

    if(line != NULL)
        free(line);

    fclose(file);

    return count;
}

static uint32_t countFilesRecursive(FFstrbuf* baseDirPath, const char* filename)
{
    uint32_t baseDirPathLength = baseDirPath->length;

    ffStrbufAppendC(baseDirPath, '/');
    ffStrbufAppendS(baseDirPath, filename);
    bool exists = ffFileExists(baseDirPath->chars, S_IFREG);
    ffStrbufSubstrBefore(baseDirPath, baseDirPathLength);
    if(exists)
        return 1;

    DIR* dirp = opendir(baseDirPath->chars);
    if(dirp == NULL)
        return 0;

    ffStrbufAppendC(baseDirPath, '/');
    baseDirPathLength = baseDirPath->length;

    uint32_t sum = 0;

    struct dirent *entry;
    while((entry = readdir(dirp)) != NULL) {
        // According to the PMS, neither category nor package name can begin with '.', so no need to check for . or .. specifically
        if(entry->d_type != DT_DIR || entry->d_name[0] == '.')
            continue;

        ffStrbufAppendS(baseDirPath, entry->d_name);
        sum += countFilesRecursive(baseDirPath, filename);
        ffStrbufSubstrBefore(baseDirPath, baseDirPathLength);
    }

    closedir(dirp);
    return sum;
}

static uint32_t countFilesIn(const char* dirname, const char* filename)
{
    FFstrbuf baseDirPath;
    ffStrbufInitA(&baseDirPath, 128);
    ffStrbufAppendS(&baseDirPath, dirname);
    uint32_t result = countFilesRecursive(&baseDirPath, filename);
    ffStrbufDestroy(&baseDirPath);
    return result;
}

void ffPrintPackages(FFinstance* instance)
{
    uint32_t pacman = getNumElements("/var/lib/pacman/local", DT_DIR);
    uint32_t dpkg = getNumStrings("/var/lib/dpkg/status", "Status: ");

    #if __ANDROID__
        dpkg += getNumStrings("/data/data/com.termux/files/usr/var/lib/dpkg/status", "Status: ");
    #endif

    #ifdef FF_HAVE_RPM
        uint32_t rpm = getRpmPackageCount(instance);
    #else
        uint32_t rpm = 0;
    #endif

    uint32_t emerge = countFilesIn("/var/db/pkg", "SIZE");
    uint32_t xbps = getNumElements("/var/db/xbps", DT_REG);
    uint32_t flatpak = getNumElements("/var/lib/flatpak/app", DT_DIR);
    uint32_t snap = getNumElements("/snap", DT_DIR);

    //Accounting for the /snap/bin folder
    if(snap > 0)
        --snap;

    uint32_t all = pacman + dpkg + rpm + emerge + xbps + flatpak + snap;

    if(all == 0)
    {
        ffPrintError(instance, FF_PACKAGES_MODULE_NAME, 0, &instance->config.packagesKey, &instance->config.packagesFormat, FF_PACKAGES_NUM_FORMAT_ARGS, "No packages from known package managers found");
        return;
    }

    FFstrbuf manjaroBranch;
    ffStrbufInit(&manjaroBranch);
    if(ffParsePropFile("/etc/pacman-mirrors.conf", "Branch =", &manjaroBranch) && manjaroBranch.length == 0)
        ffStrbufSetS(&manjaroBranch, "stable");

    if(instance->config.packagesFormat.length == 0)
    {
        ffPrintLogoAndKey(instance, FF_PACKAGES_MODULE_NAME, 0, &instance->config.batteryKey);

        #define FF_PRINT_PACKAGE(name) \
        if(name > 0) \
        { \
            printf("%u ("#name")", name); \
            if((all = all - name) > 0) \
                printf(", "); \
        };

        if(pacman > 0)
        {
            printf("%u (pacman)", pacman);
            if(manjaroBranch.length > 0)
                printf("[%s]", manjaroBranch.chars);
            if((all = all - pacman) > 0)
                printf(", ");
        };

        FF_PRINT_PACKAGE(dpkg)
        FF_PRINT_PACKAGE(rpm)
        FF_PRINT_PACKAGE(emerge)
        FF_PRINT_PACKAGE(xbps)
        FF_PRINT_PACKAGE(flatpak)
        FF_PRINT_PACKAGE(snap)

        //Fix linter warning of unused value of all
        (void) all;

        #undef FF_PRINT_PACKAGE

        putchar('\n');
    }
    else
    {
        ffPrintFormatString(instance, FF_PACKAGES_MODULE_NAME, 0, &instance->config.packagesKey, &instance->config.packagesFormat, NULL, FF_PACKAGES_NUM_FORMAT_ARGS, (FFformatarg[]){
            {FF_FORMAT_ARG_TYPE_UINT, &all},
            {FF_FORMAT_ARG_TYPE_UINT, &pacman},
            {FF_FORMAT_ARG_TYPE_STRBUF, &manjaroBranch},
            {FF_FORMAT_ARG_TYPE_UINT, &dpkg},
            {FF_FORMAT_ARG_TYPE_UINT, &rpm},
            {FF_FORMAT_ARG_TYPE_UINT, &emerge},
            {FF_FORMAT_ARG_TYPE_UINT, &xbps},
            {FF_FORMAT_ARG_TYPE_UINT, &flatpak},
            {FF_FORMAT_ARG_TYPE_UINT, &snap}
        });
    }

    ffStrbufDestroy(&manjaroBranch);
}
