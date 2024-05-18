#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dbus/dbus.h>
#include <unistd.h>
#include <regex.h>

typedef enum { false, true } bool;
typedef struct BleDevice BleDevice;
struct BleDevice
{
        DBusConnection* conn;
        char* uuid;
        char* controller;
        char* version;
};

DBusConnection* connect_bus() {
        // initialise the errorsDBusConnection* conn
        static DBusConnection* conn;
        DBusError err;
        dbus_error_init(&err);

        // connect to the bus
        conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "Failed: Connection Error (%s)\n", err.message);
                dbus_error_free(&err);
                exit(1);
        }
        if (NULL == conn) exit(1);
        return conn;
}

void start_scan(BleDevice device) {
        DBusMessage* msg;
        char* build_path = malloc(strlen(device.controller) + 12);
        sprintf(build_path, "/org/bluez/%s", device.controller);

        msg = dbus_message_new_method_call("org.bluez", // target for the method call
                                           build_path, // object to call on
                                           "org.bluez.Adapter1", // interface to call on
                                           "StartDiscovery"); // method name

        if (!dbus_connection_send(device.conn, msg, NULL)) {
                fprintf(stderr, "Failed: Unable to start scan\n");
                exit(1);
        }
        else fprintf(stderr, "Start discovery ...\n");
        dbus_connection_flush(device.conn);
        dbus_message_unref(msg);
        free(build_path);
}

void stop_scan(BleDevice device) {
        DBusMessage* msg;
        char* build_path = malloc(strlen(device.controller) + 12);
        sprintf(build_path, "/org/bluez/%s", device.controller);

        msg = dbus_message_new_method_call("org.bluez", // target for the method call
                                           build_path, // object to call on
                                           "org.bluez.Adapter1", // interface to call on
                                           "StopDiscovery"); // method name

        if (!dbus_connection_send(device.conn, msg, NULL)) {
                fprintf(stderr, "Failed: Unable to stop scan\n");
                exit(1);
        }
        else fprintf(stderr, "Stop discovery\n");
        dbus_connection_flush(device.conn);
        dbus_message_unref(msg);
        free(build_path);
}

char* read_resp(DBusMessageIter args) {
        int type = dbus_message_iter_get_arg_type(&args);
        char* sret;
        if (type == DBUS_TYPE_STRING) {
                const char* resp;
                dbus_message_iter_get_basic (&args, &resp);

                sret = malloc(strlen(resp) + 1);
                strcpy(sret, resp);
        } else if (type == DBUS_TYPE_ARRAY) {
                type = dbus_message_iter_get_element_type(&args);
                if (type == DBUS_TYPE_BYTE) {
                        DBusMessageIter resp_iter;
                        char resp[BUFSIZ];
                        char tmp;
                        dbus_message_iter_recurse(&args, &resp_iter);
                        int i = 0;
                        do {
                                dbus_message_iter_get_basic(&resp_iter, &tmp);
                                resp[i++] = tmp;
                        } while(dbus_message_iter_next(&resp_iter));

                        sret = malloc(strlen(resp) + 1);
                        strcpy(sret, resp);
                } else {
                        fprintf(stderr, "Failed: Not handled\n");
                        exit(1);
                }
        } else if (type == DBUS_TYPE_VARIANT) {
                DBusMessageIter resp_iter;
                dbus_message_iter_recurse(&args, &resp_iter);
                int size = 0;
                do {
                        size++;
                } while(dbus_message_iter_next(&resp_iter));

                char* resp[size];

                dbus_message_iter_recurse(&args, &resp_iter);
                int malloc_size = 0;
                int i = 0;
                do {
                        type = dbus_message_iter_get_arg_type(&resp_iter);
                        if (type == DBUS_TYPE_BOOLEAN) {
                                bool tmp;
                                dbus_message_iter_get_basic(&resp_iter, &tmp);
                                resp[i] = tmp ? "true" : "false";
                                malloc_size += strlen(resp[i++]);
                        } else {
                                fprintf(stderr, "Failed: Not handled\n");
                                exit(1);
                        }
                } while(dbus_message_iter_next(&resp_iter));

                sret = malloc(malloc_size + 1);
                strcpy(sret, resp[0]);
                for (int i = 10; i < size; i++) {
                        strcat(sret, resp[i]);
                }
        } else {
                fprintf(stderr, "Failed: Not handled\n");
                exit(1);
        }
        return sret;
}

