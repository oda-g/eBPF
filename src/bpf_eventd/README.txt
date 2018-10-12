bpf_eventd
==========

概要
----

eBPFプログラムから発行したイベントを取得するプログラム。

usage
-----

bpf_eventd map-path output-file

ex.
sudo ./bpf_eventd /sys/fs/bpf/map_event /tmp/out

output-fileは、「-」を指定すると標準出力に出力する（主に動作確認用。後述の実行例参照。）

補足: システムのCPU数は、get_nprocs_conf(3)で取得するようにした。


setup
-----

$ gcc -o bpf_eventd bpf_eventd.c
$ gcc -o print_event print_event.c
$ clang -O2 -Wall -target bpf -c test_prog1.c -o test_prog1.o
$ clang -O2 -Wall -target bpf -c test_prog2.c -o test_prog2.o


実行例
------
(1) sudo ./bpf_eventd /sys/fs/bpf/map_event - | ./print_event
  まず最初にこれを動かす。マップmap_eventを作成し、イベント取得のための設定を行う。

(2) sudo .../load_kp_bpf test_prog1.o sys_bpf
(3) sudo .../load_kp_bpf test_prog2.o sys_bpf
  eBPFプログラムを動かす。同じ関数に複数設定しても問題ない。

(4) sudo bpftool map show
    sudo taskset 2 map show
    ...
  sys_bpfを通るような操作をいろいろやってみる。

(終了は、逆順に)


イベントの発行の仕方
--------------------

実行例で使用したプログラムを例に説明する。

--- test_prog1.c ---
#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/ptrace.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdint.h>
#include "bpf_elf.h"

#ifndef __section
#define __section(NAME) \
__attribute__((section(NAME), used))
#endif

#ifndef __inline
#define __inline \
inline __attribute((always_inline))
#endif

#ifndef BPF_FUNC
#define BPF_FUNC(NAME, ...) \
(*NAME)(__VA_ARGS__) = (void *)BPF_FUNC_##NAME
#endif

/* for x86_64 only */
#define PT_REGS_PARM1(x) ((x)->rdi)
#define PT_REGS_PARM2(x) ((x)->rsi)
#define PT_REGS_PARM3(x) ((x)->rdx)

static uint32_t BPF_FUNC(get_smp_processor_id, void);
static uint64_t BPF_FUNC(ktime_get_ns, void);
static int BPF_FUNC(perf_event_output, void *, void *, uint64_t, void *, uint64_t);

struct bpf_elf_map map_event __section("maps") = {     ★ イベント取得用マップの定義。bpf_eventd で指定するものと合わせること。
	.type		= BPF_MAP_TYPE_PERF_EVENT_ARRAY,          bpf_eventdで予め作成することを前提としているため、名前とpinningの
	.size_key	= sizeof(uint32_t),                       設定以外は、load_kp_bpf で参照されないので、適当な値でよい。
	.size_value	= sizeof(uint32_t),                       (特に max_elem は、予め分からないので、適当でよい。)
	.pinning	= PIN_OBJECT_NS,
	.max_elem	= 4,
};

__section("prog")
int test_prog(struct pt_regs *ctx)
{
	struct {                                           ★ 出力するデータの定義。
		short idx;                                        実際には、共通化し、ヘッダに定義した方がよいでしょう。
		short cmd;
		uint32_t cpu;
		uint64_t time;
	} data;

	data.idx = 1;
	data.cmd = (short)PT_REGS_PARM1(ctx);
	data.cpu = get_smp_processor_id();                 ★ 補足:イベントはCPUごとに記録されるが、bpf_eventdで区別なくひとつのファイルに
	data.time = ktime_get_ns();                                出すので、データの中にも記録しておいてください。
	perf_event_output(ctx, &map_event, BPF_F_CURRENT_CPU,  ★ これが出力用ヘルパー関数。第一引数は、pt_regs *ctx をそのまま指定する。
			   	&data, (uint64_t)sizeof(data));               第2引数がmapの指定。第３引数は、例のとおり指定する。
                                                              第4,5引数は、データのポインタとデータの大きさ。
	return 0;
}

char __license[] __section("license") = "GPL";
---

補足：event用マップは、送信側と受信側で分けた方がよいかも。(その場合は、bpf_eventd を２つ動かすことになる。)