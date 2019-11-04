カーネル解析(参考)
================

カーネルコードクロスリファレンス
----------------------------

下記、URLにカーネルのソースコードのクロスリファレンスがある。便利。

https://elixir.bootlin.com/linux/latest/source

crash
-----

ダンプ解析用ツールであるが、動作中のカーネルにも使用することができる。ばかでかい構造体や山のようなifdefで、実際に動作しているカーネルのコードや構造体がどうなっているのか、ソースから追うのは難しい。crashで動作カーネルを見るのがてっとり早くて便利である。

(注: ifdef自体は、/boot/config-<カーネルバージョン> ファイルで確認できる。)

インストール
^^^^^^^^^^

::
  $ sudo apt install crash
  
debug symbol 取得
^^^^^^^^^^^^^^^^^

カーネルのdegug symbol パッケージが必要。debug symbol パッケージの取得は、https://wiki.ubuntu.com/Debug%20Symbol%20Packages 参照
(下記手順もそれに準拠)。

::

  $ sudo tee /etc/apt/sources.list.d/ddebs.list << EOF
  > deb http://ddebs.ubuntu.com/ $(lsb_release -cs)          main restricted universe multiverse
  > deb http://ddebs.ubuntu.com/ $(lsb_release -cs)-updates  main restricted universe multiverse
  > deb http://ddebs.ubuntu.com/ $(lsb_release -cs)-proposed main restricted universe multiverse
  > EOF
  $ sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys F2EDC64DC5AEE1F6B9C621F0C8CAB6595FDFF622
  $ apt update
  $ sudo apt install linux-image-$(uname -r)-dbgsym

