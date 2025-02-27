#include "fastfetch.h"

#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

// Things only needed by fastfetch
typedef struct FFdata
{
    FFvaluestore valuestore;
    FFstrbuf structure;
    FFstrbuf logoName;
    FFstrbuf logoColors[FASTFETCH_LOGO_MAX_COLORS];
    bool multithreading;
} FFdata;

static void constructAndPrintCommandHelpFormat(const char* name, const char* def, uint32_t numArgs, ...)
{
    va_list argp;
    va_start(argp, numArgs);

    printf("--%s-format:\n", name);
    printf("Sets the format string for %s output.\n", name);
    puts("To see how a format string is constructed, take a look at \"fastfetch --help format\".");
    puts("The following values are passed:");

    for(uint32_t i = 1; i <= numArgs; i++)
        printf("        {%u}: %s\n", i, va_arg(argp, const char*));

    printf("The default is something like \"%s\".\n", def);

    va_end(argp);
}

static inline void printCommandHelp(const char* command)
{
    if(command == NULL)
        fputs(FASTFETCH_DATATEXT_HELP, stdout);
    else if(strcasecmp(command, "c") == 0 || strcasecmp(command, "color") == 0)
        fputs(FASTFETCH_DATATEXT_HELP_COLOR, stdout);
    else if(strcasecmp(command, "format") == 0)
        fputs(FASTFETCH_DATATEXT_HELP_FORMAT, stdout);
    else if(strcasecmp(command, "load-config") == 0 || strcasecmp(command, "loadconfig") == 0 || strcasecmp(command, "config") == 0)
        fputs(FASTFETCH_DATATEXT_HELP_CONFIG, stdout);
    else if(strcasecmp(command, "os-format") == 0)
    {
        constructAndPrintCommandHelpFormat("os", "{3} {12}", 12,
            "System name (typically just Linux)",
            "Name of the OS",
            "Pretty name of the OS",
            "ID of the OS",
            "ID like of the OS",
            "Variant of the OS",
            "Variant ID of the OS",
            "Version of the OS",
            "Version ID of the OS",
            "Version codename of the OS",
            "Build ID of the OS",
            "Architecture of the OS"
        );
    }
    else if(strcasecmp(command, "host-format") == 0)
    {
        constructAndPrintCommandHelpFormat("host", "{2} {3}", 3,
            "Host family",
            "Host name",
            "Host version"
        );
    }
    else if(strcasecmp(command, "kernel-format") == 0)
    {
        constructAndPrintCommandHelpFormat("kernel", "{2}", 3,
            "Kernel sysname",
            "Kernel release",
            "Kernel version"
        );
    }
    else if(strcasecmp(command, "uptime-format") == 0)
    {
        constructAndPrintCommandHelpFormat("uptime", "{} days {} hours {} mins", 4,
            "Days",
            "Hours",
            "Minutes",
            "Seconds"
        );
    }
    else if(strcasecmp(command, "processes-format") == 0)
    {
        constructAndPrintCommandHelpFormat("processes", "{}", 1,
            "Count"
        );
    }
    else if(strcasecmp(command, "packages-format") == 0)
    {
        constructAndPrintCommandHelpFormat("packages", "{2} (pacman){?3}[{3}]{?}, {4} (dpkg), {5} (rpm), {6} (emerge), {7} (xps), {8}, (flatpak), {9} (snap)", 9,
            "Number of all packages",
            "Number of pacman packages",
            "Pacman branch on manjaro",
            "Number of dpkg packages",
            "Number of rpm packages",
            "Number of emerge packages",
            "Number of xbps packages",
            "Number of flatpak packages",
            "Number of snap packages"
        );
    }
    else if(strcasecmp(command, "shell-format") == 0)
    {
        constructAndPrintCommandHelpFormat("shell", "{3} {4}", 7,
            "Shell process name",
            "Shell path with exe name",
            "Shell exe name",
            "Shell version",
            "User shell path with exe name",
            "User shell exe name",
            "User shell version"
        );
    }
    else if(strcasecmp(command, "resolution-format") == 0)
    {
        constructAndPrintCommandHelpFormat("resolution", "{}x{} @ {}Hz", 3,
            "Screen width",
            "Screen height",
            "Screen refresh rate"
        );
    }
    else if(strcasecmp(command, "de-format") == 0)
    {
        constructAndPrintCommandHelpFormat("de", "{2} {3}", 3,
            "DE process name",
            "DE pretty name",
            "DE version"
        );
    }
    else if(strcasecmp(command, "wm-format") == 0)
    {
        constructAndPrintCommandHelpFormat("wm", "{2} ({3})", 3,
            "WM process name",
            "WM pretty name",
            "WM protocol name"
        );
    }
    else if(strcasecmp(command, "wm-theme-format") == 0)
    {
        constructAndPrintCommandHelpFormat("wm-theme", "{}", 1,
            "WM theme name"
        );
    }
    else if(strcasecmp(command, "theme-format") == 0)
    {
        constructAndPrintCommandHelpFormat("theme", "{} ({3}) [Plasma], {7}", 7,
            "Plasma theme",
            "Plasma color scheme",
            "Plasma color scheme pretty",
            "GTK2 theme",
            "GTK3 theme",
            "GTK4 theme",
            "Combined GTK themes"
        );
    }
    else if(strcasecmp(command, "icons-format") == 0)
    {
        constructAndPrintCommandHelpFormat("icons", "{} [Plasma], {5}", 5,
            "Plasma icons",
            "GTK2 icons",
            "GTK3 icons",
            "GTK4 icons",
            "Combined GTK icons"
        );
    }
    else if(strcasecmp(command, "font-format") == 0)
    {
        constructAndPrintCommandHelpFormat("font", "{5} [Plasma], {21}", 21,
            "Plasma raw",
            "Plasma name",
            "Plasma size",
            "Plasma styles",
            "Plasma pretty",
            "GTK2 raw",
            "GTK2 name",
            "GTK2 size",
            "GTK2 styles",
            "GTK2 pretty",
            "GTK3 raw",
            "GTK3 name",
            "GTK3 size",
            "GTK3 styles",
            "GTK3 pretty",
            "GTK4 raw",
            "GTK4 name",
            "GTK4 size",
            "GTK4 styles",
            "GTK4 pretty",
            "GTK2/3/4 pretty"
        );
    }
    else if(strcasecmp(command, "cursor-format") == 0)
    {
        constructAndPrintCommandHelpFormat("cursor", "{} ({}pt)", 2,
            "Cursor theme",
            "Cursor size"
        );
    }
    else if(strcasecmp(command, "terminal-format") == 0)
    {
        constructAndPrintCommandHelpFormat("terminal", "{3}", 3,
            "Terminal process name",
            "Terminal path with exe name",
            "Terminal exe name"
        );
    }
    else if(strcasecmp(command, "terminal-font-format") == 0)
    {
        constructAndPrintCommandHelpFormat("terminal-font", "{5}", 5,
            "Terminal font raw",
            "Terminal font name",
            "Termianl font size",
            "Terminal font styles"
            "Terminal font pretty"
        );
    }
    else if(strcasecmp(command, "cpu-format") == 0)
    {
        constructAndPrintCommandHelpFormat("cpu", "{2} ({7}) @ {14}GHz", 14,
            "CPU name",
            "Prettified CPU name",
            "CPU Vendor name (Vendor ID)",
            "CPU logical core count online",
            "CPU logical core count configured",
            "CPU physical core count",
            "Always set core count",
            "frequency bios limit",
            "frequency scaling max",
            "frequency scaling min",
            "frequency info max",
            "frequency info min",
            "frequeny from /proc/cpuinfo",
            "most accurate frequeny"
        );
    }
    else if(strcasecmp(command, "cpu-usage-format") == 0)
    {
        constructAndPrintCommandHelpFormat("cpu-usage", "{0}%", 1,
            "CPU usage without percent mark"
        );
    }
    else if(strcasecmp(command, "gpu-format") == 0)
    {
        constructAndPrintCommandHelpFormat("gpu", "{2} {4}", 5,
            "GPU vendor",
            "GPU vendor pretty",
            "GPU name",
            "GPU name pretty",
            "GPU driver"
        );
    }
    else if(strcasecmp(command, "memory-format") == 0)
    {
        constructAndPrintCommandHelpFormat("memory", "{}MiB / {}MiB ({}%)", 3,
            "Used memory",
            "Total memory",
            "Used memory percentage"
        );
    }
    else if(strcasecmp(command, "disk-format") == 0)
    {
        constructAndPrintCommandHelpFormat("disk", "{}GB / {}GB ({4}%)", 4,
            "Used disk space",
            "Total disk space",
            "Number of files",
            "Used disk space percentage"
        );
    }
    else if(strcasecmp(command, "battery-format") == 0)
    {
        constructAndPrintCommandHelpFormat("battery", "{}%, {}", 5,
            "Battery manufactor",
            "Battery model",
            "Battery technology",
            "Battery capacity",
            "Battery status"
        );
    }
    else if(strcasecmp(command, "locale-format") == 0)
    {
        constructAndPrintCommandHelpFormat("locale", "{}", 1,
            "Locale code"
        );
    }
    else if(strcasecmp(command, "local-ip-format") == 0)
    {
        constructAndPrintCommandHelpFormat("local-ip", "{}", 1,
            "Local IP address"
        );
    }
    else if(strcasecmp(command, "public-ip-format") == 0)
    {
        constructAndPrintCommandHelpFormat("public-ip", "{}", 1,
            "Public IP address"
        );
    }
    else if(strcasecmp(command, "player-format") == 0)
    {
        constructAndPrintCommandHelpFormat("player", "{}", 1,
            "Player name"
        );
    }
    else if(strcasecmp(command, "song-format") == 0)
    {
        constructAndPrintCommandHelpFormat("song", "{2} - {3} - {1}", 3,
            "Song name",
            "Artist name",
            "Album name"
        );
    }
    else
        fprintf(stderr, "No specific help for command %s provided\n", command);
}

