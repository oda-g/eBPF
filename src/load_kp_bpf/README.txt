1. はじめに

load_kp_bpf プログラムは、kprobe 用 eBPFプログラムのローダである。
clangでコンパイルして作成した、ELFファイルを入力とし、マップの準備やeBPFプログラムのロード、eBPFプログラムの有効化を行う。

以降の章で、使用するための手順やポイントについて解説する。

2. eBPFプログラム

eBPFプログラムは、tc 用と同様に作成する。ただし、以下の違いがある。
- プログラムセクションはひとつのみで、セクション名は、"prog" であること。
  ひとつのファイルでひとつのeBPFプログラムしか定義できない。
- プログラムに渡って来るパラメータは、「pt_regs *」である。
  プログラムタイプが BPF_PROG_TYPE_KPROBE の場合は、これである。<linux/ptrace.h>をインクルードする。

マップの取り扱いについては、tc と合わせてある。すなわち、
- "maps"セクションのマップの定義を行う。構造体は、tc と同じものを使用している。

マップ定義のpinningメンバに以下の意味を持たせている。
- 0 (PIN_NONE)
  pinしない。
- 1 (PIN_OBJECT_NS)
  /sys/fs/bpf/ 直下にpinする。
- 2 (PIN_GLOBAL_NS)
  /sys/fs/bpf/tc/globals/ 下にpinする。
  これを指定しておけば、tc 用プログラムとマップの共有ができる。
pinning 1, 2 の場合、既にマップが存在すれば、それを使用し、なければ、新規に作成してpinする。このあたりの動作は、tc と同じである。

kprobe 対象関数の引数について：
eBPFプログラムへのパラメータとしては、「pt_regs *ctx」が渡って来るので、その中から、関数へのパラメータを取り出すことができる。
例えば、第一引数は ctx->rdi となる。

当ディレクトリの下に例題として、test_prog.c があるので、参考にされたい。

3. コンパイル

tc用eBPFプログラムと同様である。

$ clang -O2 -Wall -target bpf -c test_prog.c -o test_prog.o

4. load_kp_bpf の実行

シンタックスは、以下のとおり。
「load_kp_bpf {ELFファイル} {カーネル関数名}」

実行例:
$ sudo ./load_kp_bpf test_prog.o sys_bpf
interrupt するまで実行。interrupt したら終了。(プログラムは消える。マップは、pinされていれば、残る。)

5. kprobe eBPFの有効化(参考)

load_kp_bpf では、マップの準備、eBPFプログラムのロードまでは、tc と同様のことを行っている(はずである)。
eBPFプログラムの有効化については、プログラムタイプが異なるため、異なる。

kprobeタイプの有効化の手順を説明する。

(1) kprobe イベントの設定

kprobe用eBPFでは、まず、kprobeイベントを設定する必要がる。

/sys/kernel/debug/tracing/kprobe_events に書き込み:

書き込む形式は、以下のとおり。
「p:kprobes/{event名} 関数名」
event名は、任意の(ユニークな)文字列。後で参照する。関数名は、kprobe を掛けたいカーネルの関数名。
(traceのためには、まだこの後にも定義するパラメータがあるが、eBPF用には、これで十分)

指定例: (test_progの場合。sys_bpf に kprobe を掛ける想定である。)
# echo "p:kprobes/test_bpf sys_bpf" >> /sys/kernel/debug/tracing/kprobe_events

注意: 「>>」を使うこと。「>」を使うと定義済のものが消えてしまう。
  
(設定を個別に)削除したい場合は、「p」を「-」に変えて、書き込む。
# echo "-:kprobes/test_bpf sys_bpf" >> /sys/kernel/debug/tracing/kprobe_events
(上記の注意を逆手に取って、# echo > /sys/kernel/debug/tracing/kprobe_events とやれば、すべての設定を削除できる。)
(load_kp_bpfでは、interrupt時に、消している。)

設定は、/sys/kernel/debug/tracing/kprobe_events を参照して確認できる。
# cat /sys/kernel/debug/tracing/kprobe_events
p:kprobes/test_bpf sys_bpf
#

設定を行うと、/sys/kernel/debug/tracing/events/kprobes/{event名} ディレクトリが作成され、いくつかのファイルができる。
(正確には、eventsの下に設定した、kprobes/{event名} ができる。実は、kprobesの部分も任意で、ディレクトリによるグループ化が
できるようになっている。)
後で、idファイルを参照することになる。
# ls /sys/kernel/debug/tracing/events/kprobes/test_bpf
enable  filter  format  hist  id  trigger
# cat /sys/kernel/debug/tracing/events/kprobes/test_bpf/id
1478
#

(2) perf_event_open システムコールによる設定

次に、(1)で設定したeventに対し、有効化、および、eBPFプログラムとの関連付けを行う。
load_kp_bpf のポイントの部分のみ抜き出すと以下のようになる。

-------------------------------------------
struct perf_event_attr ev_attr = {};
ev_attr.config = 1478; // ここにidファイルの値を設定する。
ev_attr.type = PERF_TYPE_TRACEPOINT;
efd = syscall(__NR_perf_event_open, &ev_attr, -1/*pid*/, 0/*cpu*/, -1/*group_fd*/, 0);
ioctl(efd, PERF_EVENT_IOC_ENABLE, 0); //トレースの有効化
ioctl(efd, PERF_EVENT_IOC_SET_BPF, prog_fd); // eBPFプログラムをトレースに結びつける。prog_fd は、eBPFプログラムロード時のファイルディスクリプタ
--------------------------------------------
(エラー処理は略した)

以上