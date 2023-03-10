/******** DO NOT EDIT THIS FILE ********/
#define MUNIT_ENABLE_ASSERT_ALIASES
#include <stdio.h>
#include <math.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "munit.h"
#include "rft_client_util.h"

#define DEFAULT_PORT 20333

static MunitResult test_send_metadata(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_metadata_err(const MunitParameter params[], 
    void* fixture);

static MunitTest tests[] = {
    { "/test_send_metadata", test_send_metadata, NULL, NULL, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_metadata_err", test_send_metadata_err, NULL, NULL, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/test_rftcu_send_metadata", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE 
};    

int main(int argc, char** argv) {
    uint16_t srvr_port = DEFAULT_PORT;
    if (argc == 2) {
        srvr_port = atoi(argv[1]);
        argv[1] = NULL;
        argc = 1;
    }
    
    return munit_suite_main(&suite, &srvr_port, argc, argv);
}

void test_client_err(int sockfd, pid_t ppid, char* err_msg, int line) {
    if (sockfd != -1)
        close(sockfd);
    kill(ppid, SIGKILL);
    munit_errorf("client error: %s at line: %d", err_msg, line);
    exit(EXIT_FAILURE);
}

static MunitResult test_send_metadata(const MunitParameter params[], 
    void* fixture) {
    uint16_t* srvr_port = (uint16_t*) fixture;
    munit_logf(MUNIT_LOG_INFO, "Using port: %d", *srvr_port);
    
    pid_t ppid = getpid();
    pid_t pid = fork();
    
    assert_int(pid, !=, -1);
        
    if (pid == 0) {
        proto_state ps;
        sleep(1);

        protocol_t proto;
        memset(&proto, 0, sizeof(protocol_t));
        proto.sockaddr_size = (socklen_t) sizeof(struct sockaddr_in); 
        proto.fsize = 64;
        snprintf(proto.out_fname, MAX_FILENAME_SIZE, "%s", "test_file.txt");
        
        proto.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (proto.sockfd == -1) {
            test_client_err(proto.sockfd, ppid, "socket failed", __LINE__ - 2);
        }
            
        proto.server.sin_family = AF_INET;
        proto.server.sin_port = htons(*srvr_port);
        if(!inet_aton("127.0.0.1", &proto.server.sin_addr)) {
            test_client_err(proto.sockfd, ppid, "inet_aton failed", 
                __LINE__ - 2);
        }
        
        ps = send_metadata(&proto);
        
        close(proto.sockfd);

        exit(ps);
    } else {
        // in parent
        int cstatus;
        
        metadata_t file_inf;
        size_t metadata_size = sizeof(metadata_t);
        memset(&file_inf, 0, sizeof(metadata_t));
        socklen_t sock_len = (socklen_t) sizeof(struct sockaddr_in); ;
        ssize_t bytes = -1;

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        assert_int(sockfd, !=, -1);
        struct sockaddr_in server;

        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(*srvr_port);
 
        if (bind(sockfd, (struct sockaddr *) &server, sock_len)) {
            printf("***********\nIMPORTANT: bind failed\n"
                "this is NOT a student error, try starting the test with a "
                "different port instead of: %d\n***********\n", *srvr_port); 
            close(sockfd);
            kill(pid, SIGKILL);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in client;
        memset(&client, 0, sock_len);
    
        bytes = recvfrom(sockfd, &file_inf, metadata_size, 0, 
                        (struct sockaddr*) &client, &sock_len);
        
        assert_size(bytes, ==, metadata_size);
        assert_string_equal(file_inf.name, "test_file.txt");
        assert_size(file_inf.size, ==, 64);

        close(sockfd);

        waitpid(pid, &cstatus, 0);
        if (WIFEXITED(cstatus))
            assert_int(WEXITSTATUS(cstatus), ==, PS_META_SENT);
        
        munit_log(MUNIT_LOG_INFO, "Metadata received successfully");
    }

    return MUNIT_OK;
}

static MunitResult test_send_metadata_err(const MunitParameter params[], 
    void* fixture) {
    uint16_t* srvr_port = (uint16_t*) fixture;
    protocol_t proto;

    memset(&proto, 0, sizeof(protocol_t));
    proto.sockaddr_size = (socklen_t) sizeof(struct sockaddr_in); 
    proto.fsize = 64;
    snprintf(proto.out_fname, MAX_FILENAME_SIZE, "%s", "test_file.txt");
    
    proto.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
         
    proto.server.sin_family = AF_INET;
    proto.server.sin_port = htons(*srvr_port);
    inet_aton("127.0.0.1", &proto.server.sin_addr);
    
    close(proto.sockfd);
    
    assert_int(send_metadata(&proto), ==, PS_BAD_META);

    return MUNIT_OK;
}

