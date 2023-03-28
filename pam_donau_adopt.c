/* pam_donau_adopt module */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <linux/limits.h>
#include <security/pam_ext.h>
#include <security/pam_modules.h>

// macro definitions
#define PRINT_BUFFER_LEN 1024
#define MAX_MSG_LEN 4096
#define MSG_HEADER_LEN 12
#define CMD_MAX_LEN 1024
#define CMD_TIMEOUT_SEC 6
#define BASH_PATH "/bin/bash"

// constant definitions
const char *donau_agent_version = "1.3.0";
const char *g_rootUser = "root";
const char *g_argsLogLevel = "log_level=";
const char *g_argsAgentSocket = "donau_agent_socket=";

// log level
const char *g_logLevelDebug = "debug";
// system command
const char *g_lsofPath = "/usr/sbin/lsof";
const char *g_sys_lsofPath = "/usr/bin/lsof";

int GetCodeFromMessage(const char *message, size_t msgLen)
{
    char *msgStr = strchr(message, ':');
    if (msgStr == NULL) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]incorrect message content");
        return -1;
    }

    int code = -1;
    char value[MAX_MSG_LEN] = {0};
    int ret = sscanf(message, "%d:%s", &code, value);
    if (ret < 1) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]incorrect message content");
        return -1;
    }

    if (code != 0 && strlen(value) > 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]%s", message);
    }
    return code;
}

int SendRecvMessage(const char *message, const char *serverSockPath)
{
    if (message == NULL || serverSockPath == NULL) {
        return -1;
    }

    int ret = -1;
    struct sockaddr_un server;
    (void)memset(&server, 0, sizeof(server));
    server.sun_family = AF_UNIX;
    (void)strcpy(server.sun_path, serverSockPath);
    int socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]client socket fail");
        return -1;
    }

    char *msgBuff = NULL;
    if (connect(socketFd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]fail to connect to agent by socket, %s", strerror(errno));
        goto exit;
    }
    msgBuff = (char *)malloc(MAX_MSG_LEN);
    if (msgBuff == NULL) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]malloc msgBuff fail");
        goto exit;
    }
    (void)memset(msgBuff, 0, MAX_MSG_LEN);

    size_t msgLen = strlen(message);
    memcpy(msgBuff, &msgLen, sizeof(size_t));
    memcpy(msgBuff + MSG_HEADER_LEN, message, msgLen);

    if (write(socketFd, msgBuff, msgLen + MSG_HEADER_LEN) == -1) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]fail to write to agent by socket, %s", strerror(errno));
        goto exit;
    }

    (void)memset(msgBuff, 0, MAX_MSG_LEN);
    if (read(socketFd, msgBuff, MAX_MSG_LEN) > 0) {
        msgLen = strlen(msgBuff);
        ret = GetCodeFromMessage(msgBuff, msgLen);
    }
exit:
    close(socketFd);
    if (msgBuff != NULL) {
        free(msgBuff);
    }
    
    return ret;
}

int CheckUserJobsFromAgent(const pid_t procPid, const char *user, const char *sshConnectInfo, const char *agentSocketFile)
{
    if (agentSocketFile == NULL || access(agentSocketFile, F_OK) != 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]CheckUserJobsFromAgent agentSocketFile is not available");
        return -1;
    }
    char request[PRINT_BUFFER_LEN] = {0};
    int errorNo = sprintf(request, "{\"msgType\":\"INTRA_NODE_TASK_PAM_QUERY_REQ\",\"containerID\":\"\","\
        "\"messageBody\":\"{\\\"UserName\\\":\\\"%s\\\",\\\"SshConnInfo\\\":\\\"%s\\\",\\\"Pid\\\":\\\"%d\\\"}\","\
        "\"version\":\"%s\"}END", user, sshConnectInfo, procPid, donau_agent_version);
    if (errorNo == -1) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]combination request failed");
        return errorNo;
    }

    int ret = SendRecvMessage(request, agentSocketFile);
    return ret;
}

void SubProcessCmd(int ePorts[2], const pid_t pid)
{
    close(ePorts[0]);
    dup2(ePorts[1], STDOUT_FILENO);
    char cmdLine[CMD_MAX_LEN + 1] = {0};
    char actualpath[PATH_MAX] = {0};
    const char *lsof = g_lsofPath;
    if (access(lsof, R_OK | X_OK) != 0) {
        lsof = g_sys_lsofPath;
    }
    int errorNo = sprintf(cmdLine, "timeout %d %s -p %ld 2>/dev/null | grep TCP | awk '{print $9}'", CMD_TIMEOUT_SEC, lsof, pid);
    if (errorNo == -1) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]combination cmd lsof failed");
        return;
    }

    char *argList[] = {BASH_PATH, "-c", cmdLine, NULL};
    int status_code = execv(BASH_PATH, argList);
    if (status_code == -1) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]execv run failed, %s", strerror(errno));
    }
}

