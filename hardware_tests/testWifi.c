#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

typedef enum { false, true } bool;

char* getServiceName(char* ssid) {
        char* search_ssid = malloc(strlen(ssid) + 50);
        sprintf(search_ssid, "connmanctl services | grep \"%s\" | grep -o \"wifi.*\"", ssid);

        FILE* cmd = popen(search_ssid, "r");
        if (cmd == NULL) return NULL;
        char* service;
        size_t len = 0;
        getline(&service, &len, cmd);

        free(search_ssid);
        if (pclose(cmd) != 0) return "";
        else return service;
}

void scanWifi() {
        FILE* cmd = popen("connmanctl scan wifi", "r");
        if (cmd == NULL) exit(1);
        pclose(cmd);
}

bool ping() {
        FILE* cmd = popen("ping -c2 8.8.8.8", "r");
        if (cmd == NULL) exit(1);
        if (pclose(cmd) == -1) return false;
        else return true;
}

bool enableWifi() {
        FILE* cmd = popen("connmanctl enable wifi 2>&1", "r");
        if (cmd == NULL) exit(1);
        char* resp;
        size_t len = 0;
        getline(&resp, &len, cmd);

        pclose(cmd);
        return (strstr(resp, "Enabled wifi") || strstr(resp, "Already enabled"));
}


bool disableWifi() {
        FILE* cmd = popen("connmanctl disable wifi 2>&1", "r");
        if (cmd == NULL) exit(1);
        char* resp;
        size_t len = 0;
        getline(&resp, &len, cmd);

        pclose(cmd);
        return (strstr(resp, "Disabled wifi") || strstr(resp, "Already disabled"));
}

bool connectWifi(char* service, char* passphrase, int send, int receive) {
        char buffer[BUFSIZ];
        int try_count;
        int threshold = 40;
        fcntl(receive, F_SETFL, O_NONBLOCK);

	fprintf(stderr, "Wifi connection ...\n");
        write(send, "agent on\n", strlen("agent on\n") + 1);
        try_count = 0;
        while(try_count < threshold)
        {
                if(read(receive, buffer, BUFSIZ) != -1)
                {
                        if(strstr(buffer, "Agent registered") != NULL) break;
                        else try_count++;
                }
                else try_count++;
                usleep(250000);
        }
        if(try_count == threshold)
        {
                fprintf(stderr, "Failed: unable to register agent.\n");
                return false;
        }
        else fprintf(stderr, "Agent registered\n");

        bool connected = false;
        char* connect = malloc(strlen(service) + 10);
        sprintf(connect, "connect %s\n", service);
        write(send, connect, strlen(connect) + 1);
        free(connect);
        try_count = 0;
        while(try_count < threshold)
        {
                if(read(receive, buffer, BUFSIZ) != -1)
                {
                        if(strstr(buffer, "Passphrase?") != NULL) break;
                        else if(strstr(buffer, "Connected") != NULL) {
                                fprintf(stderr, "Connected to known %s\n", service);
                                connected = true;
                                break;
                        }
                        else if(strstr(buffer, "Already connected") != NULL) {
                                fprintf(stderr, "Already connected to %s\n", service);
                                connected = true;
                                break;
                        }
                        else try_count++;
                }
                else try_count++;
                usleep(250000);
        }
        if(try_count == threshold)
        {
                fprintf(stderr, "Failed: unable to connect.\n");
                return false;
        }

        if (connected == false) {
                char* psk = malloc(strlen(passphrase) + 2);
                sprintf(psk, "%s\n", passphrase);
                write(send, psk, strlen(psk) + 1);
                free(psk);
                try_count = 0;
                while(try_count < threshold)
                {
                        if(read(receive, buffer, BUFSIZ) != -1)
                        {
                                if(strstr(buffer, "Connected") != NULL) break;
                                else try_count++;
                        }
                        else try_count++;
                        usleep(250000);
                }
                if(try_count == threshold)
                {
                        fprintf(stderr, "Failed: unable to connect.\n");
                        return false;
                }
                else fprintf(stderr, "Connected to %s\n", service);
        }

        write(send, "agent off\n", strlen("agent off\n") + 1);
        try_count = 0;
        while(try_count < threshold)
        {
                if(read(receive, buffer, BUFSIZ) != -1)
                {
                        if(strstr(buffer, "Agent unregistered") != NULL) break;
                        else try_count++;
                }
                else try_count++;
                usleep(250000);
        }
        if(try_count == threshold)
        {
                fprintf(stderr, "Failed: unable to unregister agent.\n");
                return false;
        }
        else fprintf(stderr, "Agent unregistered\n");
	return true;
}

