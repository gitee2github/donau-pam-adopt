# Donau PAM 可插入认证模块

## 如何构建和安装

### 编译

>>从openEuler社区下载 **pam_donau_adopt** 源文件，并存放在独立目录下
>>进入源文件目录，运行 `make clean;make`

### 安装

（1）方法1：
>>运行 `make install`

（2）方法2：
>>拷贝复制 **pam_donau_adopt.so** 到 `/lib64/security/` 或 `/lib/security/` 或当前操作系统 pam security 指定的文件目录下
>
>>修改目标目录下 **pam_donau_adopt.so** 权限为 **500**
>
>>修改目标目录下 **pam_donau_adopt.so** 属主为 **root**，属组为 **root**

### 配置

（1）使用 `vi` 命令打开sshd配置文件
>> `vi /etc/pam.d/sshd`

（2）添加如下内容到文件中 `account` 段的末尾
>> `-account    required    pam_donau_adopt.so log_level=debug donau_agent_socket=/tmp/batch/4230533106/.socket/agent.socket`

 *说明：*
>*- log_level*: 日志级别，**debug** 级别会将所有日志记录，其他级别只会记录错误日志信息。日志记录在 **/var/log/secure** 和 **/var/log/message**

>*- donau_agent_socket*: Donau Agent服务的socket文件路径，需要根据当前节点实际环境配置

## FAQs

### 编译报错

编译构建时出现如下报错
>> `fatal error: security/pam_xxx.h: No such file or directory`

__解决方法__

报错表示编译环境缺少PAM开发依赖组件，需要安装 `libpam` 的开发组件包

1. 安装组件

- 在Ubuntu系统上

>> `apt-get install libpam0g-dev`

- 在CentOS、openEuler、Red Hat或麒麟系统上

>> `yum install pam-devel`

2. 检查缺失的头文件是否安装到 `/usr/include/security` 路径下

>> `ls -a /usr/include/security`