void MainProcessProc(const pid_t pid, int ePorts[2], char **strContent)
{
    if (strContent == NULL) {
        return;
    }
    close(ePorts[1]);
    char *readBuf = (char *)malloc(PRINT_BUFFER_LEN + 1);
    if (readBuf == NULL) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]malloc failed");
        close(ePorts[0]);
        return;
    }
    (void)memset(readBuf, 0, PRINT_BUFFER_LEN + 1);

    int cc = read(ePorts[0], readBuf, PRINT_BUFFER_LEN);
    if (cc <= 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]main process read failed");
        free(readBuf);
        close(ePorts[0]);
        return;
    }
    if (readBuf[cc - 1] == '\n') {
        readBuf[cc - 1] = '\0';
    }
    close(ePorts[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
    }

    if (WIFEXITED(status) != 0) {
        *strContent = readBuf;
        return;
    }
    if (WIFSIGNALED(status)) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]cmd proc process was interrupted by signal %d", WTERMSIG(status));
    } else {
        syslog(LOG_ERR, "[pam_donau_adopt][error]cmd proc process exit abnormal, status=%d, WIFEXITED(status)=%d",
        status, WIFEXITED(status));
    }
}

char *GetSshConnectInfo(const pid_t procPid)
{
    int ePorts[2] = {0};
    if (pipe(ePorts) < 0) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]pipe failed");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(ePorts[0]);
        close(ePorts[1]);
        syslog(LOG_ERR, "[pam_donau_adopt][error]fork failed");
        return NULL;
    }
    char *readBuf = NULL;
    if (pid == 0) {
        SubProcessCmd(ePorts, procPid);
    } else {
        MainProcessProc(pid, ePorts, &readBuf);
    }
    return readBuf;
}

/* check for user running jobs in Donau Agent */
int CheckUserJobs(const char *user, const char *agentSocketFile, const bool isDebug)
{
    if (isDebug) {
        syslog(LOG_NOTICE, "[pam_donau_adopt][notice]checking running jobs for user %s on donau agent", user);
    }
    pid_t procPid = getpid();
    char *sshConnectInfo = GetSshConnectInfo(procPid);
    if (sshConnectInfo == NULL) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]fail to get ssh connection info");
        return -1;
    }
    if (isDebug) {
        syslog(LOG_NOTICE, "[pam_donau_adopt][notice]CheckUserJobs [%s] [%s] [%s]", user, sshConnectInfo, agentSocketFile);
    }

    int ret = CheckUserJobsFromAgent(procPid, user, sshConnectInfo, agentSocketFile);
    free(sshConnectInfo);
    return ret;
}

static char *ParseVal(const char *argv)
{
    char *ret = NULL;
    char *val = strchr(argv, '=');
    if (val != NULL && val + 1 != NULL) {
        ret = val + 1;
    }
    return ret;
}

static int ParseOptions(const int argc, const char **argv, bool *isDebug, char **agentSocketFile)
{
    if (isDebug == NULL || agentSocketFile == NULL) {
        syslog(LOG_ERR, "[pam_donau_adopt][error]parse param failed, internal error");
        return -1;
    }
    int i = 0;
    for (; i < argc; i++) {
        if (strncasecmp(argv[i], g_argsLogLevel, strlen(g_argsLogLevel)) == 0) {
            char *val = ParseVal(argv[i]);
            if (val == NULL) {
                continue;
            }
            if (strcasecmp(val, g_logLevelDebug) == 0) {
                *isDebug = true;
            }
        } else if (strncasecmp(argv[i], g_argsAgentSocket, strlen(g_argsAgentSocket)) == 0) {
            char *val = ParseVal(argv[i]);
            if (val == NULL) {
                continue;
            }
            *agentSocketFile = val;
        } else {
            syslog(LOG_ERR, "[pam_donau_adopt][error]ParseOptions : arg [%s] not match", argv[i]);
        }
    }
    return 0;
}

/*
 * account management for user ssh request, verify user in the following orders:
 * 1) root user is ignored to check, no matter agent service is available or not
 * 2) cluster administrator is ignored to check for running jobs on Donau agent
 * 3) normal user is required to check running jobs on Donau agent
 */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    const char *user = NULL;
    bool isDebug = false;
    char *agentSocketFile = NULL;
    int retval = ParseOptions(argc, argv, &isDebug, &agentSocketFile);
    if (retval != 0) {
        pam_syslog(pamh, LOG_ERR, "[error]argument parsing failed");
        return PAM_SESSION_ERR;
    }

    if (isDebug) {
        pam_syslog(pamh, LOG_DEBUG, "[debug]starting acct_mgmt");
    }

    int returnValue = pam_get_item(pamh, PAM_USER, (const void **)&user);
    if (returnValue != PAM_SUCCESS || user == NULL) {
        pam_syslog(pamh, LOG_ERR, "[error]username is not found in PAM_USER, error: %s", pam_strerror(pamh, returnValue));
        return PAM_USER_UNKNOWN;
    }

    if (isDebug) {
        pam_syslog(pamh, LOG_DEBUG, "[debug]username = %s", user);
    }
    /* permit root */
    if (strcmp(user, g_rootUser) == 0) {
        return PAM_IGNORE;
    }

    int ret = CheckUserJobs(user, agentSocketFile, isDebug);
    if (ret != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_ERR, "[error]account authentication failed for %s", user);
        return PAM_AUTH_ERR;
    }
    if (isDebug) {
        pam_syslog(pamh, LOG_DEBUG, "[debug]account authentication success for %s", user);
    }
    return PAM_SUCCESS;
}

#ifdef PAM_STATIC
struct pam_module _pam_donau_adopt_modstruct = {
    "pam_donau_adopt", NULL, NULL, pam_sm_acct_mgmt, NULL, NULL, NULL
};
#endif