bool disconnectWifi(char* service, int send, int receive) {
        char buffer[BUFSIZ];
        int try_count;
        int threshold = 40;
        fcntl(receive, F_SETFL, O_NONBLOCK);

	fprintf(stderr, "Wifi disconnection ...\n");
        char* disconnect = malloc(strlen(service) + 13);
        sprintf(disconnect, "disconnect %s\n", service);
        write(send, disconnect, strlen(disconnect) + 1);
        free(disconnect);
        try_count = 0;
        while(try_count < threshold)
        {
                if(read(receive, buffer, BUFSIZ) != -1)
                {
                        if(strstr(buffer, "Disconnected") != NULL) break;
                        else try_count++;
                }
                else try_count++;
                usleep(250000);
        }
        if(try_count == threshold)
        {
                fprintf(stderr, "Failed: unable to disconnect.\n");
                return false;
        }
        else fprintf(stderr, "Disconnected from %s\n", service);
	return true;
}

char* checkSSID(char* ssid) {
        char* service;
        int threshold = 5;
        for (int i = 1; i <= threshold; i++) {
                if (i == 1) service = malloc(BUFSIZ);
                else service = realloc(service, BUFSIZ);
                strcpy(service, getServiceName(ssid));

                fprintf(stderr, "\rSearching for SSID '%s' ... attempt %d of %d", ssid, i, threshold);
                if (strlen(service) > 0) break;
                sleep(1);
                scanWifi();
        }
        fprintf(stderr, "\n");
        if (strlen(service) > 0) {
                fprintf(stderr, "SSID found\n");
                return strtok(service, "\n");
        }
        else{
                fprintf(stderr, "Failed: SSID: %s not found\n", ssid);
                return NULL;
        }
}

int main(int argc, char *argv[])
{
        if (argc != 3) {
                fprintf(stderr, "Failed: No SSID and/or no passphrase !\n");
                fprintf(stderr, "Usage: %s <ssid> <Passphrase>\n", argv[0]);
                return 1;
        }

        char* ssid = argv[1];
        char* passphrase = argv[2];

        int send_pipe[2];
        if (pipe(send_pipe) != 0) {
                fprintf(stderr, "Failed: Unable to setup send_pipe");
                return 1;
        }

        int receive_pipe[2];
        if (pipe(receive_pipe) != 0) {
                fprintf(stderr, "Failed: Unable to setup receive_pipe");
                return 1;
        }

        pid_t pid;
        switch (pid = fork()) {
        case (-1):
                fprintf(stderr, "Failed: Unable to fork");
                return 1;
                break;
        case 0:
                close(send_pipe[1]);
                close(receive_pipe[0]);

                dup2(send_pipe[0], 0);
                dup2(receive_pipe[1], 1);
                dup2(receive_pipe[1], 2);

                close(send_pipe[0]);
                close(receive_pipe[1]);

                char *args_connman[] = { "connmanctl", NULL };
                execvp("connmanctl", args_connman);
                break;
        default:
                close(send_pipe[0]);
                close(receive_pipe[1]);

                fprintf(stderr, "Power on wifi : ");
                bool state = enableWifi();
                fprintf(stderr, " %s\n", state ? "Success" : "Failed");
                if (state) {
                        char* service = checkSSID(ssid);
			if (service != NULL) {
                            state = connectWifi(service, passphrase, send_pipe[1], receive_pipe[0]);
                            if (state) {
                                fprintf(stderr, "Network:");
                                state = ping();
                                fprintf(stderr, " %s\n", state ? "online" : "offline");
                                disconnectWifi(service, send_pipe[1], receive_pipe[0]);
                            }
			    free(service);
			}
                }

		fprintf(stderr, "Power off wifi : ");
		state = disableWifi();
		fprintf(stderr, " %s\n", state ? "Success" : "Failed");

                close(send_pipe[1]);
                close(receive_pipe[0]);
                break;
        }

        return 0;
}
