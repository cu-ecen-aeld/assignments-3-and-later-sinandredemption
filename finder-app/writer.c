#include <stdio.h>
#include <syslog.h>

/*
*/
int main(int argc, char** argv)
{
    // Setup syslog logging for your utility using the LOG_USER facility.
    openlog("writer.c", LOG_PERROR, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Expected 3 arguments, got %d instead.", argc);
        return 1;
    }

    FILE* output = fopen(argv[1], "w");

    if (output != NULL)
    {
        // Use the syslog capability to write a message “Writing <string> to <file>” where <string> is the text string written to file (second argument) and <file> is the file created by the script.  This should be written with LOG_DEBUG level.

        syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
        fprintf(output, (char*)argv[2]);
        fclose(output);
    }
    else
    {
        // Use the syslog capability to log any unexpected errors with LOG_ERR level.
        syslog(LOG_ERR, "Couldn't open file '%s' for writing", argv[1]);
        return 1;
    }

    closelog();
    return 0;
}