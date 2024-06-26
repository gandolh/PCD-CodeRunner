#include <stdio.h>
#include <stdarg.h>
#include <time.h>

void write_log(const char *format, ...) {
    FILE *file = fopen("logs/logs.txt", "a");
    if (file == NULL) {
        printf("Error opening logs file!\n");
        return;
    }

    // Get current time
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss"
    char buf[80];
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", t);

    fprintf(file, "%s - ", buf);

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);

    fprintf(file, "\n");
    fclose(file);
}

void create_log_file() {
    FILE *file = fopen("logs/logs.txt", "w");
    if (file == NULL) {
        write_log("Error creating logs file!\n");
        return;
    }

    fclose(file);
    write_log("Log file created");
}


