/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD$
 * created from FreeBSD: stable/10/sys/amd64/linux/syscalls.master 293555 2016-01-09 16:58:57Z dchagin 
 */

#define	LINUX_SYS_read	0
#define	LINUX_SYS_write	1
#define	LINUX_SYS_linux_open	2
#define	LINUX_SYS_close	3
#define	LINUX_SYS_linux_newstat	4
#define	LINUX_SYS_linux_newfstat	5
#define	LINUX_SYS_linux_newlstat	6
#define	LINUX_SYS_poll	7
#define	LINUX_SYS_linux_lseek	8
#define	LINUX_SYS_linux_mmap2	9
#define	LINUX_SYS_linux_mprotect	10
#define	LINUX_SYS_munmap	11
#define	LINUX_SYS_linux_brk	12
#define	LINUX_SYS_linux_rt_sigaction	13
#define	LINUX_SYS_linux_rt_sigprocmask	14
#define	LINUX_SYS_linux_rt_sigreturn	15
#define	LINUX_SYS_linux_ioctl	16
#define	LINUX_SYS_linux_pread	17
#define	LINUX_SYS_linux_pwrite	18
#define	LINUX_SYS_readv	19
#define	LINUX_SYS_writev	20
#define	LINUX_SYS_linux_access	21
#define	LINUX_SYS_linux_pipe	22
#define	LINUX_SYS_linux_select	23
#define	LINUX_SYS_sched_yield	24
#define	LINUX_SYS_linux_mremap	25
#define	LINUX_SYS_linux_msync	26
#define	LINUX_SYS_linux_mincore	27
#define	LINUX_SYS_madvise	28
#define	LINUX_SYS_linux_shmget	29
#define	LINUX_SYS_linux_shmat	30
#define	LINUX_SYS_linux_shmctl	31
#define	LINUX_SYS_dup	32
#define	LINUX_SYS_dup2	33
#define	LINUX_SYS_linux_pause	34
#define	LINUX_SYS_linux_nanosleep	35
#define	LINUX_SYS_linux_getitimer	36
#define	LINUX_SYS_linux_alarm	37
#define	LINUX_SYS_linux_setitimer	38
#define	LINUX_SYS_linux_getpid	39
#define	LINUX_SYS_linux_sendfile	40
#define	LINUX_SYS_linux_socket	41
#define	LINUX_SYS_linux_connect	42
#define	LINUX_SYS_linux_accept	43
#define	LINUX_SYS_linux_sendto	44
#define	LINUX_SYS_linux_recvfrom	45
#define	LINUX_SYS_linux_sendmsg	46
#define	LINUX_SYS_linux_recvmsg	47
#define	LINUX_SYS_linux_shutdown	48
#define	LINUX_SYS_linux_bind	49
#define	LINUX_SYS_linux_listen	50
#define	LINUX_SYS_linux_getsockname	51
#define	LINUX_SYS_linux_getpeername	52
#define	LINUX_SYS_linux_socketpair	53
#define	LINUX_SYS_linux_setsockopt	54
#define	LINUX_SYS_linux_getsockopt	55
#define	LINUX_SYS_linux_clone	56
#define	LINUX_SYS_linux_fork	57
#define	LINUX_SYS_linux_vfork	58
#define	LINUX_SYS_linux_execve	59
#define	LINUX_SYS_linux_exit	60
#define	LINUX_SYS_linux_wait4	61
#define	LINUX_SYS_linux_kill	62
#define	LINUX_SYS_linux_newuname	63
#define	LINUX_SYS_linux_semget	64
#define	LINUX_SYS_linux_semop	65
#define	LINUX_SYS_linux_semctl	66
#define	LINUX_SYS_linux_shmdt	67
#define	LINUX_SYS_linux_msgget	68
#define	LINUX_SYS_linux_msgsnd	69
#define	LINUX_SYS_linux_msgrcv	70
#define	LINUX_SYS_linux_msgctl	71
#define	LINUX_SYS_linux_fcntl	72
#define	LINUX_SYS_flock	73
#define	LINUX_SYS_fsync	74
#define	LINUX_SYS_linux_fdatasync	75
#define	LINUX_SYS_linux_truncate	76
#define	LINUX_SYS_linux_ftruncate	77
#define	LINUX_SYS_linux_getdents	78
#define	LINUX_SYS_linux_getcwd	79
#define	LINUX_SYS_linux_chdir	80
#define	LINUX_SYS_fchdir	81
#define	LINUX_SYS_linux_rename	82
#define	LINUX_SYS_linux_mkdir	83
#define	LINUX_SYS_linux_rmdir	84
#define	LINUX_SYS_linux_creat	85
#define	LINUX_SYS_linux_link	86
#define	LINUX_SYS_linux_unlink	87
#define	LINUX_SYS_linux_symlink	88
#define	LINUX_SYS_linux_readlink	89
#define	LINUX_SYS_linux_chmod	90
#define	LINUX_SYS_fchmod	91
#define	LINUX_SYS_linux_chown	92
#define	LINUX_SYS_fchown	93
#define	LINUX_SYS_linux_lchown	94
#define	LINUX_SYS_umask	95
#define	LINUX_SYS_gettimeofday	96
#define	LINUX_SYS_linux_getrlimit	97
#define	LINUX_SYS_getrusage	98
#define	LINUX_SYS_linux_sysinfo	99
#define	LINUX_SYS_linux_times	100
#define	LINUX_SYS_linux_ptrace	101
#define	LINUX_SYS_linux_getuid	102
#define	LINUX_SYS_linux_syslog	103
#define	LINUX_SYS_linux_getgid	104
#define	LINUX_SYS_setuid	105
#define	LINUX_SYS_setgid	106
#define	LINUX_SYS_geteuid	107
#define	LINUX_SYS_getegid	108
#define	LINUX_SYS_setpgid	109
#define	LINUX_SYS_linux_getppid	110
#define	LINUX_SYS_getpgrp	111
#define	LINUX_SYS_setsid	112
#define	LINUX_SYS_setreuid	113
#define	LINUX_SYS_setregid	114
#define	LINUX_SYS_linux_getgroups	115
#define	LINUX_SYS_linux_setgroups	116
#define	LINUX_SYS_setresuid	117
#define	LINUX_SYS_getresuid	118
#define	LINUX_SYS_setresgid	119
#define	LINUX_SYS_getresgid	120
#define	LINUX_SYS_getpgid	121
#define	LINUX_SYS_linux_setfsuid	122
#define	LINUX_SYS_linux_setfsgid	123
#define	LINUX_SYS_linux_getsid	124
#define	LINUX_SYS_linux_capget	125
#define	LINUX_SYS_linux_capset	126
#define	LINUX_SYS_linux_rt_sigpending	127
#define	LINUX_SYS_linux_rt_sigtimedwait	128
#define	LINUX_SYS_linux_rt_sigqueueinfo	129
#define	LINUX_SYS_linux_rt_sigsuspend	130
#define	LINUX_SYS_linux_sigaltstack	131
#define	LINUX_SYS_linux_utime	132
#define	LINUX_SYS_linux_mknod	133
#define	LINUX_SYS_linux_personality	135
#define	LINUX_SYS_linux_ustat	136
#define	LINUX_SYS_linux_statfs	137
#define	LINUX_SYS_linux_fstatfs	138
#define	LINUX_SYS_linux_sysfs	139
#define	LINUX_SYS_linux_getpriority	140
#define	LINUX_SYS_setpriority	141
#define	LINUX_SYS_linux_sched_setparam	142
#define	LINUX_SYS_linux_sched_getparam	143
#define	LINUX_SYS_linux_sched_setscheduler	144
#define	LINUX_SYS_linux_sched_getscheduler	145
#define	LINUX_SYS_linux_sched_get_priority_max	146
#define	LINUX_SYS_linux_sched_get_priority_min	147
#define	LINUX_SYS_linux_sched_rr_get_interval	148
#define	LINUX_SYS_mlock	149
#define	LINUX_SYS_munlock	150
#define	LINUX_SYS_mlockall	151
#define	LINUX_SYS_munlockall	152
#define	LINUX_SYS_linux_vhangup	153
#define	LINUX_SYS_linux_pivot_root	155
#define	LINUX_SYS_linux_sysctl	156
#define	LINUX_SYS_linux_prctl	157
#define	LINUX_SYS_linux_arch_prctl	158
#define	LINUX_SYS_linux_adjtimex	159
#define	LINUX_SYS_linux_setrlimit	160
#define	LINUX_SYS_chroot	161
#define	LINUX_SYS_sync	162
#define	LINUX_SYS_acct	163
#define	LINUX_SYS_settimeofday	164
#define	LINUX_SYS_linux_mount	165
#define	LINUX_SYS_linux_umount	166
#define	LINUX_SYS_swapon	167
#define	LINUX_SYS_linux_swapoff	168
#define	LINUX_SYS_linux_reboot	169
#define	LINUX_SYS_linux_sethostname	170
#define	LINUX_SYS_linux_setdomainname	171
#define	LINUX_SYS_linux_iopl	172
#define	LINUX_SYS_linux_create_module	174
#define	LINUX_SYS_linux_init_module	175
#define	LINUX_SYS_linux_delete_module	176
#define	LINUX_SYS_linux_get_kernel_syms	177
#define	LINUX_SYS_linux_query_module	178
#define	LINUX_SYS_linux_quotactl	179
#define	LINUX_SYS_linux_nfsservctl	180
#define	LINUX_SYS_linux_getpmsg	181
#define	LINUX_SYS_linux_putpmsg	182
#define	LINUX_SYS_linux_afs_syscall	183
#define	LINUX_SYS_linux_tuxcall	184
#define	LINUX_SYS_linux_security	185
#define	LINUX_SYS_linux_gettid	186
#define	LINUX_SYS_linux_setxattr	188
#define	LINUX_SYS_linux_lsetxattr	189
#define	LINUX_SYS_linux_fsetxattr	190
#define	LINUX_SYS_linux_getxattr	191
#define	LINUX_SYS_linux_lgetxattr	192
#define	LINUX_SYS_linux_fgetxattr	193
#define	LINUX_SYS_linux_listxattr	194
#define	LINUX_SYS_linux_llistxattr	195
#define	LINUX_SYS_linux_flistxattr	196
#define	LINUX_SYS_linux_removexattr	197
#define	LINUX_SYS_linux_lremovexattr	198
#define	LINUX_SYS_linux_fremovexattr	199
#define	LINUX_SYS_linux_tkill	200
#define	LINUX_SYS_linux_time	201
#define	LINUX_SYS_linux_sys_futex	202
#define	LINUX_SYS_linux_sched_setaffinity	203
#define	LINUX_SYS_linux_sched_getaffinity	204
#define	LINUX_SYS_linux_set_thread_area	205
#define	LINUX_SYS_linux_lookup_dcookie	212
#define	LINUX_SYS_linux_epoll_create	213
#define	LINUX_SYS_linux_epoll_ctl_old	214
#define	LINUX_SYS_linux_epoll_wait_old	215
#define	LINUX_SYS_linux_remap_file_pages	216
#define	LINUX_SYS_linux_getdents64	217
#define	LINUX_SYS_linux_set_tid_address	218
#define	LINUX_SYS_linux_semtimedop	220
#define	LINUX_SYS_linux_fadvise64	221
#define	LINUX_SYS_linux_timer_create	222
#define	LINUX_SYS_linux_timer_settime	223
#define	LINUX_SYS_linux_timer_gettime	224
#define	LINUX_SYS_linux_timer_getoverrun	225
#define	LINUX_SYS_linux_timer_delete	226
#define	LINUX_SYS_linux_clock_settime	227
#define	LINUX_SYS_linux_clock_gettime	228
#define	LINUX_SYS_linux_clock_getres	229
#define	LINUX_SYS_linux_clock_nanosleep	230
#define	LINUX_SYS_linux_exit_group	231
#define	LINUX_SYS_linux_epoll_wait	232
#define	LINUX_SYS_linux_epoll_ctl	233
#define	LINUX_SYS_linux_tgkill	234
#define	LINUX_SYS_linux_utimes	235
#define	LINUX_SYS_linux_mbind	237
#define	LINUX_SYS_linux_set_mempolicy	238
#define	LINUX_SYS_linux_get_mempolicy	239
#define	LINUX_SYS_linux_mq_open	240
#define	LINUX_SYS_linux_mq_unlink	241
#define	LINUX_SYS_linux_mq_timedsend	242
#define	LINUX_SYS_linux_mq_timedreceive	243
#define	LINUX_SYS_linux_mq_notify	244
#define	LINUX_SYS_linux_mq_getsetattr	245
#define	LINUX_SYS_linux_kexec_load	246
#define	LINUX_SYS_linux_waitid	247
#define	LINUX_SYS_linux_add_key	248
#define	LINUX_SYS_linux_request_key	249
#define	LINUX_SYS_linux_keyctl	250
#define	LINUX_SYS_linux_ioprio_set	251
#define	LINUX_SYS_linux_ioprio_get	252
#define	LINUX_SYS_linux_inotify_init	253
#define	LINUX_SYS_linux_inotify_add_watch	254
#define	LINUX_SYS_linux_inotify_rm_watch	255
#define	LINUX_SYS_linux_migrate_pages	256
#define	LINUX_SYS_linux_openat	257
#define	LINUX_SYS_linux_mkdirat	258
#define	LINUX_SYS_linux_mknodat	259
#define	LINUX_SYS_linux_fchownat	260
#define	LINUX_SYS_linux_futimesat	261
#define	LINUX_SYS_linux_newfstatat	262
#define	LINUX_SYS_linux_unlinkat	263
#define	LINUX_SYS_linux_renameat	264
#define	LINUX_SYS_linux_linkat	265
#define	LINUX_SYS_linux_symlinkat	266
#define	LINUX_SYS_linux_readlinkat	267
#define	LINUX_SYS_linux_fchmodat	268
#define	LINUX_SYS_linux_faccessat	269
#define	LINUX_SYS_linux_pselect6	270
#define	LINUX_SYS_linux_ppoll	271
#define	LINUX_SYS_linux_unshare	272
#define	LINUX_SYS_linux_set_robust_list	273
#define	LINUX_SYS_linux_get_robust_list	274
#define	LINUX_SYS_linux_splice	275
#define	LINUX_SYS_linux_tee	276
#define	LINUX_SYS_linux_sync_file_range	277
#define	LINUX_SYS_linux_vmsplice	278
#define	LINUX_SYS_linux_move_pages	279
#define	LINUX_SYS_linux_utimensat	280
#define	LINUX_SYS_linux_epoll_pwait	281
#define	LINUX_SYS_linux_signalfd	282
#define	LINUX_SYS_linux_timerfd	283
#define	LINUX_SYS_linux_eventfd	284
#define	LINUX_SYS_linux_fallocate	285
#define	LINUX_SYS_linux_timerfd_settime	286
#define	LINUX_SYS_linux_timerfd_gettime	287
#define	LINUX_SYS_linux_accept4	288
#define	LINUX_SYS_linux_signalfd4	289
#define	LINUX_SYS_linux_eventfd2	290
#define	LINUX_SYS_linux_epoll_create1	291
#define	LINUX_SYS_linux_dup3	292
#define	LINUX_SYS_linux_pipe2	293
#define	LINUX_SYS_linux_inotify_init1	294
#define	LINUX_SYS_linux_preadv	295
#define	LINUX_SYS_linux_pwritev	296
#define	LINUX_SYS_linux_rt_tsigqueueinfo	297
#define	LINUX_SYS_linux_perf_event_open	298
#define	LINUX_SYS_linux_recvmmsg	299
#define	LINUX_SYS_linux_fanotify_init	300
#define	LINUX_SYS_linux_fanotify_mark	301
#define	LINUX_SYS_linux_prlimit64	302
#define	LINUX_SYS_linux_name_to_handle_at	303
#define	LINUX_SYS_linux_open_by_handle_at	304
#define	LINUX_SYS_linux_clock_adjtime	305
#define	LINUX_SYS_linux_syncfs	306
#define	LINUX_SYS_linux_sendmmsg	307
#define	LINUX_SYS_linux_setns	308
#define	LINUX_SYS_linux_process_vm_readv	309
#define	LINUX_SYS_linux_process_vm_writev	310
#define	LINUX_SYS_linux_kcmp	311
#define	LINUX_SYS_linux_finit_module	312
#define	LINUX_SYS_MAXSYSCALL	313
