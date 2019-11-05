カーネル解析(参考)
================

カーネルコードクロスリファレンス
----------------------------

下記、URLにカーネルのソースコードのクロスリファレンスがある。便利。

https://elixir.bootlin.com/linux/latest/source

crash
-----

ダンプ解析用ツールであるが、動作中のカーネルにも使用することができる。ばかでかい構造体や山のようなifdefで、実際に動作しているカーネルのコードや構造体がどうなっているのか、ソースから追うのは難しい。crashで動作カーネルを見るのがてっとり早くて便利である。

(注: ifdef自体は、/boot/config-<カーネルバージョン> ファイルで確認できる。)

インストール
^^^^^^^^^^

::

  $ sudo apt install crash
  
debug symbol 取得
^^^^^^^^^^^^^^^^^

カーネルのdegug symbol パッケージが必要。debug symbol パッケージの取得は、https://wiki.ubuntu.com/Debug%20Symbol%20Packages 参照
(下記手順もそれに準拠)。

::

  $ sudo tee /etc/apt/sources.list.d/ddebs.list << EOF
  > deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse
  > deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse
  > deb http://ddebs.ubuntu.com $(lsb_release -cs)-proposed main restricted universe multiverse
  > EOF
  $ sudo apt install ubuntu-dbgsym-keyring
  $ sudo apt-get update
  $ sudo apt install linux-image-$(uname -r)-dbgsym

/usr/lib/debug/boot/の下にvmlinux-<カーネルバージョン>という名前でファイルがインストールされる。

crash使用例
^^^^^^^^^^

起動::

  $ sudo crash /usr/lib/debug/boot/vmlinux-4.15.0-20-generic
  crash 7.2.1
  Copyright (C) 2002-2017  Red Hat, Inc.
  Copyright (C) 2004, 2005, 2006, 2010  IBM Corporation
  Copyright (C) 1999-2006  Hewlett-Packard Co
  Copyright (C) 2005, 2006, 2011, 2012  Fujitsu Limited
  Copyright (C) 2006, 2007  VA Linux Systems Japan K.K.
  Copyright (C) 2005, 2011  NEC Corporation
  Copyright (C) 1999, 2002, 2007  Silicon Graphics, Inc.
  Copyright (C) 1999, 2000, 2001, 2002  Mission Critical Linux, Inc.
  This program is free software, covered by the GNU General Public License,
  and you are welcome to change it and/or distribute copies of it under
  certain conditions.  Enter "help copying" to see the conditions.
  This program has absolutely no warranty.  Enter "help warranty" for details.
 
  GNU gdb (GDB) 7.6
  Copyright (C) 2013 Free Software Foundation, Inc.
  License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
  This is free software: you are free to change and redistribute it.
  There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
  and "show warranty" for details.
  This GDB was configured as "x86_64-unknown-linux-gnu"...

  WARNING: kernel relocated [326MB]: patching 99211 gdb minimal_symbol values

  crash: read error: kernel virtual address: ffffffff96a4a268  type: "page_offset_base"
  crash: this kernel may be configured with CONFIG_STRICT_DEVMEM, which
         renders /dev/mem unusable as a live memory source.
  crash: trying /proc/kcore as an alternative to /dev/mem

        KERNEL: /usr/lib/debug/boot/vmlinux-4.15.0-20-generic
      DUMPFILE: /proc/kcore
          CPUS: 4
          DATE: Tue Nov  5 09:17:48 2019
        UPTIME: 64 days, 12:08:11
  LOAD AVERAGE: 0.35, 0.35, 0.26
         TASKS: 156
      NODENAME: ebpf1
       RELEASE: 4.15.0-20-generic
       VERSION: #21-Ubuntu SMP Tue Apr 24 06:16:15 UTC 2018
       MACHINE: x86_64  (1797 Mhz)
        MEMORY: 19.5 GB
           PID: 21221
       COMMAND: "crash"
          TASK: ffff88d854a0dc00  [THREAD_INFO: ffff88d854a0dc00]
           CPU: 1
         STATE: TASK_RUNNING (ACTIVE)
  crash>


