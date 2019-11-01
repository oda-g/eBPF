bpfプログラムの作成と実行(3)(event)
================================

これまでの例題では、(arrayタイプの)マップに情報を格納したが、トレース(イベント)ログのような大量の情報の格納には向いていない。
(基本的に、マップは、ユーザ・カーネル間での情報交換に使用すると考えた方がよい。）

kprobeタイプのbpfプログラムは、トレース用途での使用が多いと考えられる。ここでは、bpfプログラムからイベントログを出力して、それをユーザプログラムで受け取るための仕組みについて、説明する。

コードは、src/bpf_eventd/ の下にある。

性能イベントの取得
----------------

linux kernel では、ハードウェア、ソフトウェア各種の性能イベントを採取する仕組みが用意されており、perf_event_open(2) システムコールを使って、その採取を指定する。詳細は、「man perf_event_open」参照。

bpfプログラムからデータを出力し、それを採取するのもその仕組みを使っている。ここでは、まず、より簡単な例で、性能イベントの採取の仕方を説明する。

コンテキストスイッチの情報取得
^^^^^^^^^^^^^^^^^^^^^^^^^^

src/bpf_eventd/cs_eventd.c 参照。コードに沿って、手順を説明する。

::

   121		bzero(&ev_attr, sizeof(ev_attr));
   122		ev_attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME;
   123		ev_attr.type = PERF_TYPE_SOFTWARE;
   124		ev_attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
   125		ev_attr.sample_period = 1;
   126		ev_attr.wakeup_events = 1;
   127	
   128		fd = syscall(__NR_perf_event_open, &ev_attr, -1, cpu, -1,
   129				PERF_FLAG_FD_CLOEXEC);

ここでは、ソフトウェアイベント(ev_attr.type = PERF_TYPE_SOFTWARE) の中の、コンテキストスイッチイベント(ev_attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES) を採取することを指定している。
また、イベント発生時に pid/tid と時刻を取ることを指定している。(ev_attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME)

perf_event_open の第3引数(cpu)で、イベントを取るCPUの指定をしている。cs_eventdの引数で指定したCPUのイベントを採取する。

::

   135		if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {

perf_event_open で返ってきた fd に対して、ioctl で PERF_EVENT_IOC_ENABLE を指定することにより、イベントの取得が始まる。(まだ、バッファの定義をしていないので、実質何も出ないが)

::

   140		perf_header = mmap(NULL, PAGE_SIZE * (NR_DATA_PAGE + 1),
   141				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

perf_event_open で返ってきた fd に対して、mmap を実行することにより、イベントを記録するためのリングバッファの確保が行われる仕組みになっている。この領域は、カーネルと本プログラムの両方で参照される。
mmapで指定するサイズは、「2のべき乗＋1」×ページサイズでなければならない。

::

   154		pfd.fd = fd;
   155		pfd.events = POLLIN;
   156		for (;;) {
   157			if (poll(&pfd, 1, -1) == -1) {
   158				perror("poll");
   159				return 1;
   160			}
   161			if (pfd.revents | POLLIN) {
   162				output_event(perf_header);
   163			} else if (pfd.revents != 0) {
   164				fprintf(stderr, "poll error revents(%d)\n",
   165						pfd.revents);
   166				return 1;
   167			}
   168		}

perf_event_open で返ってきた fd に対して、poll を実行することで、カーネルがイベントを書き込んだことを検知できる。(「ev_attr.wakeup_events = 1;」で毎イベント通知するようにしている。)




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