static inline void listAvailablePresetsFromFolder(FFstrbuf* folder, uint8_t indentation, const char* folderName)
{
    DIR* dir = opendir(folder->chars);
    if(dir == NULL)
        return;

    uint32_t folderLength = folder->length;

    if(folderName != NULL)
        printf("%s/\n", folderName);

    struct dirent* entry;

    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_DIR)
        {

            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            ffStrbufAppendS(folder, entry->d_name);
            ffStrbufAppendC(folder, '/');
            listAvailablePresetsFromFolder(folder, (uint8_t) (indentation + 1), entry->d_name);
            ffStrbufSubstrBefore(folder, folderLength);
            continue;
        }

        for(uint8_t i = 0; i < indentation; i++)
            fputs("  | ", stdout);

        puts(entry->d_name);
    }

    closedir(dir);
}

static inline void listAvailablePresets(FFinstance* instance)
{
    FFstrbuf folder;
    ffStrbufInitA(&folder, 64);

    ffStrbufSetS(&folder, instance->state.passwd->pw_dir);
    ffStrbufAppendS(&folder, "/.local/share/fastfetch/presets/");
    listAvailablePresetsFromFolder(&folder, 0, NULL);

    ffStrbufSetS(&folder, "/usr/share/fastfetch/presets/");
    listAvailablePresetsFromFolder(&folder, 0, NULL);

    ffStrbufDestroy(&folder);
}

