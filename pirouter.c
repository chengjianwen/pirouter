/*
  该程序连接所有在/var/lib/piserver/hosts.json中定义的主机，接收它们的消息，并
  将这些消息统一发送至服务端口，相当于一个汇聚功能。
  当然，它也接收来自于服务端口的信息，并根据消息中的mac地址，发送至对一个的主机。
                          主机-1
        服务端口 <----->  主机-2
                          主机-3
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <json.h>

#define PISERVER_DATADIR  "/var/lib/piserver"
#define PISERVER_HOSTSFILE  PISERVER_DATADIR "/hosts.json"
#define	PORT              7788

int main(int argc, char *argv[])
{
  struct nn_pollfd *fds;
  char url[NN_SOCKADDR_MAX];

  fds = (struct nn_pollfd *)malloc (sizeof (struct nn_pollfd));
  fds[0].fd = nn_socket (AF_SP, NN_PAIR);
  fds[0].events = NN_POLLIN;

  sprintf (url, "tcp://%s:7788", "*");
  nn_bind (fds[0].fd, url);
  printf ("fds][0] was bind to %s\n", url);

  FILE *fp = fopen (PISERVER_HOSTSFILE, "r");
  if (!fp)
  {
    fprintf (stderr, "can't open file: %s\n", PISERVER_HOSTSFILE);
    nn_close (fds[0].fd);
    free (fds);
    return -1;
  }

  fseek(fp, 0, SEEK_END);
  int size = ftell(fp) + 1;
  char *buf = malloc (size);
  rewind(fp);
  fread (buf, 1, size, fp);
  buf[size - 1] = '\0';
  fclose (fp);

  struct json_object *hosts;
  hosts = json_tokener_parse(buf);
  free(buf);
  if (!json_object_is_type(hosts, json_type_array))
  {
    fprintf (stderr, "invalid json file.\n");
    return -1;
  }

  fds = (struct nn_pollfd *)realloc (fds, sizeof (struct nn_pollfd) * (json_object_array_length(hosts) + 1));
  for (int i = 0; i < json_object_array_length(hosts); i++)
  {
    struct json_object *tmp = json_object_array_get_idx(hosts, i);
    fds[i + 1].fd = nn_socket (AF_SP, NN_PAIR);
    fds[i + 1].events = NN_POLLIN;
    struct json_object *tmp_2;
    json_object_object_get_ex(tmp, "name", &tmp_2);
    sprintf (url, "tcp://%s:7788", json_object_get_string(tmp_2));
    nn_connect (fds[i + 1].fd, url);
    printf ("connect to %s\n", url);
  }

  while (1)
  {
    nn_poll (fds, json_object_array_length(hosts) + 1, -1);
    for (int i = 0; i < json_object_array_length(hosts) + 1; i++)
    {
      if (fds [i].revents & NN_POLLIN)
      {
        void *msg = NULL;
        nn_recv (fds[i].fd, &msg, NN_MSG, 0);
        if (i)
          nn_send (fds[0].fd, &msg, NN_MSG, NN_DONTWAIT);
        else
        {
          char *mac = strstr(msg, " ") + 1;
          for (int j = 0; j < json_object_array_length(hosts); j++)
          {
            struct json_object *tmp = json_object_array_get_idx(hosts, j);
            struct json_object *tmp_2;
            json_object_object_get_ex(tmp, "mac", &tmp_2);
            if (strncmp(mac, json_object_get_string(tmp_2), 17)== 0)
            {
              nn_send (fds[j + 1].fd, &msg, NN_MSG, NN_DONTWAIT);
              break;
            }
          }
        }
      }
    }
  }
}
