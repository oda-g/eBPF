====
eBPF
====

eBPFについて調査したドキュメントや作成したプログラムを保存するリポジトリ。

本READMEは、主にリンク(外部サイト、本リポジトリ内)を記載する。

対象システム
------------

Ubuntu 18.04 を対象にしている。カーネルのバージョンは、4.15 。

eBPFは、まだアクティブに開発が続けられており、カーネルが新しいほど、いろいろな機能が使える。
したがって、なるべく新しいカーネルを使うのが望ましいのであるが、そうは言っても、オリジナルカーネルを
持ってきて、ビルドするのは面倒なので、通常のディストリビューションの中で、一番カーネルが新しいもの
を選択した。

なお、eBPFの機能とカーネルバージョンの関係は、下記ドキュメントを参照されたい。

https://github.com/iovisor/bcc/blob/master/docs/kernel-versions.md

ビルド順
--------

いろいろなコンポーネントをビルドしており、その都度、足りないパッケージをインストールしている。
本リポジトリのドキュメントでは、ビルド例を示しているが、ビルドの順序が違うと、足りないパッケージも違ってくるので面倒である。

そこで、以下にビルドした順番を示しておく。これに合わせると、ドキュメントとの乖離が少なくて済むと思われる。

#. bpftool
#. bpfプログラムの作成と実行(1)(tc)
#. kernel解析(参考)

本リポジトリ内リンク
--------------------

* 命令セット: instruction.rst_
* bpftool: bpftool.rst_
* bpfファイルシステム: bpf_filesystem.rst_
* bpfシステムコール: bpf_syscall.rst_
* bpfプログラムの作成と実行(1)(tc): bpf_example_1_tc.rst_
* bpfプログラムの作成と実行(2)(kprobe): bpf_example_2_kprobe.rst_
* bpfプログラムの作成と実行(3)(event): bpf_example_3_event.rst_
* kernel解析(参考): kernel_analysis.rst_

.. _instruction.rst: doc/instruction.rst
.. _bpftool.rst: doc/bpftool.rst
.. _bpf_filesystem.rst: doc/bpf_filesystem.rst
.. _bpf_syscall.rst: doc/bpf_syscall.rst
.. _bpf_example_1_tc.rst: doc/bpf_example_1_tc.rst
.. _bpf_example_2_kprobe.rst: doc/bpf_example_2_kprobe.rst
.. _bpf_example_3_event.rst: doc/bpf_example_3_event.rst
.. _kernel_analysis.rst: doc/kernel_analysis.rst

参考: 各種リンク
----------------

eBPFの概要
^^^^^^^^^^

http://cilium.readthedocs.io/en/latest/bpf/

eBPFについて書かれたドキュメントはほとんどないという状況。その中で上記が唯一と言ってよいドキュメント。

https://github.com/iovisor/bcc

eBPFを使用した各種トレースツール。

後記
----

2018年にカーネルのトレース目的で、eBPFを調査、使用した。一度調べた内容をまた再度一から調べ直さなくても済むように、記録しておく目的で本リポジトリを作成した。内容については、2018年時点のものなので注意されたい。(また、今のところ、更新の予定はない)

トレース目的であれば、(分かってしまえば) bcc を使用するのが便利であると思われる。ただし、bcc は構築も面倒だし、学習コストもそれなりに高い。(個人的感想)

中身が分からないと気が済まないたちなので、プリミティブなところから調べた。なお、調査の際は、bcc も参考にさせてもらった(straceで観察するなど)。(なお、実作業では、適用の簡単さから、自ら作成したload_kp_bpf等を使用した。)