static void parseOption(FFinstance* instance, FFdata* data, const char* key, const char* value);

static void parseConfigFile(FFinstance* instance, FFdata* data, FILE* file)
{
    char* lineStart = NULL;
    size_t len = 0;
    ssize_t read;

    FFstrbuf line;
    ffStrbufInitA(&line, 256); //The default structure line needs this size

    while ((read = getline(&lineStart, &len, file)) != -1)
    {
        ffStrbufSetS(&line, lineStart);
        ffStrbufTrimRight(&line, '\n');
        ffStrbufTrim(&line, ' ');

        if(line.length == 0 || line.chars[0] == '#')
            continue;

        uint32_t firstSpace = ffStrbufFirstIndexC(&line, ' ');

        if(firstSpace >= line.length)
        {
            parseOption(instance, data, line.chars, NULL);
            continue;
        }

        //Separate key and value by simply replacing the first space with a \0
        char* valueStart = &line.chars[firstSpace];
        *valueStart = '\0';
        ++valueStart;

        //Trim whitespace at beginning of value
        while(*valueStart == ' ')
            ++valueStart;

        //If we want whitespace in values, we need to quote it. This is done to keep consistency with shell.
        if(*valueStart == '"')
        {
            char* last = line.chars + line.length - 1;
            if(*last == '"')
            {
                ++valueStart;
                *last = '\0';
                --line.length;
            }
        }

        parseOption(instance, data, line.chars, valueStart);
    }

    ffStrbufDestroy(&line);

    if(lineStart != NULL)
        free(lineStart);
}

static void optionParseConfigFile(FFinstance* instance, FFdata* data, const char* key, const char* value)
{
    if(value == NULL)
    {
        fprintf(stderr, "Error: usage: %s <file>\n", key);
        exit(413);
    }

    FILE* file = fopen(value, "r");
    if(file != NULL)
    {
        parseConfigFile(instance, data, file);
        fclose(file);
        return;
   }

    FFstrbuf filename;
    ffStrbufInitA(&filename, 64);

    ffStrbufAppendS(&filename, instance->state.passwd->pw_dir);
    ffStrbufAppendS(&filename, "/.local/share/fastfetch/presets/");
    ffStrbufAppendS(&filename, value);

    file = fopen(filename.chars, "r");
    if(file != NULL)
    {
        parseConfigFile(instance, data, file);
        fclose(file);
        return;
    }

    ffStrbufSetS(&filename, "/usr/share/fastfetch/presets/");
    ffStrbufAppendS(&filename, value);

    file = fopen(filename.chars, "r");
    if(file != NULL)
    {
        parseConfigFile(instance, data, file);
        fclose(file);
        return;
    }

    fprintf(stderr, "Error: couldn't find config: %s\n", value);
    exit(414);
}

static bool optionParseBoolean(const char* str)
{
    return (
        !ffStrSet(str) ||
        strcasecmp(str, "true") == 0 ||
        strcasecmp(str, "yes")  == 0 ||
        strcasecmp(str, "on")   == 0 ||
        strcasecmp(str, "1")    == 0
    );
}

static inline void optionCheckString(const char* key, const char* value, FFstrbuf* buffer)
{
    if(value == NULL)
    {
        fprintf(stderr, "Error: usage: %s <str>\n", key);
        exit(477);
    }
    ffStrbufEnsureFree(buffer, 63); //This is not needed, as ffStrbufSetS will resize capacity if needed, but giving a higher start should improve performance
}

