1. 測定内容

パケットが、NIC A の ingress qdisc に入ってから、NIC B の egress qdics に入るまでの実時間を計測する。
典型的には、VMのtapデバイスから、出力用物理NICに至るまでの時間を計測するのに使用する。

計測するパケットは、UDP の destination port 5001 のものに限定されている。計測対象は、これに従うこと。
また、計測対象以外の負荷を掛けたい場合は、desitnation port を異なるものにしなければならない。

パケットサイズは関知していない。測定者がコントロールすること。

2. 制限事項

NIC A のingressに入ってから、NIC Bのegressに入るまでの間に、次のパケットが、NIC A のingress に入って来るとまずい。

そのような状況が生じているかどうかは、../mes_chk の方を使用して確認できる。また、そのような状況が生じている場合は、
../mes_chkの方を使用すること。

3. 測定準備

* bpf ファイルシステムのマウント。既にされている場合は、不要。マウントポイントは、以下に示すものにすること。
sudo mount -t bpf none /sys/fs/bpf

* tcの設定。下記例のインタフェース名(v1, v2)は、対象のものに変更すること。
sudo tc qdisc add dev v1 clsact
sudo tc filter add dev v1 ingress bpf da obj mes_pkt.o sec ingress
sudo tc qdisc add dev v2 clsact
sudo tc filter add dev v2 egress bpf da obj mes_pkt.o sec egress

補足: ビルド環境を作成するのも結構面倒なので、とりあえず、バイナリを持って行ってください。

* 設定を解除したい場合は、以下のとおり。filterは何もせず、いきなり、qdiscをdelしてよい。
  もし、前の設定(別のebpfプログラム)が残っている場合は、まず、これをすること。
sudo tc qdisc del dev v1 clsact
sudo tc qdisc del dev v2 clsact

4. 測定

* 条件にあったパケットを流せば、計測される。

* 結果の確認は、以下のコマンドを実行。
sudo ./mes_show

* 再度、測定したい場合は、以下のコマンドを実行し、マップをクリアする。(tc の再設定は不要。)
sudo ./mes_clear

補足:
tcにロード後の最初の一回目は、時間が大きくなっているかもしれない。最初に一回やって、mes_clearして、それから本測定するとよい。