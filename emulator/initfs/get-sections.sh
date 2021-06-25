#!/bin/sh
text=$(cat /sys/module/gatedefender/sections/.text)
bss=$(cat /sys/module/gatedefender/sections/.bss)
data=$(cat /sys/module/gatedefender/sections/.data)
echo "text: ""$text"
echo "bss: ""$bss"
echo "data: ""$data"
echo "add-symbol-file gatedefender.ko $text -s .data $data -s .bss $bss"