static void optionParseString(const char* key, const char* value, FFstrbuf* buffer)
{
    optionCheckString(key, value, buffer);
    ffStrbufSetS(buffer, value);
}

static void optionParseColor(const char* key, const char* value, FFstrbuf* buffer)
{
    optionCheckString(key, value, buffer);

    static const char reset[] = "reset_";
    static const char bright[] = "bright_";

    static const char black[] = "black";
    static const char red[] = "red";
    static const char green[] = "green";
    static const char yellow[] = "yellow";
    static const char blue[] = "blue";
    static const char magenta[] = "magenta";
    static const char cyan[] = "cyan";
    static const char white[] = "white";

    while(*value != '\0')
    {
        if(strncasecmp(value, reset, sizeof(reset) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "0;");
            value += sizeof(reset) - 1;
        }
        else if(strncasecmp(value, bright, sizeof(bright) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "1;");
            value += sizeof(bright) - 1;
        }
        else if(strncasecmp(value, black, sizeof(black) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "30");
            value += sizeof(black) - 1;
        }
        else if(strncasecmp(value, red, sizeof(red) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "31");
            value += sizeof(red) - 1;
        }
        else if(strncasecmp(value, green, sizeof(green) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "32");
            value += sizeof(green) - 1;
        }
        else if(strncasecmp(value, yellow, sizeof(yellow) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "33");
            value += sizeof(yellow) - 1;
        }
        else if(strncasecmp(value, blue, sizeof(blue) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "34");
            value += sizeof(blue) - 1;
        }
        else if(strncasecmp(value, magenta, sizeof(magenta) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "35");
            value += sizeof(magenta) - 1;
        }
        else if(strncasecmp(value, cyan, sizeof(cyan) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "36");
            value += sizeof(cyan) - 1;
        }
        else if(strncasecmp(value, white, sizeof(white) - 1) == 0)
        {
            ffStrbufAppendS(buffer, "37");
            value += sizeof(white) - 1;
        }
        else
        {
            ffStrbufAppendC(buffer, *value);
            ++value;
        }
    }
}

