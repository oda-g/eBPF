bpfプログラムの作成と実行(2)(kprobe)
=================================

ここでは、BPF_PROG_TYPE_KPROBEタイプ(以下、簡単にkprobeタイプと呼称)のbpfプログラムのロードと設定について説明する。

kprobeタイプのbpfプログラムは、設定したkprobeポイントで、bpfプログラムを実行するものである。kprobeポイントとしては、カーネル内関数のエントリポイント、リターンポイントが設定できる。(ここでは、エントリポイントのみ扱っている)

kprobeタイプのbpfプログラムのローダーは、一般的なコマンドとしては用意されていない。ここでは、自力でローダーを作成している。

ローダーの中身
------------

コードは、src/load_kprobe_bpf/load_kp_bpf.c 参照。以下、コードで何をやっているかとともにローダーの中身について説明する。

ローダーの仕様
^^^^^^^^^^^^

実行形式は、「load_kp_bpf オブジェクトファイル カーネル関数名」。

オブジェクトファイル中のbpfプログラムをロードし、指定したカーネル関数へのkprobe設定と、bpfプログラムの有効化を行う。その後、フォアグランドで待機状態となり、Ctl-C で終了するまで、bpfプログラムが有効となる。(Ctl-Cで終了時、後始末も行われる)

C言語プログラムの形式としては、mapsセクションにマップファイルの情報、progセクションにbpfプログラムを定義する。マップファイルの情報は、tcのものを踏襲した。tcと異なり、プログラムを格納するセクション名は、prog固定で、プログラムは１つしか定義できない。

オブジェクトファイルの読み込み
^^^^^^^^^^^^^^^^^^^^^^^^^^

progセクションにバイトコードが格納されているので、それを取り出す。マップを使っていなければ、それでお終いである。

マップを使っている場合は、マップをオープンして、そのファイルディスクリプタをバイトコードに埋めないといけない。その際、REL、STRTAB、SYMTABセクションを参照する。以下の手順で行う。

#. マップの参照箇所は、RELセクションを見れば分かる。RELセクションのエントリ数分ループする。
#. r_offsetがprogセクション中のオフセット。sizeof(struct bpf_insn)で割れば、instructionのindexが算出できる。
#. r_infoの中にSYMTABのインデックスが入っており(GELF_R_SYM(rel.r_info))、どのシンボルを参照しているか分かる。
#. 前記シンボル情報のst_valueにmapsセクション中のオフセットが入っている。mapsセクション中のそのオフセットから始まる、bpf_elf_map構造体を参照すればよい。また、シンボル情報のst_nameにシンボル名のSTRTABセクション中のオフセットが入っている。それでシンボル名(==マップ名)が分かる。
#. 前記マップがまだオープンされていなければ、オープンする。(参照箇所が複数あれば、既にオープンされていることもある)
#. instruction中にファイルディスクリプタを埋め込む。

コードは、load_kp_bpf.c get_bpf_obj_info 参照。

プログラムのロード
^^^^^^^^^^^^^^^^

load_kp_bpf.c load_program 参照。以下、注意事項のみ記す。

