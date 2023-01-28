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
#define ACK_SIZE 3

char* in1data[] = { "a", NULL };
char* in17data[] = { "hello hello hello", NULL };
char* in34data[] = { "hello\ngoodbye\ntest 34 char payload", NULL };
char* in35data[] = { "hello\ngoodbye\ntest 35 char payload\n", NULL };
char* in36data[] = { "hello goodbye test 36 char payload!", "!", NULL};
char* in350data[] = {
    "n this module you will gain an unde",
    "rstanding of operating system and n",
    "etwork design and implementation. A",
    "s outlined above, this is an import",
    "ant topic in itself.\nIt is also imp",
    "ortant because of what it teaches u",
    "s about good systems and applicatio",
    "ns design in general\n\tMany of the a",
    "spects of computer science - abstra",
    "ction, algorithm design and choice ",
    NULL
};
char* in660data[] = {
    "We\nall\nuse operating systems and ne",
    "tworks every day, when we use a mob",
    "ile phone, tablet, laptop, or any n",
    "umber of other computing devices. A",
    "n ordinary user may not\nbe\ninterest",
    "ed in how the operating sy\nstem of ",
    "a device they use works and may not",
    " even know the operating system exi",
    "sts.\n\n\tAs computer scientists we bo",
    "th should be interested in how they",
    " work and need to know how they wor",
    "k.\n\nFor example, it is not possible",
    " to detect and protect against cybe",
    "r attacks without an understanding ",
    "of operating systems and networks.\n",
    "\nIf you want to resolve performance",
    " or reliability problems, you need ",
    "an understanding of the role of ope",
    "rating systems and networks.\n\n",
    NULL
}; 

typedef struct data {
    size_t size;
    char** data;
} data_t;

data_t data_setup[] = {
    { 1, in1data },
    { 17, in17data },
    { 34, in34data },
    { 35, in35data },
    { 36, in36data },
    { 350, in350data },
    { 660, in660data },
};

typedef enum data_idx {
    DATA_SZ_1,
    DATA_SZ_17,
    DATA_SZ_34,
    DATA_SZ_35,
    DATA_SZ_36,
    DATA_SZ_350,
    DATA_SZ_660
} data_idx_t;


static void proto_tear_down(void* fixture);
static void* proto_setup(const MunitParameter params[], void* user_data);

