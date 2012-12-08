/*
 * dvbls
 * Copyright (C) 2012, Andrey Dyldin <and@cesbo.com>
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/dvb/net.h>
#include <errno.h>

static int adapter = 0;
static int device = 0;

static void iterate_dir(const char *dir, const char *filter, void (*callback)(const char *))
{
    DIR *dirp = opendir(dir);
    if(!dirp)
    {
        printf("ERROR: opendir() failed %s [%s]\n", dir, strerror(errno));
        return;
    }

    char item[64];
    const int item_len = sprintf(item, "%s/", dir);
    const int filter_len = strlen(filter);
    do
    {
        struct dirent *entry = readdir(dirp);
        if(!entry)
            break;
        if(strncmp(entry->d_name, filter, filter_len))
            continue;
        sprintf(&item[item_len], "%s", entry->d_name);
        callback(item);
    } while(1);

    closedir(dirp);
}

static int get_last_int(const char *str)
{
    int i = 0;
    int i_pos = -1;
    for(; str[i]; ++i)
    {
        const char c = str[i];
        if(c >= '0' && c <= '9')
        {
            if(i_pos == -1)
                i_pos = i;
        }
        else if(i_pos >= 0)
            i_pos = -1;
    }

    if(i_pos == -1)
        return 0;

    return atoi(&str[i_pos]);
}

static void check_net_device(const char *item)
{
    device = get_last_int(&item[(sizeof("/dev/dvb/adapter") - 1) + (sizeof("/net") - 1)]);

    int ret = 0;

    int fd = open(item, O_RDWR | O_NONBLOCK);
    if(!fd)
    {
        printf("ERROR: open() failed %s [%s]\n", item, strerror(errno));
        return;
    }

    struct dvb_net_if net;
    net.pid = 0;
    if(ioctl(fd, NET_ADD_IF, &net) < 0)
    {
        printf("ERROR: NET_ADD_IF failed %s [%s]\n", item, strerror(errno));
        close(fd);
        return;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    sprintf(ifr.ifr_name, "dvb%d_%d", adapter, device);

    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
        printf("ERROR: SIOCGIFHWADDR failed %s [%s]\n", item, strerror(errno));
    else
    {
        const unsigned char *mac = ifr.ifr_hwaddr.sa_data;
        printf("adapter:%d device:%d mac:%02X:%02X:%02X:%02X:%02X:%02X\n"
               , adapter, device
               , mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    close(sock);

    if(ioctl(fd, NET_REMOVE_IF, net.if_num) < 0)
        printf("ERROR: NET_REMOVE_IF failed %s [%s]\n", item, strerror(errno));

    close(fd);
}

static void check_adapter(const char *item)
{
    adapter = get_last_int(&item[sizeof("/dev/dvb/adapter") - 1]);
    iterate_dir(item, "net", check_net_device);
}

int main(int argc, char const *argv[])
{
    printf("dvbls v.1\n\n");
    iterate_dir("/dev/dvb", "adapter", check_adapter);
    return 0;
}