* | kern_versionに入れる値は、カーネルと合っていないといけない。
  | ubuntu 18.04 では、一番最初のカーネル(4.15.0-20-generic)では、で266001であったが、その後のupdate(ex. 4.15.0-34-generic or after)でで266002に代わっている。
  | 通常は、マクロLINUX_VERSION_CODE(linux/version.h)(266002になっている)を使用する。
  | 本コードは、4.15.0-20-genericで動作確認を行い、ヘッダと整合性が取れていなかったため、266001とハードコーディングしている。(何で、ヘッダと整合性が取れてないのかに関しては、詳細未追及）
  | 使用のカーネルに合わせ、コードを修正されたい。
* bpfプログラムにエラーがあったとき、log_bufにいろいろとメッセージが格納される。エラー発生時、log_bufの大きさが足りないと、ENOSPCのエラーとなり、エラーの解析ができないので、十分な大きさを用意すること。
* licenseは、GPL(コンパチ)でないといけない。本来は、tcの例のようにCプログラム側のセクションで指定すべきだが、本プログラムは決め打ちで入れている。

プログラムの有効化
^^^^^^^^^^^^^^^

load_kp_bpf.c set_event 参照。以下の手順で行う。

(1) kprobe イベントの設定
~~~~~~~~~~~~~~~~~~~~~~~

kprobe用eBPFでは、まず、kprobeイベントを設定する必要がある。
そのためには、/sys/kernel/debug/tracing/kprobe_events に書き込みを行う。

書き込む形式は、以下のとおり。

「p:kprobes/{event名} 関数名」

event名は、任意の(ユニークな)文字列。後で参照する。関数名は、kprobe を掛けたいカーネルの関数名。
(traceのためには、まだこの後にも定義するパラメータがあるが、eBPF用には、これで十分)


指定例: 

::

# echo "p:kprobes/test_bpf sys_bpf" >> /sys/kernel/debug/tracing/kprobe_events

注意:「>>」を使うこと。「>」を使うと定義済のものが消えてしまう。

(設定を個別に)削除したい場合は、「p」を「-」に変えて、書き込む。

::

  # echo "-:kprobes/test_bpf" >> /sys/kernel/debug/tracing/kprobe_events

(前記の注意を逆手に取って、「echo > /sys/kernel/debug/tracing/kprobe_events」 とやれば、すべての設定を削除できる。)

設定は、/sys/kernel/debug/tracing/kprobe_events を参照して確認できる。

::

  # cat /sys/kernel/debug/tracing/kprobe_events
  p:kprobes/test_bpf sys_bpf
  #

設定を行うと、/sys/kernel/debug/tracing/events/kprobes/{event名} ディレクトリが作成され、いくつかのファイルができる。
(正確には、eventsの下に設定した、kprobes/{event名} ができる。実は、kprobesの部分も任意で、ディレクトリによるグループ化ができるようになっている。)

作成されたファイルの内、後で、idファイルを参照することになる。

::

  # ls /sys/kernel/debug/tracing/events/kprobes/test_bpf
  enable  filter  format  hist  id  trigger
  # cat /sys/kernel/debug/tracing/events/kprobes/test_bpf/id
  1478
  #

(2) perf_event_open システムコールによる設定
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

次に、(1)で設定したeventに対し、有効化、および、eBPFプログラムとの関連付けを行う。

以下、コードを参照しつつ手順をコメント。

::

   282		struct perf_event_attr ev_attr = {};
   283		int efd;
   ...
   286		int id;
   ...
   302		ev_attr.config = id;  // idファイルの内容を指定
   303		ev_attr.type = PERF_TYPE_TRACEPOINT;
   304	
   305		efd = syscall(__NR_perf_event_open, &ev_attr,       // トレースの定義
   306			       -1/*pid*/, 0/*cpu*/, -1/*group_fd*/, 0);
   ...
   311		if (ioctl(efd, PERF_EVENT_IOC_ENABLE, 0) < 0) {     // トレースの有効化
   ...
   316		if (ioctl(efd, PERF_EVENT_IOC_SET_BPF, prog_fd) < 0) {  // bpfプログラムをトレースに結び付ける。
          // prog_fd は、bpfプログラムロード時に返されたファイルディスクリプタ。

動作確認例
---------

テストプログラム
^^^^^^^^^^^^^^

src/load_kprobe_bpf/test_prog.c 参照。

カーネル関数sys_bpf()にkprobeを設定することを前提。sys_bpfの第一引数がBPF_PROG_GET_FD_BY_IDの場合、マップのカウントアップをする。

::

     5	#define PT_REGS_PARM1(x) ((x)->rdi)  /* for x86_64 */
   ...
    17	__section("prog")
    18	int test_prog(struct pt_regs *ctx)
    19	{
    20		uint32_t idx, *cnt;
    21		int cmd = (int)PT_REGS_PARM1(ctx);

kprobeタイプのプログラムに渡ってくる引数は、struct pt_regs構造体ポインタである。kprobeで設定したカーネル関数が呼び出されたときのレジスタ情報が格納されている。x86_64の場合は、rdiを参照すれば、第一引数が分かる。

準備
^^^^

bpfファイルシステムは、マウントしておく。

プログラムのコンパイル。(Makefile参照)

::

  $ make
  $ make test_prog.o
  
動作確認
^^^^^^^

プログラムのロード:

::

  $ sudo ./load_kp_bpf test_prog.o sys_bpf
  Running.
  (このままフォアグランドで動作し続ける)
  
(EINVALでエラーになる場合は、前記「プログラムのロード」の章の注意事項参照。)

別の端末で、bpftool を実行。

::

  $ sudo bpftool map show
  (test_map_cnt の idを確認)
  $ sudo bpftool map dump id <id>
  (valueを確認。まだ、0)
  $ sudo bpftool prog show
  ...
  $ sudo bpftool map dump id <id>
  (valueを確認。増えている)
  (map show では、カウントは増えないが、prog show でカウントが増えることを確認)
  
プログラムの終了: load_kp_bpf実行中の端末に戻って、Ctl-C 押下。

::

  $ sudo ./load_kp_bpf test_prog.o sys_bpf
  Running.
  ^CTerminate.
  $
  
(イベントの削除は行っているが、マップの削除はしていない。マップの削除は手動で。)