構造体確認::

  crash> struct -o sk_buff
  struct sk_buff {
          union {
              struct {
      [0]         struct sk_buff *next;
      [8]         struct sk_buff *prev;
                  union {
     [16]             struct net_device *dev;
     [16]             unsigned long dev_scratch;
                  };
              };
      [0]     struct rb_node rbnode;
          };
     [24] struct sock *sk;
          union {
     [32]     ktime_t tstamp;
     [32]     u64 skb_mstamp;
          };
     [40] char cb[48];
          union {
              struct {
     [88]         unsigned long _skb_refdst;
     [96]         void (*destructor)(struct sk_buff *);
              };
     [88]     struct list_head tcp_tsorted_anchor;
    ...
    [186] __u16 inner_transport_header;
    [188] __u16 inner_network_header;
    [190] __u16 inner_mac_header;
    [192] __be16 protocol;
    [194] __u16 transport_header;
    [196] __u16 network_header;
    [198] __u16 mac_header;
    [200] __u32 headers_end[];
    [200] sk_buff_data_t tail;
    [204] sk_buff_data_t end;
    [208] unsigned char *head;
    [216] unsigned char *data;
    [224] unsigned int truesize;
    [228] refcount_t users;
  }
  SIZE: 232

カーネルモジュール内に定義された構造体やシンボルの確認には、ロードが必要。

::

  crash> mod
       MODULE       NAME                      SIZE  OBJECT FILE
  ffffffffc024b480  pata_acpi                16384  (not loaded)  [CONFIG_KALLSYMS]
  ffffffffc025db00  floppy                   77824  (not loaded)  [CONFIG_KALLSYMS]
  ffffffffc0267040  sysfillrect              16384  (not loaded)  [CONFIG_KALLSYMS]
  ffffffffc0270500  i2c_piix4                24576  (not loaded)  [CONFIG_KALLSYMS]
  ffffffffc027f880  virtio_net               45056  (not loaded)  [CONFIG_KALLSYMS]
  ...
  crash> mod -s virtio_net
       MODULE       NAME                      SIZE  OBJECT FILE
  ffffffffc027f880  virtio_net               45056  /lib/modules/4.15.0-20-generic/kernel/drivers/net/virtio_net.ko 
  crash> 

構造体の中身確認::

  crash> struct net_device ffff88d8507fc000
  struct net_device {
    name = "ens3\000\000\000\000\000\000\000\000\000\000\000", 
    name_hlist = {
      next = 0x0, 
      pprev = 0xffff88d5e4fd5f88
    }, 
    ifalias = 0x0, 
    mem_end = 0, 
    mem_start = 0, 
    base_addr = 0, 
    irq = 0, 
    carrier_changes = {
      counter = 2
    }, 
    state = 3, 
    dev_list = {
      next = 0xffff88d5df000050, 
      prev = 0xffff88d5e4802050
    }, 
    ...
    
メモリ内容確認::

  crash> rd pid_max
  ffffffff96a59e98:  0000000000008000                    ........
  crash> rd ffffffff96a59e98
  ffffffff96a59e98:  0000000000008000                    ........

逆アセンブルリスト::

  crash> dis ring_buffer_poll_wait
  0xffffffff9576d150 <ring_buffer_poll_wait>:     push   %rbp
  0xffffffff9576d151 <ring_buffer_poll_wait+1>:   cmp    $0xffffffff,%esi
  0xffffffff9576d154 <ring_buffer_poll_wait+4>:   mov    %rsp,%rbp
  0xffffffff9576d157 <ring_buffer_poll_wait+7>:   push   %r13
  0xffffffff9576d159 <ring_buffer_poll_wait+9>:   lea    0x60(%rdi),%r13
  0xffffffff9576d15d <ring_buffer_poll_wait+13>:  push   %r12
  0xffffffff9576d15f <ring_buffer_poll_wait+15>:  push   %rbx
  0xffffffff9576d160 <ring_buffer_poll_wait+16>:  je     0xffffffff9576d185 <ring_buffer_poll_wait+53>
  ...
  0xffffffff9576d1b3 <ring_buffer_poll_wait+99>:  lock addl $0x0,-0x4(%rsp)
  ...

(「lock addl $0x0,-0x4(%rsp)」== smp_mb())

おまけ: カーネル内メモリ確認
-------------------------

crashで動作カーネルを見るとき、/proc/kcoreを参照している。動作カーネルのメモリの内容は、/proc/kcoreを参照しても確認できる。

/proc/kcoreは、ELF形式(core file)になっている。

::

  $ sudo readelf -a /proc/kcore
  ELF Header:
    Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
    Class:                             ELF64
    Data:                              2's complement, little endian
    Version:                           1 (current)
    OS/ABI:                            UNIX - System V
    ABI Version:                       0
    Type:                              CORE (Core file)
    Machine:                           Advanced Micro Devices X86-64
    Version:                           0x1
    Entry point address:               0x0
    Start of program headers:          64 (bytes into file)
    Start of section headers:          0 (bytes into file)
    Flags:                             0x0
    Size of this header:               64 (bytes)
    Size of program headers:           56 (bytes)
    Number of program headers:         11
    Size of section headers:           0 (bytes)
    Number of section headers:         0
    Section header string table index: 0

  There are no sections in this file.

  There are no sections to group in this file.

  Program Headers:
    Type           Offset             VirtAddr           PhysAddr
                   FileSiz            MemSiz              Flags  Align
    NOTE           0x00000000000002a8 0x0000000000000000 0x0000000000000000
                   0x0000000000001914 0x0000000000000000         0x0
    LOAD           0x00007fffff602000 0xffffffffff600000 0xffffffffffffffff
                   0x0000000000001000 0x0000000000001000  RWE    0x1000
    LOAD           0x00007fff95602000 0xffffffff95600000 0x0000000440c00000
                   0x0000000001b69000 0x0000000001b69000  RWE    0x1000
    LOAD           0x00001fce40002000 0xffff9fce40000000 0xffffffffffffffff
                   0x00001fffffffffff 0x00001fffffffffff  RWE    0x1000
    LOAD           0x00007fffc0002000 0xffffffffc0000000 0xffffffffffffffff
                   0x000000003f000000 0x000000003f000000  RWE    0x1000
    LOAD           0x000008d340003000 0xffff88d340001000 0x0000000000001000
                   0x000000000009e000 0x000000000009e000  RWE    0x1000
    LOAD           0x0000410a40002000 0xffffc10a40000000 0xffffffffffffffff
                   0x0000000000003000 0x0000000000003000  RWE    0x1000
    LOAD           0x000008d340102000 0xffff88d340100000 0x0000000000100000
                   0x00000000bfedf000 0x00000000bfedf000  RWE    0x1000
    LOAD           0x0000410a40006000 0xffffc10a40004000 0xffffffffffffffff
                   0x0000000002ffc000 0x0000000002ffc000  RWE    0x1000
    LOAD           0x000008d440002000 0xffff88d440000000 0x0000000100000000
                   0x0000000422000000 0x0000000422000000  RWE    0x1000
    LOAD           0x0000410a44002000 0xffffc10a44000000 0xffffffffffffffff
                   0x0000000010880000 0x0000000010880000  RWE    0x1000

  There is no dynamic section in this file.

  There are no relocations in this file.

  The decoding of unwind sections for machine type Advanced Micro Devices X86-64 is not currently supported.

  Dynamic symbol information is not available for displaying symbols.

  No version information found in this file.

  Displaying notes found at file offset 0x000002a8 with length 0x00001914:
    Owner                 Data size	Description
    CORE                 0x00000150	NT_PRSTATUS (prstatus structure)
    CORE                 0x00000088	NT_PRPSINFO (prpsinfo structure)
    CORE                 0x00001700	NT_TASKSTRUCT (task structure)

プログラムセクションを見て、参照したい仮想アドレスがファイルのどのオフセットにあるか調べて、readすればよい。

プログラム例:

それをプログラムにしたのが、src/get_kval/get_kval.c。(指定したアドレスから4バイト読む)

動作確認例:

カーネルのシンボル情報は、/proc/kallsyms を見れば分かる。

::

  $ bash get_sym_val pid_max
  ffffffff96a59e98: 00008000(32768)