static void parseOption(FFinstance* instance, FFdata* data, const char* key, const char* value)
{
    if(strcasecmp(key, "-h") == 0 || strcasecmp(key, "--help") == 0)
    {
        printCommandHelp(value);
        exit(0);
    }
    else if(strcasecmp(key, "-v") == 0 || strcasecmp(key, "--version") == 0)
    {
        puts(FASTFETCH_PROJECT_NAME" "FASTFETCH_PROJECT_VERSION);
        exit(0);
    }
    else if(strcasecmp(key, "--list-logos") == 0)
    {
        ffListLogos();
        exit(0);
    }
    else if(strcasecmp(key, "--list-logos-autocompletion") == 0)
    {
        ffListLogosForAutocompletion();
        exit(0);
    }
    else if(strcasecmp(key, "--print-logos") == 0)
    {
        ffPrintLogos(instance);
        exit(0);
    }
    else if(strcasecmp(key, "--print-config") == 0)
    {
        fputs(FASTFETCH_DATATEXT_CONFIG, stdout);
        exit(0);
    }
    else if(strcasecmp(key, "--print-structure") == 0)
    {
        puts(FASTFETCH_DATATEXT_STRUCTURE);
        exit(0);
    }
    else if(strcasecmp(key, "--list-modules") == 0)
    {
        fputs(FASTFETCH_DATATEXT_MODULES, stdout);
        exit(0);
    }
    else if(strcasecmp(key, "--list-presets") == 0)
    {
        listAvailablePresets(instance);
        exit(0);
    }
    else if(strcasecmp(key, "--list-features") == 0)
    {
        ffListFeatures();
        exit(0);
    }
    else if(strcasecmp(key, "--spacing") == 0)
    {
        if(value == NULL)
        {
            fprintf(stderr, "Error: usage: %s <width>\n", key);
            exit(404);
        }
        if(sscanf(value, "%hu", &instance->config.logoKeySpacing) != 1)
        {
            fprintf(stderr, "Error: couldn't parse %s to uint16_t\n", value);
            exit(405);
        }
    }
    else if(strcasecmp(key, "-x") == 0 || strcasecmp(key, "--offsetx") == 0)
    {
        if(value == NULL)
        {
            fprintf(stderr, "Error: usage: %s <offset>\n", key);
            exit(408);
        }
        if(sscanf(value, "%hi", &instance->config.offsetx) != 1)
        {
            fprintf(stderr, "Error: couldn't parse %s to int16_t\n", value);
            exit(409);
        }
    }
    else if(strcasecmp(key, "--set") == 0)
    {
        if(value == NULL)
        {
            fprintf(stderr, "Error: usage: %s <key=value>\n", key);
            exit(411);
        }

        char* separator = strchr(value, '=');

        if(separator == NULL)
        {
            fprintf(stderr, "Error: usage: %s <key=value>, '=' missing\n", key);
            exit(412);
        }

        *separator = '\0';

        ffValuestoreSet(&data->valuestore, value, separator + 1);
    }
    else if(strcasecmp(key, "-r") == 0 || strcasecmp(key, "--recache") == 0)
    {
        //Set cacheSave as well, beacuse the user expects the values to be cached when expliciting using --recache
        instance->config.recache = optionParseBoolean(value);
        instance->config.cacheSave = instance->config.recache;
    }
    else if(strcasecmp(key, "--nocache") == 0)
    {
        instance->config.recache = optionParseBoolean(value);
        instance->config.cacheSave = false;
    }
    else if(strcasecmp(key, "--load-config") == 0)
        optionParseConfigFile(instance, data, key, value);
    else if(strcasecmp(key, "--show-errors") == 0)
        instance->config.showErrors = optionParseBoolean(value);
    else if(strcasecmp(key, "--color-logo") == 0)
        instance->config.colorLogo = optionParseBoolean(value);
    else if(strcasecmp(key, "--print-remaining-logo") == 0)
        instance->config.printRemainingLogo = optionParseBoolean(value);
    else if(strcasecmp(key, "--multithreading") == 0)
        data->multithreading = optionParseBoolean(value);
    else if(strcasecmp(key, "--allow-slow-operations") == 0)
        instance->config.allowSlowOperations = optionParseBoolean(value);
    else if(strcasecmp(key, "--disable-linewrap") == 0)
        instance->config.disableLinewrap = optionParseBoolean(value);
    else if(strcasecmp(key, "--hide-cursor") == 0)
        instance->config.hideCursor = optionParseBoolean(value);
    else if(strcasecmp(key, "--logo-raw") == 0)
        instance->config.userLogoIsRaw = optionParseBoolean(value);
    else if(strcasecmp(key, "--structure") == 0)
        optionParseString(key, value, &data->structure);
    else if(strcasecmp(key, "-l") == 0 || strcasecmp(key, "--logo") == 0)
        optionParseString(key, value, &data->logoName);
    else if(strcasecmp(key, "-s") == 0 || strcasecmp(key, "--separator") == 0)
        optionParseString(key, value, &instance->config.separator);
    else if(strcasecmp(key, "-c") == 0 || strcasecmp(key, "--color") == 0)
        optionParseColor(key, value, &instance->config.color);
    else if(strcasecmp(key, "--os-format") == 0)
        optionParseString(key, value, &instance->config.osFormat);
    else if(strcasecmp(key, "--os-key") == 0)
        optionParseString(key, value, &instance->config.osKey);
    else if(strcasecmp(key, "--host-format") == 0)
        optionParseString(key, value, &instance->config.hostFormat);
    else if(strcasecmp(key, "--host-key") == 0)
        optionParseString(key, value, &instance->config.hostKey);
    else if(strcasecmp(key, "--kernel-format") == 0)
        optionParseString(key, value, &instance->config.kernelFormat);
    else if(strcasecmp(key, "--kernel-key") == 0)
        optionParseString(key, value, &instance->config.kernelKey);
    else if(strcasecmp(key, "--uptime-format") == 0)
        optionParseString(key, value, &instance->config.uptimeFormat);
    else if(strcasecmp(key, "--uptime-key") == 0)
        optionParseString(key, value, &instance->config.uptimeKey);
    else if(strcasecmp(key, "--processes-format") == 0)
        optionParseString(key, value, &instance->config.processesFormat);
    else if(strcasecmp(key, "--processes-key") == 0)
        optionParseString(key, value, &instance->config.processesKey);
    else if(strcasecmp(key, "--packages-format") == 0)
        optionParseString(key, value, &instance->config.packagesFormat);
    else if(strcasecmp(key, "--packages-key") == 0)
        optionParseString(key, value, &instance->config.packagesKey);
    else if(strcasecmp(key, "--shell-format") == 0)
        optionParseString(key, value, &instance->config.shellFormat);
    else if(strcasecmp(key, "--shell-key") == 0)
        optionParseString(key, value, &instance->config.shellKey);
    else if(strcasecmp(key, "--resolution-format") == 0)
        optionParseString(key, value, &instance->config.resolutionFormat);
    else if(strcasecmp(key, "--resolution-key") == 0)
        optionParseString(key, value, &instance->config.resolutionKey);
    else if(strcasecmp(key, "--de-format") == 0)
        optionParseString(key, value, &instance->config.deFormat);
    else if(strcasecmp(key, "--de-key") == 0)
        optionParseString(key, value, &instance->config.deKey);
    else if(strcasecmp(key, "--wm-format") == 0)
        optionParseString(key, value, &instance->config.wmFormat);
    else if(strcasecmp(key, "--wm-key") == 0)
        optionParseString(key, value, &instance->config.wmKey);
    else if(strcasecmp(key, "--wm-theme-format") == 0)
        optionParseString(key, value, &instance->config.wmThemeFormat);
    else if(strcasecmp(key, "--wm-theme-key") == 0)
        optionParseString(key, value, &instance->config.wmThemeKey);
    else if(strcasecmp(key, "--theme-format") == 0)
        optionParseString(key, value, &instance->config.themeFormat);
    else if(strcasecmp(key, "--theme-key") == 0)
        optionParseString(key, value, &instance->config.themeKey);
    else if(strcasecmp(key, "--icons-format") == 0)
        optionParseString(key, value, &instance->config.iconsFormat);
    else if(strcasecmp(key, "--icons-key") == 0)
        optionParseString(key, value, &instance->config.iconsKey);
    else if(strcasecmp(key, "--font-format") == 0)
        optionParseString(key, value, &instance->config.fontFormat);
    else if(strcasecmp(key, "--font-key") == 0)
        optionParseString(key, value, &instance->config.fontKey);
    else if(strcasecmp(key, "--cursor-key") == 0)
        optionParseString(key, value, &instance->config.cursorKey);
    else if(strcasecmp(key, "--cursor-format") == 0)
        optionParseString(key, value, &instance->config.cursorFormat);
    else if(strcasecmp(key, "--terminal-format") == 0)
        optionParseString(key, value, &instance->config.terminalFormat);
    else if(strcasecmp(key, "--terminal-key") == 0)
        optionParseString(key, value, &instance->config.terminalKey);
    else if(strcasecmp(key, "--terminal-font-format") == 0)
        optionParseString(key, value, &instance->config.termFontFormat);
    else if(strcasecmp(key, "--terminal-font-key") == 0)
        optionParseString(key, value, &instance->config.termFontKey);
    else if(strcasecmp(key, "--cpu-format") == 0)
        optionParseString(key, value, &instance->config.cpuFormat);
    else if(strcasecmp(key, "--cpu-usage-format") == 0)
        optionParseString(key, value, &instance->config.cpuUsageFormat);
    else if(strcasecmp(key, "--cpu-key") == 0)
        optionParseString(key, value, &instance->config.cpuKey);
    else if(strcasecmp(key, "--cpu-usage-key") == 0)
        optionParseString(key, value, &instance->config.cpuUsageKey);
    else if(strcasecmp(key, "--gpu-format") == 0)
        optionParseString(key, value, &instance->config.gpuFormat);
    else if(strcasecmp(key, "--gpu-key") == 0)
        optionParseString(key, value, &instance->config.gpuKey);
    else if(strcasecmp(key, "--memory-format") == 0)
        optionParseString(key, value, &instance->config.memoryFormat);
    else if(strcasecmp(key, "--memory-key") == 0)
        optionParseString(key, value, &instance->config.memoryKey);
    else if(strcasecmp(key, "--disk-format") == 0)
        optionParseString(key, value, &instance->config.diskFormat);
    else if(strcasecmp(key, "--disk-key") == 0)
        optionParseString(key, value, &instance->config.diskKey);
    else if(strcasecmp(key, "--battery-format") == 0)
        optionParseString(key, value, &instance->config.batteryFormat);
    else if(strcasecmp(key, "--battery-key") == 0)
        optionParseString(key, value, &instance->config.batteryKey);
    else if(strcasecmp(key, "--locale-format") == 0)
        optionParseString(key, value, &instance->config.localeFormat);
    else if(strcasecmp(key, "--locale-key") == 0)
        optionParseString(key, value, &instance->config.localeKey);
    else if(strcasecmp(key, "--local-ip-key") == 0)
        optionParseString(key, value, &instance->config.localIpKey);
    else if(strcasecmp(key, "--local-ip-format") == 0)
        optionParseString(key, value, &instance->config.localIpFormat);
    else if(strcasecmp(key, "--public-ip-key") == 0)
        optionParseString(key, value, &instance->config.publicIpKey);
    else if(strcasecmp(key, "--public-ip-format") == 0)
        optionParseString(key, value, &instance->config.publicIpFormat);
    else if(strcasecmp(key, "--player-key") == 0)
        optionParseString(key, value, &instance->config.playerKey);
    else if(strcasecmp(key, "--player-format") == 0)
        optionParseString(key, value, &instance->config.playerFormat);
    else if(strcasecmp(key, "--song-key") == 0)
        optionParseString(key, value, &instance->config.songKey);
    else if(strcasecmp(key, "--song-format") == 0)
        optionParseString(key, value, &instance->config.songFormat);
    else if(strcasecmp(key, "--lib-PCI") == 0)
        optionParseString(key, value, &instance->config.libPCI);
    else if(strcasecmp(key, "--lib-vulkan") == 0)
        optionParseString(key, value, &instance->config.libVulkan);
    else if(strcasecmp(key, "--lib-wayland") == 0)
        optionParseString(key, value, &instance->config.libWayland);
    else if(strcasecmp(key, "--lib-xcb-randr") == 0)
        optionParseString(key, value, &instance->config.libXcbRandr);
    else if(strcasecmp(key, "--lib-xcb") == 0)
        optionParseString(key, value, &instance->config.libXcb);
    else if(strcasecmp(key, "--lib-Xrandr") == 0)
        optionParseString(key, value, &instance->config.libXrandr);
    else if(strcasecmp(key, "--lib-X11") == 0)
        optionParseString(key, value, &instance->config.libX11);
    else if(strcasecmp(key, "--lib-gio") == 0)
        optionParseString(key, value, &instance->config.libGIO);
    else if(strcasecmp(key, "--lib-DConf") == 0)
        optionParseString(key, value, &instance->config.libDConf);
    else if(strcasecmp(key, "--lib-dbus") == 0)
        optionParseString(key, value, &instance->config.libDBus);
    else if(strcasecmp(key, "--lib-XFConf") == 0)
        optionParseString(key, value, &instance->config.libXFConf);
    else if(strcasecmp(key, "--lib-rpm") == 0)
        optionParseString(key, value, &instance->config.librpm);
    else if(strcasecmp(key, "--disk-folders") == 0)
        optionParseString(key, value, &instance->config.diskFolders);
    else if(strcasecmp(key, "--battery-dir") == 0)
        optionParseString(key, value, &instance->config.batteryDir);
    else if(strcasecmp(key, "--separator-string") == 0)
        optionParseString(key, value, &instance->config.separatorString);
    else if(strcasecmp(key, "--localip-show-ipv4") == 0)
        instance->config.localIpShowIpV4 = optionParseBoolean(value);
    else if(strcasecmp(key, "--localip-show-ipv6") == 0)
        instance->config.localIpShowIpV6 = optionParseBoolean(value);
    else if(strcasecmp(key, "--localip-show-loop") == 0)
        instance->config.localIpShowLoop = optionParseBoolean(value);
    else if(strcasecmp(key, "--os-file") == 0)
        optionParseString(key, value, &instance->config.osFile);
    else if(strcasecmp(key, "--player-name") == 0)
        optionParseString(key, value, &instance->config.playerName);
    else if(strcasecmp(key, "--public-ip-timeout") == 0)
    {
        if(value == NULL)
        {
            fprintf(stderr, "Error: usage: %s <value>\n", key);
            exit(465);
        }

        if(sscanf(value, "%u", &instance->config.publicIpTimeout) != 1)
        {
            fprintf(stderr, "Error: couldn't parse %s to uint32_t\n", value);
            exit(466);
        }
    }
    else if(strncasecmp(key, "--color-", 7) == 0 && key[8] != '\0' && key[9] == '\0') // matches "--color-*"
    {
        //Map the number to an array index, so that '1' -> 0, '2' -> 1, etc.
        int index = (int)key[8] - 49;

        //Match only --color-[1-9]
        if(index < 0 || index >= FASTFETCH_LOGO_MAX_COLORS)
        {
            fprintf(stderr, "Error: invalid --color-[1-9] index: %c\n", key[8]);
            exit(472);
        }

        optionParseColor(key, value, &data->logoColors[index]);
    }
    else
    {
        fprintf(stderr, "Error: unknown option: %s\n", key);
        exit(400);
    }
}

