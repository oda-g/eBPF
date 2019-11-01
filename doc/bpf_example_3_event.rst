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

リングバッファの内容
^^^^^^^^^^^^^^^^^

::

                   +-----------------------------+
                   | perf_event_mmap_page 構造体  |
                   |                             |
                   |                             |
            +------+ head                        | 1ページ
            |      |                             |
            |  +---+ tail                        |
            |  |   |                             |
            |  |   +-----------------------------+
            |  |   |                             |
            |  |   |                             | リングバッファ
            |  +-->|+---------------------------+| 2**n ページ
            |      ||                           ||
            |      ||        未読データ          ||
            |      ||                           ||
            +----->|+---------------------------+|
                   |                             |
                   |                             |
                   |                             |
                   +-----------------------------+

mmapした領域は上図のようになっている。最初の1ページは、perf_event_mmap_page 構造体で各種管理データがあるが、今回使っているのは、head と tail だけである。

head は、カーネルが次にイベントデータを書く場所をポイントしており、更新はカーネルが行う。

tail は、ユーザプログラムが読んだ最後の場所をポイントしており、更新はユーザプログラムが行う。未読データがない場合は、head == tail である。

poll で wakeup された後、tail と head の差分の間を読み込み、tail を更新する。

::

    26	struct perf_event_sample {
    27	        struct perf_event_header header;
    28	        uint32_t pid, tid;
    29	        uint64_t time;
    30	};

今回の例では、イベントレコードは上記の内容になっている。イベントレコードの詳細については、linux/perf_event.h の「enum perf_event_type」の注釈を参照。「ev_attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME;」で指定したメンバが、前から詰まって格納される。

補足: コード中の「e_hdr->type == PERF_RECORD_LOST」は、未読データでリングバッファが一杯になった(tailがheadに追いついた)場合、レコードはロストするが、そのロストした数を記録しており、場所が空いた際にstruct perf_event_lost の形式でレコードが格納される。

補足: 本コードでは簡単に動作確認するため、printfで情報表示しているが、通常、イベントのデータは多くなりがちで、標準出力への出力はオーバヘッドが大きいので、ファイルにバイナリで出力することになろう。
その際の注意点であるが、ひとつひとつのイベントレコードは、キャッシュラインの倍数に合わせられるため、思った大きさと異なることに注意。実際の大きさは、perf_event_headerのsizeメンバを確認する必要がある。(レコードサイズ != sizeof(struct perf_event_sample) かもしれない)

補足: リングバッファのアクセスには、**取り扱い注意** な事項があるが、それに関しては、本資料の最後にメモを記す。

動作確認
^^^^^^^

コンテキストスイッチを扱っているので、データが大量になりがちである。観察対象のCPUが、cs_eventdの実行CPUであったり、操作端末(sshd)の実行CPUであったりすると、マッチポンプになってしまうので注意。

落ち着いて観察するためには、isolcpusで、cs_eventdの実行CPUと観察CPUを避けておくとよい。(kernel起動時パラメータで、例えば、isolcpus=2,3と指定すれば、CPU2,3が通常のプロセスのスケジュール対象から外れる。(kernelの再起動必要))

::

  ### cs_eventd ビルド。Makefile参照 ###
  $ make cs_eventd
  ### cs_eventdをCPU2で実行。CPU3を観察。
  $ sudo taskset 4 ./cs_eventd 3
  Running.
  ### フォアグランドで動作(名前はデーモン風であるが)。イベントが取れれば端末に出力する。終了は、Ctl-C押下。
  
  ### 別の端末で ###
  ### CPU3 でコマンド実行 ###
  $ sudo taskset 8 適当なコマンド ### 例えば、「printf("%d\n", get_pid());」を実行するプログラムだと、観察が容易 ###

bpfプログラムのイベント取得
------------------------

src/bpf_eventd/bpf_eventd.c 参照。

ユーザプログラム側の処理は、cs_eventdと基本的に同様。固有の事情は以下のとおり。

