bpfシステムコール
=================

実行形式
--------

::

  #define _GNU_SOURCE
  #include <sys/syscall.h>
  #include <linux/bpf.h>

  int syscall(__NR_bpf, int cmd, union bpf_attr *attr, unsigned int size);

「man 2 bpf」でも情報は出てくるが、内容がかなり古いので注意。(間違っているというわけではないが、機能の追加に追随していない。)

libcではサポートされていないので、上記のように、syscall 関数を使用する。

eBPFに関する様々な機能をこれひとつで実現している。ユーザプログラムからカーネルに対する唯一のインタフェースである。

引数
^^^^

cmdは、カーネルに対して実行を要求する機能の指定を行う。linux/bpf.h に enum で定義されており、以下の種類がある。

BPF_MAP_CREATE
  マップの作成
  
BPF_MAP_LOOKUP_ELEM
  マップの要素の取得
  
BPF_MAP_UPDATE_ELEM
  マップの要素の更新
  
BPF_MAP_DELETE_ELEM
  マップの要素の削除

BPF_MAP_GET_NEXT_KEY
  マップの次のキーの取得
  
BPF_PROG_LOAD
  プログラムのロード
  
BPF_OBJ_PIN
  オブジェクト(マップ、プログラム)をbpfファイルシステムに載せる
  
BPF_OBJ_GET
  bpfファイルシステム上のオブジェクト(マップ、プログラム)の取得
  
BPF_PROG_ATTACH
  プログラムのアタッチ

BPF_PROG_DETACH
  プログラムのデタッチ
  
BPF_PROG_TEST_RUN
  未稿
  
BPF_PROG_GET_NEXT_ID
  次のプログラムのID取得
  
BPF_MAP_GET_NEXT_ID
  次のマップのID取得
  
BPF_PROG_GET_FD_BY_ID
  IDで示すプログラムをオープンする
  
BPF_MAP_GET_FD_BY_ID
  IDで示すマップをオープンする
  
BPF_OBJ_GET_INFO_BY_FD
  ファイルディスクリプタからオブジェクト(マップ、プログラム)の情報を取得する

BPF_PROG_QUERY
  未稿

attr は、システムコールの入出力に使用される。パラメータは、コマンドごとに異なり、そのすべてをカバーしたunionで定義されている。sizeは、attr(で示される、用意した領域)の大きさである。
コマンドごとに必要な大きさは異なるが、通常、以下のように実行するため、意識することはない。

::

 union bpf_attr attr;
 int ret;

 bzero(&attr, sizeof(attr));
 attr.必要なメンバ = のみ設定;
 ret = syscall(__sys_BPF, BPF_XXX, &attr, sizeof(attr));

復帰値
^^^^^^

エラーの場合、-1が返され、errno にエラー番号が設定される。正常終了の場合、ファイルディスクリプタを返すコマンドについては、ファイルディスクリプタ(0以上の整数)が返され、それ以外のコマンドについては、0が返される。

以降の節では、各コマンドのパラメータについて、説明。（動作確認が取れたものから、追加予定）

BPF_MAP_CREATE
--------------

+--------+----------------------------+------------------+
| type   | member                     | content          |
+========+============================+==================+
| __u32  | map_type                   | マップタイプ(後述) |
+--------+----------------------------+------------------+
| __u32  | key_size                   | キーの大きさ       |
+--------+----------------------------+------------------+
| __u32  | value_size                 | 値の大きさ         |
+--------+----------------------------+------------------+
| __u32  | map_flags                  | フラグ(後述)       |
+--------+----------------------------+------------------+
| __u32  | inner_map_fd               | --               |
+--------+----------------------------+------------------+
| __u32  | numa_node                  | nama node        |
+--------+----------------------------+------------------+
| char   | map_name[BPF_OBJ_NAME_LEN] | 名前(オプション)   |
+--------+----------------------------+------------------+


BPF_MAP_LOOKUP_ELEM
-------------------

BPF_MAP_UPDATE_ELEM
-------------------

BPF_MAP_DELETE_ELEM
-------------------

BPF_MAP_GET_NEXT_KEY
--------------------

BPF_PROG_LOAD
-------------

BPF_OBJ_PIN
-----------

BPF_OBJ_GET
-----------

BPF_PROG_ATTACH
---------------

BPF_PROG_DETACH
---------------

BPF_PROG_TEST_RUN
-----------------

BPF_PROG_GET_NEXT_ID
--------------------

BPF_MAP_GET_NEXT_ID
-------------------

BPF_PROG_GET_FD_BY_ID
---------------------

BPF_MAP_GET_FD_BY_ID
--------------------

BPF_OBJ_GET_INFO_BY_FD
----------------------

BPF_PROG_QUERY
--------------
