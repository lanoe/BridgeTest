// sudo usermod -aG i2c $USER

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <TO.h>

void print_uint_array(const uint8_t *arr, uint16_t size, uint8_t printHexa)
{
    int i = 0;
    const char* format = printHexa ? "%02X" : "%d";
    for (; i < size; i++)
    {
        if (i != 0) fprintf(stderr, " ");
        fprintf(stderr, (const char*)format, arr[i]);
    }
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
    int ret = -99;
    uint8_t serial_number[TO_SN_SIZE];
    uint8_t product_number[TO_PN_SIZE];
    uint8_t hardware_version[TO_HW_VERSION_SIZE];
    uint8_t major, minor, revision;

    if (TO_init() != TO_OK)
    {
        fprintf(stderr, "Failed to initialize TO\n");
        ret = -1;
        goto err;
    }
    fprintf(stderr, "Secure Element initialized\n");

    if (TO_get_serial_number(serial_number) != TORSP_SUCCESS)
    {
        fprintf(stderr, "Failed to get Secure Element serial number\n");
        ret = -2;
        goto err;
    }
    fprintf(stderr, "SERIAL NUMBER : ");
    print_uint_array(serial_number, TO_SN_SIZE, 1);

    if (TO_get_product_number(product_number) != TORSP_SUCCESS)
    {
        fprintf(stderr, "Failed to get Secure Element product number\n");
        ret = -3;
        goto err;
    }
    fprintf(stderr, "PRODUCT NUMBER : ");
    print_uint_array(product_number, TO_PN_SIZE, 1);

    if (TO_get_hardware_version(hardware_version) != TORSP_SUCCESS)
    {
        fprintf(stderr, "Failed to get Secure Element hardware version\n");
        ret = -4;
        goto err;
    }
    fprintf(stderr, "HARDWARE VERSION : ");
    print_uint_array(hardware_version, TO_HW_VERSION_SIZE, 1);

    if (TO_get_software_version(&major, &minor, &revision) != TORSP_SUCCESS)
    {
        fprintf(stderr, "Failed to get Secure Element software version\n");
        ret = -5;
        goto err;
    }
    fprintf(stderr, "SOFTWARE VERSION : v%d.%d.%d\n", major, minor, revision);

    ret = 0;
err:
    TO_fini();
    return ret;
}