static void parseDefaultConfigFile(FFinstance* instance, FFdata* data)
{
    FFstrbuf* filename = ffListGet(&instance->state.configDirs, 0);
    uint32_t filenameLength = filename->length;

    mkdir(filename->chars, S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH); //I hope everybody has a config folder, but who knows

    ffStrbufAppendS(filename, "/fastfetch/");
    mkdir(filename->chars, S_IRWXU | S_IRGRP | S_IROTH);

    ffStrbufAppendS(filename, "config.conf");

    FILE* file = fopen(filename->chars, "r");
    if(file != NULL)
    {
        parseConfigFile(instance, data, file);
        fclose(file);
        ffStrbufSubstrBefore(filename, filenameLength);
        return;
    }

    file = fopen(filename->chars, "w");
    if(file != NULL)
    {
        fputs(FASTFETCH_DATATEXT_CONFIG, file);
        fclose(file);
    }

    ffStrbufSubstrBefore(filename, filenameLength);
}

static void parseArguments(FFinstance* instance, FFdata* data, int argc, const char** argv)
{
    for(int i = 1; i < argc; i++)
    {
        if(i == argc - 1 || (
            *argv[i + 1] == '-' &&
            strcasecmp(argv[i], "--offsetx") != 0 && // --offsetx allows negative values
            strcasecmp(argv[i], "--separator-string") != 0 // Separator string can start with a -
        )) {
            parseOption(instance, data, argv[i], NULL);
        }
        else
        {
            parseOption(instance, data, argv[i], argv[i + 1]);
            ++i;
        }
    }
}

