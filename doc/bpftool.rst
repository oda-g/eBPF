bpftool
=======

カーネルのソースツリーの中にbpftoolというツールがあって、これがあると手軽に確認ができるように
なるので、ビルドする。

ビルド手順
----------

注意: 足りないパッケージについては、環境によって、異なる可能性がある。

カーネルのソースコード展開
::
  $ sudo apt-get update
  $ sudo apt-get install linux-source
  $ cd /usr/src/linux-source-4.15.0
  $ sudo tar xf linux-source-4.15.0.tar.bz2

ビルド準備
::
  $ cd linux-source-4.15.0/tools/bpf/bpftool
  $ sudo apt-get install binutils-dev libelf-dev

コード修正

binutilsのバージョンによる非互換から、コードの修正が必要である。現状のコードは、binutils-dev 2.28 まではOK。
上記でインストールしたbinutils-devは2.30。2.28がインストールできないかと思ったが、2.30しかなかった。
仕方ないので、コードを修正する。

修正するのは、jit_disasm.c である。

.. code-block:: none

  $ diff jit_disasm.c.orig jit_disasm.c 
  110c110
  <        disassemble = disassembler(bfdf);
  ---
  >        disassemble = disassembler(bfd_get_arch(bfdf), bfd_little_endian(bfdf) ? FALSE : TRUE, 0, NULL);

ビルド
::
  $ sudo make
  $ sudo make install
  $ sudo bpftool --help ##確認
  
man page もついでにビルドする。
::
  $ cd Documentaion
  $ sudo apt-get install python-docutils
  $ sudo make
  $ sudo make install
  $ man bpftool  ##確認
  
使用例
------

普通にシステムを起動しても、既に eBPFを使っている人がいるので、bpftoolで見ることができる。

補足: root権限が必要なので、常に sudo を付ける。

プログラム一覧
::
  $ sudo bpftool prog show

バイトコードを見てみる
::
  $ sudo bpftool prog dump xlated id 7
  
マップ一覧
::
  $ sudo bpftool map show

マップの中身を見てみる
::
  $ sudo bpftool map dump id 2