* bpfプログラムから、perf_event ファイルディスクリプタが分かるように、BPF_MAP_TYPE_PERF_EVENT_ARRAYタイプのマップにファイルディスクリプタを格納する。
* BPF_MAP_TYPE_PERF_EVENT_ARRAYは、エントリ数がCPU数、キーがCPU番号、値がファイルディスクリプタの形式のマップである。
* イベント採取は、CPUごとに行われ、リングバッファもCPUごとに用意する必要がある。

::

   154		map_fd = map_open(nr_cpu);

BPF_MAP_TYPE_PERF_EVENT_ARRAYタイプのマップ(名前は、bpf_sys_cnt)のオープン。詳細は、map_open()のコード参照。

::

   159		bzero(pfd, sizeof(pfd));
   160		bzero(&ev_attr, sizeof(ev_attr));
   161		ev_attr.sample_type = PERF_SAMPLE_RAW;
   162		ev_attr.type = PERF_TYPE_SOFTWARE;
   163		ev_attr.config = PERF_COUNT_SW_BPF_OUTPUT;
   164		ev_attr.sample_period = 1;
   165		ev_attr.wakeup_events = 1;

cs_eventdとは、sample_typeとconfigが異なる。sample_typeのPERF_SAMPLE_RAWは、データの中身の形式がユーザ定義であることを示す。configの指定は、イベントがbpfプログラムからのものであることを示す。