static void parseStructureCommand(FFinstance* instance, FFdata* data, const char* line)
{
    const char* setValue = ffValuestoreGet(&data->valuestore, line);
    if(setValue != NULL)
    {
        ffPrintCustom(instance, line, setValue);
        return;
    }

    if(strcasecmp(line, "break") == 0)
        ffPrintBreak(instance);
    else if(strcasecmp(line, "title") == 0)
        ffPrintTitle(instance);
    else if(strcasecmp(line, "separator") == 0)
        ffPrintSeparator(instance);
    else if(strcasecmp(line, "os") == 0)
        ffPrintOS(instance);
    else if(strcasecmp(line, "host") == 0)
        ffPrintHost(instance);
    else if(strcasecmp(line, "kernel") == 0)
        ffPrintKernel(instance);
    else if(strcasecmp(line, "uptime") == 0)
        ffPrintUptime(instance);
    else if(strcasecmp(line, "processes") == 0)
        ffPrintProcesses(instance);
    else if(strcasecmp(line, "packages") == 0)
        ffPrintPackages(instance);
    else if(strcasecmp(line, "shell") == 0)
        ffPrintShell(instance);
    else if(strcasecmp(line, "resolution") == 0)
        ffPrintResolution(instance);
    else if(strcasecmp(line, "desktopenvironment") == 0 || strcasecmp(line, "de") == 0)
        ffPrintDesktopEnvironment(instance);
    else if(strcasecmp(line, "windowmanager") == 0 || strcasecmp(line, "wm") == 0)
        ffPrintWM(instance);
    else if(strcasecmp(line, "theme") == 0)
        ffPrintTheme(instance);
    else if(strcasecmp(line, "wmtheme") == 0)
        ffPrintWMTheme(instance);
    else if(strcasecmp(line, "icons") == 0)
        ffPrintIcons(instance);
    else if(strcasecmp(line, "font") == 0)
        ffPrintFont(instance);
    else if(strcasecmp(line, "cursor") == 0)
        ffPrintCursor(instance);
    else if(strcasecmp(line, "terminal") == 0)
        ffPrintTerminal(instance);
    else if(strcasecmp(line, "terminalfont") == 0)
        ffPrintTerminalFont(instance);
    else if(strcasecmp(line, "cpu") == 0)
        ffPrintCPU(instance);
    else if(strcasecmp(line, "cpuusage") == 0)
        ffPrintCPUUsage(instance);
    else if(strcasecmp(line, "gpu") == 0)
        ffPrintGPU(instance);
    else if(strcasecmp(line, "memory") == 0)
        ffPrintMemory(instance);
    else if(strcasecmp(line, "disk") == 0)
        ffPrintDisk(instance);
    else if(strcasecmp(line, "battery") == 0)
        ffPrintBattery(instance);
    else if(strcasecmp(line, "locale") == 0)
        ffPrintLocale(instance);
    else if(strcasecmp(line, "localip") == 0)
        ffPrintLocalIp(instance);
    else if(strcasecmp(line, "publicip") == 0)
        ffPrintPublicIp(instance);
    else if(strcasecmp(line, "player") == 0)
        ffPrintPlayer(instance);
    else if(strcasecmp(line, "song") == 0)
        ffPrintSong(instance);
    else if(strcasecmp(line, "colors") == 0)
        ffPrintColors(instance);
    else
        ffPrintError(instance, line, 0, NULL, NULL, 0, "<no implementation provided>");
}

