bpfプログラムの作成と実行(1)(tc)
===============================

準備
----

プログラムのコンパイルには、clang を使用するので、インストール。

::

  $ sudo apt istall clang lib6-dev-i386
  
bpfプログラムの作成と実行
----------------------

バイトコードを直接プログラムすることも可能ではあるが、通常は、C言語で作成する。
clangでbpfのオブジェクトファイルを生成することができる。生成されたプログラムをローダーと呼ばれるプログラムでカーネルにロードすることになる。
bpfプログラムは、プログラムタイプにより、いろいろと約束事があり、異なっている。

* bpfプログラムに渡ってくる引数
* bpfプログラムで使用できるカーネルヘルパー
* bpfプログラムの返り値の意味

bpfプログラムはカーネルにロードしただけでは、動作せず、ロードした後に有効化する必要があるが、その方法もプログラムタイムにより異なる。
プログラムタイプにより、専用のローダーがある(ものもある)。

* | BPF_PROG_TYPE_SOCKET_FILTER: raw socket の filter
  | 例えば、tcpdumpで使用(tcpdumpで内部的にロード)
* | BPF_PROG_TYPE_KPROBE: kprobe で定義したポイントで eBPF実行
  | bccでサポートされている。
* | BPF_PROG_SCHED_CLS: qdiscでeBPF実行
  | tcでロード
* | BPF_PROG_TYPE_XDP: XDP
  | ipでロード

ここでは、比較的取り扱い易い、tc を例にとり、プログラムの作成と実行を説明する。

bpfプログラム例
-------------

src/measure_packet/mes_pkt.c、src/measure_packet/mes_pkt.h、src/include/bpf_defs.h を参照。

プログラム概要
^^^^^^^^^^^^

パケットの2点間の転送時間を計測する。パケットの種類としては、UDPのポート5001を対象とする。

arrayタイプのマップを3つ使用している。map_mes_startは、パケットの計測スタート地点の時刻を記録するもの、map_mes_endは、パケットの計測エンド遅延の時刻を記録するもので、それぞれ、MES_DATA_SIZE(512)個分、記録できる。map_mes_cntは、map_mes_start、map_mes_endそれぞれの、次にデータを格納するインデックスを記録するもの。

以下、ポイントを説明。

::

    13	struct bpf_elf_map map_mes_start __section("maps") = {
    14		.type		= BPF_MAP_TYPE_ARRAY,
    15		.size_key	= sizeof(uint32_t),
    16		.size_value	= sizeof(uint64_t),
    17		.pinning	= PIN_TC_GLOBAL,
    18		.max_elem	= MES_DATA_SIZE,
    19	};

マップの定義。マップの定義は、mapsセクションに行うという、ローダー(この場合は、tcコマンド)との約束事になっている。
bpf_elf_map構造体は、src/include/bpf_defs.h 参照。tcコマンドに合わせて定義してある。(他のローダーも大体この形式に合わせているようだ)




