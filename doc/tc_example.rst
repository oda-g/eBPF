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

プログラム説明
^^^^^^^^^^^^

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

::

    55	__section("ingress")
    56	int mes_start(struct __sk_buff *skb)
    57	{

bpfプログラムのエントリーポイント。セクション名は、任意で、tcコマンドでロードするときのパラメータとして使用される。tcコマンドの場合、ひとつのオブジェクトに複数のエントリポイント(セクション)を入れることが可能。本プログラムでは、ingressセクション(mes_start)と、egressセクション(mes_end)の2つのセクションがある。

プログラムタイプBPF_PROG_SCHED_CLSの場合、プログラムに渡される引数は、(本物の)sk_buff構造体へのポインタであるが、Cプログラム上では、linux/bpf.hに定義されている、__sk_buff構造体を参照している(下記)。

::

  /* user accessible mirror of in-kernel sk_buff.
   * new fields can only be added to the end of this structure
   */
  struct __sk_buff {
  	__u32 len;
  	__u32 pkt_type;
  	__u32 mark;
  	__u32 queue_mapping;
  	__u32 protocol;
  	__u32 vlan_present;
  	__u32 vlan_tci;
  	__u32 vlan_proto;
  	__u32 priority;
  	__u32 ingress_ifindex;
  	__u32 ifindex;
  	__u32 tc_index;
  	__u32 cb[5];
  	__u32 hash;
  	__u32 tc_classid;
  	__u32 data;
  	__u32 data_end;
  	__u32 napi_id;
   ...
   
bpfプログラムは、実際のところ、ロードしたバイトコードがそのまま実行される訳ではない。ロード時に各種チェックとともにバイトコードの変更も行われる。それぞれ、プログラムタイプによらない共通のものや、プログラムタイプ固有のものがある。

__sk_buff へのアクセスは、ロード時に sk_buff へのアクセスに変換される。

::

    37	static __inline int check_pkt(struct __sk_buff *skb)
    38	{
    39		void *data = (void *)(long)skb->data;

例えば、このコードでは、dataは、skb(これは、bpfプログラムの引数で渡ってくる値)のオフセット60(__sk_buff の data)へのアクセスとして、バイトコードが出力される。カーネルのロード時、この60という値が、本物のsk_buff構造体のdataメンバのオフセットに変更される。

__sk_buffのメンバが全部__u32であるのは、単にオフセットで、どのメンバへのアクセスかを識別しているだけだからである。気が付いたと思うが、bpfプログラムからは、__sk_buffに定義されているメンバにしかアクセスできない。


::

    37	static __inline int check_pkt(struct __sk_buff *skb)
    38	{

bpfプログラムは、関数コールをサポートしていないので、関数は、inline宣言する必要がある。(なので、関数なのは見かけ上)

その他、ループも不可なので、プログラム作成の際は、いろいろと注意する必要がある。(アドレスの前方にジャンプできない)

::

     9	static void *BPF_FUNC(map_lookup_elem, void *, void *);
    10	static int BPF_FUNC(map_update_elem, void *, void *, void *, int);
    11	static uint64_t BPF_FUNC(ktime_get_ns, void);

カーネルヘルパーを使用するための宣言。BPF_FUNCマクロは、linux/bpf.hに定義されている。
カーネルヘルパーの呼び出しは、バイトコード上は、「call ヘルパー番号」命令となる。
Cプログラム上は、下記のように、普通に関数呼び出ししているように見えるが、上記宣言では、ヘルパー番号とパラメータや復帰値の型をコンパイラに知らせる仕掛けになっている。

::

  66		cnt = map_lookup_elem(&map_mes_cnt, &idx);

ところで、カーネルのmap_lookup_elemヘルパー関数の第一引数は、(カーネルの)bpf_map構造体ポインタである。カーネル・ユーザ間のコンベンションとしては、ユーザがマップファイルをオープンし、そのファイルディスクリプタを第一引数として指定する、というものになっている。カーネルへのロード時、指定されたファイルディスクリプタからマップファイルを特定し、bpf_map構造体ポインタへの置き換えを行っている。

Cプログラムとしては、上記のようにmapsセクションに定義した、map_mes_cntへの参照となっていおり、clangが生成するオブジェクトコードでもそうなっている。からくりとしては、ローダ(tcコマンド)がマップファイルをオープンし、オブジェクトコードを変更する(マップへの参照箇所をファイルディスクリプタに置き換える)ということをやっている。ローダは、マップファイルをオープンする際にmapsセクションを参照する。

なお、パラメータは、r1～r5レジスタ(パラメータは5つまで)、返り値は、r0レジスタに格納するコンベンションになっている。

カーネルヘルパーの一覧とパラメータについては、linux/bpf.h を参照。

プログラムタイプにより、使用できるカーネルヘルパーは異なる。どれが使用できるかは、カーネルコードを参照(「struct bpf_func_proto *」 を返す関数を参照。ex. net/core/filer.c tc_cls_act_func_proto)。

::

    80		return TC_ACT_OK;

bpfプログラムの返り値の意味もプログラムタイプごとに異なる。本プログラムでは時刻の計測を行っているだけであり、パケットをそのまま通すことを意味する、TC_ACT_OKを返している。定義は、linux/pkt_cls.h。パケットに対して、何らかの処理をさせることもできる。詳しくはカーネルコード参照(ex. net/sched/act_bpf.c)。

::

   111	char __license[] __section("license") = "GPL";

tcコマンドの場合、この行が必要。なお、bpfプログラムのロード時のパラメータにlicenseの指定があり、「GPL」でないとロードできない。

プログラムコンパイル
^^^^^^^^^^^^^^^^^

clandでtargetとして、bpfを指定する。

::

  clang -O2 -Wall -target bpf -I../include -c mes_pkt.c -o mes_pkt.o
  
(Makefileも参照)

-Sでバイトコードのアセンブルリストも出せる。(mes_pkt.s ができる)

::

  clang -O2 -Wall -target bpf -I../include -c mes_pkt.c -S
  
動作確認例
---------

src/measure_packet/ の下に、お手軽に確認できるスクリプト類を置いてある。

確認環境
^^^^^^^

::

                     br1
                +--------------+
                |              |
            +--■ v1          v2 ■--+
            |   |              |   |
            |   +--------------+   |
            |                      |
       +----+----+            +----+----+
       |    ■    |            |    ■    |
       |    n1   |            |    n2   |
       +---------+            +---------+
           ns1                    ns2
           

  

