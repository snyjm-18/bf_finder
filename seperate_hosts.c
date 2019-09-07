/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 *
 * Sample MPI "hello world" application in C
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/poll.h>

#include <netdb.h>
#include <netinet/in.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

#define PATH_MAX 1024
#define PORT 10000

struct nic_info {
    int is_nic;
    int partner;
};
typedef struct nic_info nic_info_t;

int *nic_lookup;

nic_info_t  my_partner(int rank) {
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int new_fd;
    struct sockaddr_in servaddr;
    struct pollfd fd[1];
    MPI_Status status;
    nic_info_t info;

    FILE *fp;
    char path[PATH_MAX];
    char *nic_string;

    fp = popen("/usr/sbin/lspci", "r");
    if (fp == NULL)
        /* Handle error */;

    info.is_nic = 0;
    while (fgets(path, PATH_MAX, fp) != NULL) {
        nic_string = strstr(path, "BlueField SoC PCIe Bridge");
        if (nic_string) {
            info.is_nic = 1;
        }
    }

    bzero(&servaddr, sizeof(servaddr));

    socklen_t socket_size = sizeof(struct sockaddr_in);

    // assign IP, PORT 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("192.168.100.1");
    servaddr.sin_port = htons(PORT);

    if (info.is_nic) {
        while (connect(sockfd,(struct sockaddr *) &servaddr,socket_size) < 0){
        }
        write(sockfd, &rank, sizeof(int));
        info.is_nic = 1;
        MPI_Recv(&(info.partner), 1, MPI_INT, MPI_ANY_SOURCE, 123, MPI_COMM_WORLD, &status);
        close(sockfd);
        return info;
    }

    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        fprintf(stderr, "error \n");
        exit(0);
    }
    if (listen(sockfd, 1) == -1) {
        fprintf(stderr, "Error with bind %s \n", strerror(errno));
    }
    info.is_nic = 0;
    new_fd = accept(sockfd, (struct sockaddr*) &servaddr, &socket_size);
    fd[0].fd = new_fd;
    fd[0].events = POLLIN;
    poll(fd, 1, -1);
    read(new_fd, &(info.partner), sizeof(int));
    MPI_Send(&rank, 1, MPI_INT, info.partner, 123, MPI_COMM_WORLD);
    close(new_fd);
    close(sockfd);
    return info;
}

int main(int argc, char* argv[]) {
    int rank, size, len;
    int recv_rank;
    MPI_Status status;
    int is_nic;
    int my_bf;
    char *bf;
    char nid[3];
    int nic_rank, i;
    MPI_Comm HvN;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    nic_info_t my_info;
    nic_lookup = malloc(sizeof(int) * size);
    
    my_info = my_partner(rank);
    
    if (my_info.is_nic) {
        i = -1;
        MPI_Allgather(&i, 1, MPI_INT, nic_lookup, 1, MPI_INT, MPI_COMM_WORLD);
    } else {
        MPI_Allgather(&my_info.partner, 1, MPI_INT, nic_lookup, 1, MPI_INT, MPI_COMM_WORLD);
    }
    
    MPI_Comm nic_comm;
    MPI_Comm_split(MPI_COMM_WORLD, my_info.is_nic, rank, &nic_comm);
    MPI_Comm_rank(nic_comm, &nic_rank);
    
    fprintf(stderr, "I am %d globally my partner is %d and I am a nic %d ? \n", 
            rank, my_info.partner, my_info.is_nic);

    MPI_Finalize();
    return 0;

}
