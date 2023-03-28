# Donau PAM 可插入认证模块

## 如何构建和安装

### 编译
>>从OpenEuler社区下载pam_donau_adopt源文件，并存放在独立目录下
>>进入源文件目录，运行 `make clean;make`

### 安装
（1）方法1：
>>运行 `make install`

（2）方法2：
>>手工拷贝复制 pam_donau_adopt.so 到 `/lib64/security/` 或 `/lib/security/` 或当前操作系统 pam security 指定的文件目录下

###配置
（1）使用 vi 命令打开sshd配置文件
>>`vi /etc/pam.d/sshd`

（2）添加如下内容到文件中
>> `-account    sufficient    pam_donau_adopt.so log_level=debug donau_agent_socket=/tmp/batch/4230533106/.socket/agent.socket`
 *说明：*
>*- log_level 日志级别，debug级别会将所有日志记录，其他级别只会记录错误日志信息。日志记录在/var/log/secure和/var/log/message*
>*- donau_agent_socket Donau Agent服务的socket文件路径，需要根据当前节点实际环境配置*

### 配置白名单
默认仅允许有作业运行的用户ssh到此节点，若您希望允许其他用户无作业时访问，需要配置白名单
（1）使用 vi 命令打开sshd配置文件
>> `vi /etc/pam.d/sshd`

（2）在上述配置操作后追加如下内容
>> `account required pam_access.so`

（3）创建文件 `/etc/security/access.conf`，在文件中添加如下内容
>> `+:whistle:ALL`
>> `-:ALL:ALL`

## FAQ

###编译报错
编译构建时出现如下报错
>> `fatal error: security/pam_modules.h: No such file or directory`

__解决方法__
报错表示编译环境缺少PAM开发依赖组件，需要安装 `libpam` 的开发组件包，在Debian、Ubuntu 或 Linux Mint系统上
>> `sudo apt-get install libpam0g-dev`

在CentOS、Fedora 或 RHEL系统上
>> `sudo yum install gcc pam-devel`

检查缺失的头文件是否安装到 `/usr/include/security` 路径下
>> `ls -a /usr/include/security`