char* send_method(BleDevice device, char* path, char* interface, char* method, size_t param_nb, char* params[]) {
        DBusMessage* msg;
        DBusMessageIter args;
        DBusPendingCall* pending;
        char* build_path;
        bool free_path = false;

        if (path != NULL) {
                build_path = malloc(strlen(path) + 12);
                sprintf(build_path, "/org/bluez/%s", path);
                free_path = true;
        }
        else build_path = "/org/bluez";

        // fprintf(stderr, "-> method : org.bluez %s %s %s\n", build_path, interface, method);

        // create a new method call
        msg = dbus_message_new_method_call("org.bluez", build_path, interface, method);

        if (free_path) free(build_path);

        // append arguments
        dbus_message_iter_init_append(msg, &args);
        if (params != NULL) {
                char* param;
                char* type;
                char* value;
                for (size_t i = 0; i < param_nb; i++) {
                        if (i == 0) param = malloc(strlen(params[i]) + 1);
                        else param = realloc(param, strlen(params[i]) + 1);
                        strcpy(param, params[i]);
                        type = strtok(param, ":");
                        value = strtok(NULL, ":");

                        if (strcmp(type, "string") == 0) {
                                dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &value);
                        }
                        else if (strcmp(type, "array") == 0) {
                                DBusMessageIter entry;
                                if (strcmp(value, "byte") == 0) {
                                        value = strtok(NULL, ":");
                                        char* array_value = strtok(value, ",");
                                        int size;
                                        for (size=1; value[size]; value[size]==',' ? size++ : *value++) ;
                                        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &entry);
                                        char* array[size];
                                        int i = 0;
                                        while (array_value != NULL) {
                                                array[i++] = array_value;
                                                array_value = strtok(NULL, ",");
                                        }
                                        dbus_message_iter_append_fixed_array(&entry, DBUS_TYPE_BYTE, &array, size);
                                }
                                else{
                                        fprintf(stderr, "Failed: Not handled\n");
                                        exit(1);
                                }
                                dbus_message_iter_close_container(&args, &entry);
                        }
                        else{
                                fprintf(stderr, "Failed: Not handled\n");
                                exit(1);
                        }
                }
                free(param);
        }

        if (strcmp(device.version, "5.47") >= 0 && strstr(interface, "GattCharacteristic1")) {
                DBusMessageIter dict;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                                                 DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                                                 DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
                                                 DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);
                dbus_message_iter_close_container(&args, &dict);
        }

        // send message and get a handle for a reply
        dbus_connection_send_with_reply (device.conn, msg, &pending, -1); // -1 is default timeout
        dbus_connection_flush(device.conn);
        dbus_message_unref(msg);

        // get response
        dbus_pending_call_block(pending);
        msg = dbus_pending_call_steal_reply(pending);
        dbus_pending_call_unref(pending);

        // parse response
        char* sret;
        if (dbus_message_iter_init(msg, &args)) sret = read_resp(args);
        else sret = "";
        dbus_message_unref(msg);

        return sret;
}

bool scan_for(BleDevice device, int max_try) {
        // Start scan
        start_scan(device);

        // Search for device uuid
        int threshold = 0;
        char* result;
        bool skip = true;
        do {
                if (!skip) {
                        sleep(2);
                        result[0] = '\0';
                }
                else skip = false;
                result = send_method(device, device.controller, "org.freedesktop.DBus.Introspectable", "Introspect", 0, NULL);
                threshold++;
        } while(strstr(result, device.uuid) == NULL && threshold < max_try);

        // Strop scan
        stop_scan(device);

        if (threshold < max_try) return true;
        else return false;
}

bool check_connected(BleDevice device, char* path) {
        char* check_params[2] = {"string:org.bluez.Device1", "string:Connected"};
        char* result = send_method(device, path, "org.freedesktop.DBus.Properties", "Get", 2, check_params);
#ifdef DEBUG
        fprintf(stderr, "check_params : %s, %s\n", check_params[0], check_params[1]);
	fprintf(stderr, "check_connected - %s\n", path);
#endif
        if (result != NULL && strstr(result, "true")) return true;
        else return false;
}

bool connect_device(BleDevice device) {
        // Parse path
        char* path = malloc(strlen(device.controller) + strlen(device.uuid) + 2);
        sprintf(path, "%s/%s", device.controller, device.uuid);

        char* result = send_method(device, path, "org.bluez.Device1", "Connect", 0, NULL);
#ifdef DEBUG
	fprintf(stderr, "connect_device - %s\n", path);
#endif
        if (result == NULL) return false;
        return check_connected(device, path);
}

