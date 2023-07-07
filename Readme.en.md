# Donau PAM Pluggable Authentication Module

## Build and Installation

### Build

>>Download the **pam_donau_adopt** source file from the openEuler community, and save it to an independent directory. 
>>Go to the source file directory, and run the `make clean;make` command.

### Installation

（1）Method 1：
>>Run `make install`

（2）Method 2：
>>Copy **pam_donau_adopt.so** to `/lib64/security/`， `/lib/security/`， or a user-specified path.
>
>>Change the file permission to **500**.
>
>>Change the file owner and group to **root**.

### Configuration

（1）Open the sshd configuration file
>> `vi /etc/pam.d/sshd`

（2）Add the following content to the end of the `account` section in this file:
>> `-account    required    pam_donau_adopt.so log_level=debug donau_agent_socket=/tmp/batch/4230533106/.socket/agent.socket`

### Note:
>*- log_level*: log level of this component. The **debug** level outputs all logs, other levels record only error logs.
> Logs are recorded in **/var/log/secure** and **/var/log/message**.

>*- donau_agent_socket*: socket file path of the Donau Agent service. Set the parameter to the actual path of the service.

## FAQs

### Compilation Error

The following error message may be displayed during compilation and build:
>> `fatal error: security/pam_xxx.h: No such file or directory`

__Solution__

The error occurs because the compilation environment does not have the dependent component `libpam` for PAM development.

1. Install the component.

- Ubuntu

>> `apt-get install libpam0g-dev`

- CentOS, openEuler, Red Hat or Kylin

>> `yum install pam-devel`

2. Check whether the missing header file has been installed in `/usr/include/security`.

>> `ls -a /usr/include/security`