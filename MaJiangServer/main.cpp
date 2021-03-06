#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <cstdint>
#include <thread>
#include <vector>
#include <algorithm>
#include <iostream>
#include "MaJiangType.h"
#include "MaJiangCommand.h"
#include <poll.h>

/**
 *          消息的基本单位是int型
 *          命令是相对于客户端的
 * 帧格式:
 *          [消息的长度(表示帧有几个int,包括自身)|命令|数据]
 * */

#define SERV_PORT 8001

#define FRAME_LEN_POSITION 0
#define FRAME_CMD_POSITION 1
#define FRAME_DATA_POSITION 2

using namespace std;

void do_work(int *cfd) {
    vector<MaJiangType> majiangs;
    int consumeMaJiangPosition = 0;
    int turn = 0;

    //初始化麻将
    for (int i = WAN_1; i <= HONGZHONG; ++i) {
        for (int j = 0; j < 4; ++j) {
            majiangs.push_back(MaJiangType(i));
        }
    }
    long seed = std::time(nullptr);
    std::shuffle(majiangs.begin(), majiangs.end(), std::default_random_engine(seed));

    int host_fd = cfd[0];
    int oppo_fd = cfd[1];

    int mjs[512];
    int i = 0;
    for (auto &mj:majiangs) {
        mjs[i + 2] = (int) mj;
        i++;
    }
    mjs[0] = i + 2;
    mjs[1] = FAPAI;

    //发牌
    write(host_fd, mjs, sizeof(int) * (mjs[0]));
    write(oppo_fd, mjs, sizeof(int) * (mjs[0]));

    mjs[0] = 3;
    mjs[1] = ISHOST;
    mjs[2] = 1;
    write(host_fd, mjs, sizeof(int) * (mjs[0]));
    mjs[2] = 0;
    write(oppo_fd, mjs, sizeof(int) * (mjs[0]));

    struct pollfd fds[2];
    fds[0].fd = host_fd;
    fds[0].events = POLLRDNORM;

    fds[1].fd = oppo_fd;
    fds[1].events = POLLRDNORM;


    for (;;) {
        if (poll(fds, 2, -1) <= 0) {
            continue;
        }

        if (fds[0].revents & POLLRDNORM) {
            if (read(host_fd, mjs, sizeof(int)) <= 0) break;
            read(host_fd, &mjs[1], (mjs[0] - 1) * sizeof(int));
            write(oppo_fd, mjs, sizeof(int) * mjs[0]);
        }

        if (fds[1].revents & POLLRDNORM) {
            if (read(oppo_fd, mjs, sizeof(int)) <= 0) break;
            read(oppo_fd, &mjs[1], (mjs[0] - 1) * sizeof(int));
            write(host_fd, mjs, sizeof(int) * mjs[0]);
        }
    }
}


int main(int argc, char *argv[]) {
    int serverfd;
    int serv_port = SERV_PORT;
    char client_ip[128];
    struct sockaddr_in serv_addr{};
    socklen_t client_len;

    if (argc == 2)
        serv_port = atoi(argv[1]);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(static_cast<uint16_t>(serv_port));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(serverfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    listen(serverfd, 128);

    printf("wait for connect...\n");

    while (true) {
        struct sockaddr_in client_addrs[2];
        int clientfds[2];
        for (int i = 0; i < 2; i++) {
            client_len = sizeof(client_addrs[i]);
            clientfds[i] = accept(serverfd, (struct sockaddr *) &client_addrs[i], &client_len);
            printf("client:%s\t%d\n",
                   inet_ntop(AF_INET, &client_addrs[i].sin_addr.s_addr, client_ip, sizeof(client_ip)),
                   ntohs(client_addrs[i].sin_port));
        }
        std::thread t(do_work, clientfds);
        t.detach();
    }

    return 0;
}