bool disconnect_device(BleDevice device) {
        // Parse path
        char* path = malloc(strlen(device.controller) + strlen(device.uuid) + 2);
        sprintf(path, "%s/%s", device.controller, device.uuid);

        char* result = send_method(device, path, "org.bluez.Device1", "Disconnect", 0, NULL);
#ifdef DEBUG
	fprintf(stderr, "disconnect_device - %s\n", path);
#endif
        if (result == NULL) return false;
        return !check_connected(device, path);
}

void authenticate_device(BleDevice device, char* pincode) {
        // Parse path
        char* path = malloc(strlen(device.controller) + strlen(device.uuid) + 23);
        sprintf(path, "%s/%s/service0022/char0023", device.controller, device.uuid);

        char pin_param[16];
	snprintf(pin_param, 16, "array:byte:%s", pincode);
        char* write_params[1] = {pin_param};

        send_method(device, path, "org.bluez.GattCharacteristic1", "WriteValue", 1, write_params);
#ifdef DEBUG
        fprintf(stderr, "write_params : %s\n", write_params[0]);
	fprintf(stderr, "authenticate_device - %s\n", path);
#endif
        free(path);
}

int read_battery(BleDevice device) {
        // Parse path
        char* path = malloc(strlen(device.controller) + strlen(device.uuid) + 23);
        sprintf(path, "%s/%s/service000e/char000f", device.controller, device.uuid);

        char* result = send_method(device, path, "org.bluez.GattCharacteristic1", "ReadValue", 0, NULL);
#ifdef DEBUG
	fprintf(stderr, "read_battery - %s\n", path);
#endif
        free(path);
        if (result == NULL || strlen(result) > 1) return -1;
        return result[0];
}

bool search_controller(BleDevice device) {
        char* result = send_method(device, NULL, "org.freedesktop.DBus.Introspectable", "Introspect", 0, NULL);
        if (strstr(result, device.controller) != NULL) return true;
        else return false;
}

int main(int argc, char *argv[])
{
        if (argc != 4) {
                fprintf(stderr, "Failed: No MAC address and/or no interface and/or no pincode !\n");
                fprintf(stderr, "Usage: %s <interface> <mac_address> <pin_code>\n", argv[0]);
                fprintf(stderr, "Exemple: %s hci0 00:11:22:33:44 1234\n", argv[0]);
		return 1;
        }

        char* interface = argv[1];
        char* device_mac = argv[2];
        char* pin_code = argv[3];

        fprintf(stderr, "Test BLE (Bluetooth Low Energy) on %s with Danfoss ECO2 (radiator valve)\n", interface);

        // Check mac validity
        regex_t preg;
        regcomp (&preg, "(([0-9]|[A-F]){2}:){5}([0-9]|[A-F]){2}", REG_NOSUB | REG_EXTENDED);
        int match = regexec(&preg, device_mac, 0, NULL, 0);
        if (strlen(argv[2]) != 17 || match != 0) {
                fprintf(stderr, "Failed: MAC address '%s' not valid !\n", device_mac);
                return 1;
        }

        char* parse_mac = malloc(strlen(device_mac) + 5);
        sprintf(parse_mac, "dev_%s", device_mac);

        for(size_t i = 0; i <= strlen(parse_mac); i++)
                if(parse_mac[i] == ':') parse_mac[i] = '_';

        FILE* cmd = popen("bluetoothd -v", "r");
        if (cmd == NULL) return 1;
        char* version;
        size_t len = 0;
        getline(&version, &len, cmd);

        BleDevice device = {connect_bus(), parse_mac, interface, version};

        // Check if controller exist
        if (!search_controller(device)) {
                fprintf(stderr, "Failed: interface %s not found\n", interface);
                return 1;
        }

        fprintf(stderr, "Searching for device %s ... \n", device_mac);
        bool scan = scan_for(device, 5);
        fprintf(stderr, "Device found : %s\n", scan ? "Success" : "Failed");

        if (scan) {
                bool state;
                int threshold = 0;
                fprintf(stderr, "Connection ... \n");
                do state = connect_device(device);
                while(!state && ++threshold < 2);
                fprintf(stderr, "Connection : %s\n", state ? "Success" : "Failed");

                if (state) {
                        authenticate_device(device, pin_code);

                        int battery_value = read_battery(device);
                        if (battery_value < 0) {
                                fprintf(stderr, "Failed: Error while reading battery_value\n");
                                disconnect_device(device);
                                return 1;
                        }
                        else fprintf(stderr, "Battery level: %d%c\n", battery_value, '%');

                        fprintf(stderr, "Disconnection ...\n");
                        state = disconnect_device(device);
                        fprintf(stderr, "Disconnection : %s\n", state ? "Success" : "Failed");
                        return 0;
                }
        }

        return 1;
}
