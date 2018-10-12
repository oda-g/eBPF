get_kval
========

usage: sudo ./get_kval addr

カーネルのデータを読んで、標準出力に出す(10進)。データ長は、8バイト限定。
addrは、10進か16進(この場合は、0xを付けること)。
/proc/kcore を読むため、ルート権限必要。

get_offs_real
=============

usage: sudo ./get_offs_real

tk_core.timekeeper.offs_real の値を取って来るシェルスクリプト。(標準出力に出す)
ルート権限必要。やっていることは、見ればすぐに分かるはず。
