bpfプログラムの作成と実行(3)(event)
================================

これまでの例題では、(arrayタイプの)マップに情報を格納したが、トレース(イベント)ログのような大量の情報の格納には向いていない。
(基本的に、マップは、ユーザ・カーネル間での情報交換に使用すると考えた方がよい。）

kprobeタイプのbpfプログラムは、トレース用途での使用が多いと考えられる。ここでは、bpfプログラムからイベントログを出力して、それをユーザプログラムで受け取るための仕組みについて、説明する。

コードは、src/bpf_eventd/ の下にある。

bpfプログラム側の処理
-------------------

src/bpf_evend/test_prog.c 参照。

::

    15	struct bpf_elf_map bpf_sys_cnt __section("maps") = {
    ...
    23	__section("prog")
    24	int test_prog(struct pt_regs *ctx)
    25	{
    26		struct bpf_sys_data data;
    27	
    28		data.cmd = (uint32_t)PT_REGS_PARM1(ctx);
    29		data.cpu = get_smp_processor_id();
    30		data.time = ktime_get_ns();
    31		perf_event_output(ctx, &bpf_sys_cnt, BPF_F_CURRENT_CPU,
    32				   	&data, (uint64_t)sizeof(data));
    33	
    34		return 0;
    35	}

イベントログの出力には、perf_event_output ヘルパー関数を使用する。パラメータは以下のとおり。

* コンテキスト: bpfプログラムに渡ってきたものをそのまま渡せばよい。
* BPF_MAP_TYPE_PERF_EVENT_ARRAYタイプのマップへの参照。(内容は後述
* CPU番号(==マップのインデックス)。上記のBPF_F_CURRENT_CPUは、本bpfプログラムを実行したCPUを意味する。
* 出力データのアドレス(void *)。
* 出力データの大きさ。出力データの中身は任意である。

上記プログラムでは、sys_bpf関数にkprobeを設定することを前提としており、イベントデータとしては、sys_bpfのコマンド、実行CPU、実行時刻を出力するようにしている。(bpf_sys_data構造体 は、bpf_eventd.h 参照)

補足: 本例題では、マップbpf_sys_cntの作成と中身の設定は、受け取り側プログラムで行うことを前提としている。ローダー実行時、bpf_sys_cnt が存在することを仮定しており、mapsセクションの設定内容は適当である。

受け取り側プログラム
-----------------

src/bpf_eventd/bpf_eventd.c 参照。