int main(int argc, const char** argv)
{
    FFinstance instance;
    ffInitInstance(&instance);

    //Data stores things only needed for the configuration of fastfetch
    FFdata data;
    ffValuestoreInit(&data.valuestore);
    ffStrbufInitA(&data.structure, 256);
    ffStrbufInitA(&data.logoName, 0);
    data.multithreading = true;

    for(uint8_t i = 0; i < FASTFETCH_LOGO_MAX_COLORS; i++)
        ffStrbufInitA(&data.logoColors[i], 0);

    parseDefaultConfigFile(&instance, &data);
    parseArguments(&instance, &data, argc, argv);

    //Load custom logo if it exists
    if(data.logoName.length > 0)
        ffLoadLogoSet(&instance, data.logoName.chars);
    else
        ffLoadLogo(&instance);

    //If we haven't set key color, use primary color of logo
    if(instance.config.color.length == 0)
        ffStrbufSet(&instance.config.color, &instance.config.logoColors[0]);

    //Overwrite logo colors with custom colors
    for(uint8_t i = 0; i < FASTFETCH_LOGO_MAX_COLORS; i++)
    {
        if(data.logoColors[i].length > 0)
            ffStrbufSet(&instance.config.logoColors[i], &data.logoColors[i]);
    }

    //Start detection threads
    if(data.multithreading)
        ffStartDetectionThreads(&instance);

    //If we don't have a custom structure, use the default one
    if(data.structure.length == 0)
        ffStrbufSetS(&data.structure, FASTFETCH_DATATEXT_STRUCTURE);

    ffStart(&instance);

    //Parse the structure and call the modules
    uint32_t startIndex = 0;
    while (startIndex < data.structure.length)
    {
        uint32_t colonIndex = ffStrbufNextIndexC(&data.structure, startIndex, ':');
        data.structure.chars[colonIndex] = '\0';

        parseStructureCommand(&instance, &data, data.structure.chars + startIndex);

        startIndex = colonIndex + 1;
    }

    ffFinish(&instance);
}