static MunitResult test_send_data_1(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_17(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_34(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_35(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_36(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_350(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_660(const MunitParameter params[], 
    void* fixture);

static MunitResult test_send_data_exretry(const MunitParameter params[], 
    void* fixture);
static MunitResult test_send_data_err(const MunitParameter params[], 
    void* fixture);

static MunitTest tests[] = {
    { "/test_send_data_1", test_send_data_1, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_17", test_send_data_17, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_34", test_send_data_34, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_35", test_send_data_35, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_36", test_send_data_36, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_350", test_send_data_350, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_660", test_send_data_660, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },

    { "/test_send_data_exretry", test_send_data_exretry, NULL, NULL, 
        MUNIT_TEST_OPTION_NONE, NULL },
    { "/test_send_data_err", test_send_data_err, proto_setup, proto_tear_down, 
        MUNIT_TEST_OPTION_NONE, NULL },

    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/test_rftcu_send_data", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE 
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

static void test_error(protocol_t* proto, int srvr_sockfd, pid_t other_pid, 
    char* role, char* err_msg, int line) {
    if (proto->sockfd != -1)
        close(proto->sockfd);
    if (srvr_sockfd != -1)
        close(srvr_sockfd);
    if (other_pid > 0)
        kill(other_pid, SIGKILL);
    free(proto);
    munit_errorf("%s error: %s at line: %d", role, err_msg, line);
    exit(EXIT_FAILURE);
}

static void* proto_setup(const MunitParameter params[], void* user_data) {
    uint16_t* srvr_port = (uint16_t*) user_data;

    munit_logf(MUNIT_LOG_DEBUG, "\nUsing port: %d", *srvr_port);

    protocol_t* proto = (protocol_t*) malloc(sizeof(protocol_t));

    memset(proto, 0, sizeof(protocol_t));
    proto->server_port = *srvr_port;
    proto->tfr_mode = "nm";
    proto->sockaddr_size = (socklen_t) sizeof(struct sockaddr_in);
    proto->seg_size = sizeof(segment_t);
    proto->curr_retry = 0;
    proto->max_retries = DEFAULT_RETRIES;

    proto->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (proto->sockfd == -1) {
        test_error(proto, -1, -1, "setup", "socket failed", __LINE__);
    }
            
    proto->server.sin_family = AF_INET;
    proto->server.sin_port = htons(proto->server_port);
    if(!inet_aton("127.0.0.1", &proto->server.sin_addr)) {
        test_error(proto, -1, -1, "setup", "socket failed", __LINE__);
    }
        
    proto->state = PS_SEND_DATA;

    return proto;
}

static void proto_tear_down(void* fixture) {
    protocol_t* proto = (protocol_t*) fixture;
    
    if (proto->sockfd != -1)
        close(proto->sockfd);
        
    free(proto);
}

static proto_state run_client(protocol_t* proto, pid_t spid, data_idx_t d_idx) {
    sleep(1);
    char ack[ACK_SIZE];
    
    int i = 0;
    size_t data_sent = 0;
    proto_state ps = PS_INIT;
    
    while (data_setup[d_idx].data[i]) {
        proto->data.sq = i;
        proto->data.file_data = strnlen(data_setup[d_idx].data[i], 
            PAYLOAD_SIZE + 1);
        memset(&proto->data.payload, 0, PAYLOAD_SIZE);
        snprintf(proto->data.payload, PAYLOAD_SIZE, "%s",
            data_setup[d_idx].data[i]);
                
        ps = send_data(proto);
        
        if (ps != PS_DATA_SENT)
            break;
            
        socklen_t server_size = proto->sockaddr_size;
        struct sockaddr_in server;
        memset(&server, 0, server_size);
        memset(ack, 0, ACK_SIZE);
        ssize_t nbytes = recvfrom(proto->sockfd, ack, ACK_SIZE, 0, 
            (struct sockaddr *) &server, &server_size);
        
        if (nbytes != ACK_SIZE) {
            test_error(proto, -1, spid, "client", "recvfrom nbytes != ACK_SIZE", 
                __LINE__);
        }
        
        if (strncmp(ack, "OK", ACK_SIZE + 1)) {
            test_error(proto, -1, spid, "client", 
                "recvfrom got server ack != \"OK\"", __LINE__);
        }
        
        data_sent += proto->data.file_data;
        i++;
    }
    
    if (data_sent != data_setup[d_idx].size)
        test_error(proto, -1, spid, 
            "client", "data_sent != data_setup[d_idx].size", __LINE__);

    return ps;
}

static int bind_server(protocol_t* proto) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("***********\nIMPORTANT: setting sever socket failed\n"
            "this is NOT a student error, try starting the test again\n"
            "***********\n");
        exit(EXIT_FAILURE);
    }
        
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(proto->server_port);

    if (bind(sockfd, (struct sockaddr *) &server, sizeof(struct sockaddr_in))) {
        close(sockfd);
        printf("***********\nIMPORTANT: bind failed\n"
            "this is NOT a student error, try starting the test with a "
            "different port instead of: %d\n***********\n", proto->server_port);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

static void run_server(protocol_t* proto, int sockfd, pid_t cpid, 
    data_idx_t d_idx) {
    size_t expected_data = data_setup[d_idx].size;
    int i = 0;
    size_t data_received = 0;
    
    while (expected_data) {
        struct sockaddr_in client;
        socklen_t sock_len = (socklen_t) sizeof(struct sockaddr_in);
        memset(&client, 0, sock_len);
        segment_t data_seg;
        size_t seg_size = sizeof(segment_t);
        memset(&data_seg, 0, seg_size);

        ssize_t bytes = recvfrom(sockfd, &data_seg, seg_size, 0, 
                    (struct sockaddr*) &client, &sock_len);
    
        if (bytes != seg_size)
            test_error(proto, sockfd, cpid, "server", "bytes != seg_size",
                __LINE__);
        
        if (data_seg.payload[PAYLOAD_SIZE-1] != 0) 
             test_error(proto, sockfd, cpid, "server", "payload not terminate",
                __LINE__);
           
        
        size_t payload_len = strnlen(data_seg.payload, PAYLOAD_SIZE + 1);
        size_t exp_data_len = expected_data < PAYLOAD_SIZE 
                                ? expected_data : PAYLOAD_SIZE - 1;
                                
        if (payload_len != exp_data_len)
             test_error(proto, sockfd, cpid, "server", "invalid payload length",
                __LINE__);
        if (data_seg.file_data != exp_data_len)
             test_error(proto, sockfd, cpid, "server",  
                "invalid data_seg.file_data", __LINE__);
        if (strncmp(data_seg.payload, data_setup[d_idx].data[i], 
            PAYLOAD_SIZE + 1))
             test_error(proto, sockfd, cpid, "server", "invalid payload",
                __LINE__);
        if (data_seg.sq != i)
             test_error(proto, sockfd, cpid, "server", "invalid data_seg.sq",
                __LINE__);

        int cs = checksum(data_seg.payload, false);
        if (data_seg.checksum != cs)
             test_error(proto, sockfd, cpid, "server",
                "invalid data_seg.checksum", __LINE__);
            
        munit_logf(MUNIT_LOG_DEBUG,
            "server received checksum: %d, sq: %d, bytes: %d", 
            cs, data_seg.sq, (int) data_seg.file_data);
        munit_logf(MUNIT_LOG_DEBUG, "server payload received: %s",
            data_seg.payload);

        bytes = sendto(sockfd, "OK", ACK_SIZE, 0, (struct sockaddr*) &client,
            sock_len);
        
        expected_data -= data_seg.file_data;
        data_received += data_seg.file_data;
        
        i++;
    }

    close(sockfd);

    if (data_received != data_setup[d_idx].size)
        test_error(proto, -1, cpid, "server", 
            "data_received != data_setup[d_idx].size", __LINE__);
                
    return;
}

static MunitResult test_send_data(protocol_t* proto, data_idx_t d_idx) {
    int sockfd = bind_server(proto);
    pid_t spid = getpid();
    pid_t cpid = fork();
    
    if (cpid < -1) 
        return MUNIT_FAIL;

    if (cpid == 0) {
        exit(run_client(proto, spid, d_idx));
    } else {
        int cstatus;
        run_server(proto, sockfd, cpid, d_idx);
        
        waitpid(cpid, &cstatus, 0);
        
        if (WIFEXITED(cstatus))
            assert_int(WEXITSTATUS(cstatus), ==, PS_DATA_SENT);
    }        

    return MUNIT_OK;
}

static MunitResult test_send_data_1(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_1);
}

static MunitResult test_send_data_17(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_17);
}

static MunitResult test_send_data_34(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_34);
}

static MunitResult test_send_data_35(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_35);
}

static MunitResult test_send_data_36(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_36);
}

static MunitResult test_send_data_350(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_350);
}

static MunitResult test_send_data_660(const MunitParameter params[], 
    void* fixture) {   
    return test_send_data((protocol_t*) fixture, DATA_SZ_660);
}

static MunitResult test_send_data_exretry(const MunitParameter params[], 
    void* fixture) {
    protocol_t proto;
    proto.max_retries = DEFAULT_RETRIES;
    proto.curr_retry = DEFAULT_RETRIES + 1;
    
    assert_int(send_data(&proto), ==, PS_EXCEED_RETRY);
    
    return MUNIT_OK;
}
    
static MunitResult test_send_data_err(const MunitParameter params[], 
    void* fixture) {
    protocol_t* proto = (protocol_t*) fixture;
    close(proto->sockfd);
    
    assert_int(send_data(proto), ==, PS_BAD_SEND);

    return MUNIT_OK;
}
    