::

   166		for (i = 0; i < nr_cpu; i++) {
   167			int fd;
   168			union bpf_attr attr;
   169			void *base;
   170	
   171			fd = syscall(__NR_perf_event_open, &ev_attr, -1, i, -1,
   172					PERF_FLAG_FD_CLOEXEC);
   ...
   179			bzero(&attr, sizeof(attr));
   180			attr.map_fd = map_fd;
   181			attr.key = (unsigned long)&i;
   182			attr.value = (unsigned long)&fd;
   183			if (bpf_sys(BPF_MAP_UPDATE_ELEM, &attr) < 0) {
   ...
   189			if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
   ...
   195			base = mmap(NULL, PAGE_SIZE * (NR_DATA_PAGE + 1),
   196				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   ...
   203			pfd[i].fd = fd;
   204			pfd[i].events = POLLIN;
   205			perf_header[i] = base;
   206		}

CPUごとにperf_event_open(171行)、ioctl(PERF_EVENT_IOC_ENABLE)(189行)、mmap(195行)を行う。また、ファイルディスクリプタをマップに格納する(183行)。


::

   214		for (;;) {
   215			if (poll(pfd, nr_cpu, -1) == -1) {
   216				perror("poll");
   217				return 1;
   218			}
   219			for (i = 0; i < nr_cpu; i++) {
   220				if (pfd[i].revents | POLLIN) {
   221					output_event(perf_header[i]);
   222				} else if (pfd[i].revents != 0) {
   223					fprintf(stderr, "poll(%d) error revents(%d)\n",
   224							i, pfd[i].revents);
   225					return 1;
   226				}
   227			}
   228		}

pollのところは、pfdの数が増えているだけで、cs_eventdと同様。

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
* BPF_MAP_TYPE_PERF_EVENT_ARRAYタイプのマップへの参照。
* CPU番号(==マップのインデックス)。上記のBPF_F_CURRENT_CPUは、本bpfプログラムを実行したCPUを意味する。
* 出力データのアドレス(void *)。
* 出力データの大きさ。出力データの中身は任意である。

上記プログラムでは、sys_bpf関数にkprobeを設定することを前提としており、イベントデータとしては、sys_bpfのコマンド、実行CPU、実行時刻を出力するようにしている。(bpf_sys_data構造体 は、bpf_eventd.h 参照)

補足: 本例題では、マップbpf_sys_cntの作成と中身の設定は、受け取り側プログラムで行うことを前提としている。ローダー実行時、bpf_sys_cnt が存在することを仮定しており、mapsセクションの設定内容は適当である。

動作確認
-------

::

  ### ビルド。Makefile参照 ###
  $ make bpf_eventd
  $ make test_prog.o
  ### bpf_eventd 実行 ###
  $ sudo ./bpf_eventd
  Running.
  ### フォアグランドで動作(名前はデーモン風であるが)。イベントが取れれば端末に出力する。終了は、Ctl-C押下。
  
  ### 別の端末で ###
  ### bpfプログラムのロード ###
  $ sudo ../load_kprobe_bpf/load_kp_bpf test_prog.o sys_bpf
  Running.
  ### フォアグランドで動作。終了は、Ctl-C押下。
  
  ### 別の端末で ###
  ### bpftool を適当に実行し、観察 ###
  $ sudo bpftool map show
  $ sudo taskset 4 bpftool prog show
  ...
  
  終了は、load_kp_bpf、bpf_eventd の順で。
  
補足: リングバッファのアクセスについて
---------------------------------

cs_eventd.c、bpf_eventd.c の output_event()に関する注意事項。(以下、コードは、bpf_eventd.c のものを参照)

本関数は、カーネルコードの samples/bpf/trace_output_user.c perf_event_read()を参考に作成した。(大体、中身は同じ)

::

    41		volatile struct perf_event_mmap_page *header = map_base;

perf_event_mmap_page構造体(リングバッファの先頭にある)は、プログラム以外の人(カーネル)もデータを更新するため、volatile宣言が必要である。
本プログラム中でも、map_baseやdata_headは、プログラム中では更新していないので、油断は大敵である。

::

    48		asm volatile("" ::: "memory"); /* smp_rmb() for x86_64 */
   ...
    89		__sync_synchronize(); /* smp_mb() */

メモリバリア。

メモリバリアに関しては、カーネルコードの doc/Documentation/memory-barriers.txt に詳しい。( https://www.kernel.org/doc/Documentation/memory-barriers.txt )

メモリバリアについては、カーネル側と対で考える必要がある。カーネル側に関しては、kernel/events/ring_buffer.c perf_output_put_handle()のコメントに関連する記述がある。

::


	/*
	 * Since the mmap() consumer (userspace) can run on a different CPU:
	 *
	 *   kernel				user
	 *
	 *   if (LOAD ->data_tail) {		LOAD ->data_head
	 *			(A)		smp_rmb()	(C)
	 *	STORE $data			LOAD $data
	 *	smp_wmb()	(B)		smp_mb()	(D)
	 *	STORE ->data_head		STORE ->data_tail
	 *   }
	 *
	 * Where A pairs with D, and B pairs with C.
	 *
	 * In our case (A) is a control dependency that separates the load of
	 * the ->data_tail and the stores of $data. In case ->data_tail
	 * indicates there is no room in the buffer to store $data we do not.
	 *
	 * D needs to be a full barrier since it separates the data READ
	 * from the tail WRITE.
	 *
	 * For B a WMB is sufficient since it separates two WRITEs, and for C
	 * an RMB is sufficient since it separates two READs.
	 *
	 * See perf_output_begin().
	 */
    
48行目の意味としては、data_head とデータの順序を保証するためのもの。カーネル側としては、データを書いた後にdata_headを書いている(それを保証するために、カーネル側もsmb_wmb()が必要)が、ユーザ側が更新後のdata_headを読んだ後、更新前のデータを読んでしまう、という逆転現象を防ぐ目的。

48行目は、カーネル内のx86_64版smb_rmb()の実装と同じである。これは、単にコンパイラによる順序変更を防ぐためだけのものである。(これから判断するに、x86_64アーキテクチャとしては、ハード的には順序が保証されている。)

89行目の意味としては、(for文内で読もうとしていた)データを読む前にdata_tailの更新(という逆転現象)を防ぐ目的。(data_tailを更新すると、カーネルが読もうとしていたデータを書きつぶす可能性がある)
readとwriteの組み合わせなので、smb_mb()が必要。

__sync_synchronize()は、コンパイラが用意している関数で、アセンブルリストで確認したところmfence命令の実行であった(なお、カーネル内のsmb_mb()の実装は、lockプレフィックス＋addl命令で実装されていた)。

